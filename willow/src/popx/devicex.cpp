#include <algorithm>
#include <cctype>
#include <poplin/codelets.hpp>
#include <popnn/codelets.hpp>
#include <popops/ElementWise.hpp>
#include <popops/codelets.hpp>
#include <poputil/exceptions.hpp>
#include <poponnx/devicemanager.hpp>
#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/logging.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op.hpp>
#include <poponnx/popx/convoptionsx.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/devicexmanager.hpp>
#include <poponnx/popx/opx.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/pritask.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensordata.hpp>

namespace poponnx {
namespace popx {

void Devicex::weightsToHost(
    const std::map<TensorId, MutableVoidData> &onnxModelData) {

  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing weights to host");
    // write weights from IPU to host stream memory points

    pEngine->run(PopPrograms::ProgramIndex::WEIGHTSTOHOST);

    logging::devicex::debug("Writing weights to ONNX ModelProto");
    // copy from the host stream memory points to the
    // addresses on onnxModelData
    for (auto initId : ir().getTensors().getInitIds()) {
      auto found = onnxModelData.find(initId);
      if (found == onnxModelData.end()) {
        throw error("No TensorId " + initId + " in final host destination map");
      }
      MutableVoidData mv_data = found->second;
      hostStreamToHost(mv_data, initId);
    }
  }
}

poplar::Tensor Devicex::getConst(const poplar::Type &type,
                                 const std::vector<size_t> &shape,
                                 double val) {
  auto tensor = graph().addConstant(type, shape, val);
  return tensor;
}

PopTensors::PopTensors(const Ir &ir_) : ir(ir_) {}

void PopTensors::insert(TensorId id, const poplar::Tensor &pt) {
  auto found = tensors_.find(id);
  if (found != tensors_.end()) {
    throw error("ILE: poplar::Tensor " + id + " already in map");
  }

  if (!ir.getTensors().contains(id)) {
    throw error("ILE: no tensor named " + id +
                " in ir, is this a valid poplar::Tensor?");
  }

  // confirm shapes agree (up to squeezing out the extra 1s)
  auto expectedShape = ir.getTensors().get(id)->info.shape_szt();

  if (squeeze(pt.shape()) != squeeze(expectedShape)) {
    std::stringstream ss;
    ss << "poplar::Tensor " << id << " of unexpected shape. "
       << "Poplar tensor shape: ";
    appendSequence(ss, pt.shape());
    ss << ". Expected (Ir) tensor shape: ";
    appendSequence(ss, expectedShape);
    throw error(ss.str());
  }

  tensors_[id] = pt;
}

const poplar::Tensor &PopTensors::get(TensorId id) const {
  auto found = tensors_.find(id);
  if (found == tensors_.end()) {
    throw error("no poplar::Tensor " + id);
  }
  return found->second;
}

PopPrograms::PopPrograms(const int repeatCount_) : repeatCount(repeatCount_) {
  if (repeatCount_ <= 0) {
    throw error("Program repeat count must be greater than zero");
  }
}

poplar::program::Sequence &PopPrograms::weightsFromHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::WEIGHTSFROMHOST)];
}

poplar::program::Sequence &PopPrograms::optimizerFromHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::OPTIMIZERFROMHOST)];
}

poplar::program::Sequence &PopPrograms::forwardFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::FORWARD)];
}

poplar::program::Sequence &PopPrograms::lossFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::LOSS)];
}

poplar::program::Sequence &PopPrograms::backwardFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::BACKWARD)];
}

poplar::program::Sequence &PopPrograms::weightsToHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::WEIGHTSTOHOST)];
}

poplar::program::Sequence PopPrograms::weightsFromHost() {
  return weightsFromHostFragment();
}

poplar::program::Sequence PopPrograms::optimizerFromHost() {
  return optimizerFromHostFragment();
}

poplar::program::Repeat PopPrograms::infer() {
  return poplar::program::Repeat(repeatCount, forwardFragment());
}

poplar::program::Repeat PopPrograms::evaluate() {
  poplar::program::Sequence eval;

  eval.add(forwardFragment());
  eval.add(lossFragment());

  return poplar::program::Repeat(repeatCount, eval);
}

poplar::program::Repeat PopPrograms::train() {
  poplar::program::Sequence trn;

  trn.add(forwardFragment());
  trn.add(lossFragment());
  trn.add(backwardFragment());

  return poplar::program::Repeat(repeatCount, trn);
}

poplar::program::Sequence PopPrograms::weightsToHost() {
  return weightsToHostFragment();
}

std::vector<poplar::program::Program> PopPrograms::progs() {
  std::vector<poplar::program::Program> ps(ProgramIndex::N);

  ps[ProgramIndex::WEIGHTSFROMHOST]   = weightsFromHost();
  ps[ProgramIndex::OPTIMIZERFROMHOST] = optimizerFromHost();
  ps[ProgramIndex::INFER]             = infer();
  ps[ProgramIndex::EVALUATE]          = evaluate();
  ps[ProgramIndex::TRAIN]             = train();
  ps[ProgramIndex::WEIGHTSTOHOST]     = weightsToHost();

  return ps;
}

poplar::program::Sequence &
PopPrograms::programFragment(PopPrograms::ProgramFragmentIndex index) {
  return seqs[static_cast<int>(index)];
}

poplar::Graph &Devicex::graph() { return *pGraph; }

Devicex::Devicex(const Ir &ir, DeviceInfo &deviceInfo)
    : poponnx::Device(ir),
      progs(PopPrograms(ir.getDataFlow().batchesPerStep())), tensors(ir) {

  // do not like the dynamic cast, is there a better way....
  popDevice = dynamic_cast<DevicexInfo &>(deviceInfo).getDevice();

  if (!popDevice.attach()) {
    throw error("failed to attach to popDevice");
  }

  // TODO (see T5100) : if inference, forward should be INFERENCE_FWD
  for (auto it : ir.getSessionOptions().convolutionOptions) {
    fwdConvOptions.options[it.first] = it.second;
    bwdConvOptions.options[it.first] = it.second;
    wuConvOptions.options[it.first]  = it.second;
  }

  fwdConvOptions.options["pass"] = "TRAINING_FWD";
  bwdConvOptions.options["pass"] = "TRAINING_BWD";
  wuConvOptions.options["pass"]  = "TRAINING_WU";

  // Not sure what these options should be
  fwdMmOptions.set("fullyConnectedPass", "TRAINING_FWD");
  bwdMmLhsOptions.set("fullyConnectedPass", "TRAINING_BWD");
  bwdMmRhsOptions.set("fullyConnectedPass", "TRAINING_WU");

  engineOptions.set("target.workerStackSizeInBytes", "0x200");
  for (auto it : ir.getSessionOptions().engineOptions) {
    engineOptions.set(it.first, it.second);
  }

  for (auto it : ir.getSessionOptions().reportOptions) {
    reportOptions.set(it.first, it.second);
  }
}

void Devicex::weightsFromHost() {
  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing weights from host, ");
    pEngine->run(PopPrograms::ProgramIndex::WEIGHTSFROMHOST);
    logging::devicex::debug("done.");
  }
}

void Devicex::optimizerFromHost() {
  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing optimizer from host, ");
    pEngine->run(PopPrograms::ProgramIndex::OPTIMIZERFROMHOST);
    logging::devicex::debug("done.");
  }
}

void Devicex::hostToHostStream(
    void *dst,                 // destination of copy (a step tensor)
    const void *src,           // source of copy
    const TensorInfo &dstInfo, // the info for dst
    const TensorInfo &srcInfo, // user provided info for src
    TensorId id // for clear error message, we need the id of the tensor
) {

  // confirm that the shapes of dst and src agree
  if (dstInfo.shape() != srcInfo.shape()) {
    std::stringstream ss;
    ss << "Shape discrepency for tensor " << id
       << ",\nStep tensor info (user) : ";
    srcInfo.append(ss);
    ss << "\nStep tensor info (expected) : ";
    dstInfo.append(ss);
    ss << ",\nBatches per step : " << ir().getDataFlow().batchesPerStep()
       << '.';
    throw error(ss.str());
  }

  auto srcType = srcInfo.dataType();
  auto dstType = dstInfo.dataType();

  // check type compatibility
  if (srcType == dstType) {
    // copy the full step data from src to dst
    std::memcpy(dst, src, srcInfo.nbytes());
  }

  else if (srcType == DataType::INT64 && dstType == DataType::INT32) {
    auto dst_int32 = static_cast<int *>(dst);
    auto src_int64 = static_cast<const int64_t *>(src);
    for (auto i = 0; i < dstInfo.nelms(); ++i) {
      dst_int32[i] = static_cast<int>(src_int64[i]);
    }
  }
  // add more custom copies here. Design decision: don't
  // just blindly cast, if the user provides an int
  // tensor when a float tensor is expected they might
  // have made a mistake.

  else {
    std::stringstream ss;
    ss << "Type disrcepency for tensor " << id
       << ". User provided : " << srcInfo.data_type()
       << " and expected : " << dstInfo.data_type()
       << ". Consider a custom copy here (as memcpy cannot be used)";
    throw error(ss.str());
  }
}

// Copy from the host end of a d2h stream, to some final host memory.
// This is the step which follows a copy from device to host.
// poplar::Streams cannot write to an arbitrary dynamic address,
// they are connected to a fixed host address. This function copies
// from that fixed address to a dynamic address (mv_data).
void Devicex::hostStreamToHost(const MutableVoidData &mv_data, TensorId id) {

  // The host end of the poplar::Stream,
  // we will try to copy from here
  auto src = static_cast<const void *>(d2hBuffers.at(id).data());

  auto dst = mv_data.data;

  // size of the host end of the poplar stream.
  // It is a char vector, so this is in bytes.
  int64_t nbytes_src = d2hBuffers.at(id).size();

  // number of bytes of the destination.
  int64_t nbytes_dst = mv_data.info.nbytes();

  // We confirm that the sizes of src and dst are the same
  if (nbytes_src != nbytes_dst) {
    std::stringstream errms;
    errms << "sizes (in bytes) of src (" << nbytes_src << ") and dst ("
          << nbytes_dst << ") differ in hostStreamToHost";
    throw error(errms.str());
  }

  std::memcpy(dst, src, nbytes_src);
}

void Devicex::anchorsHostToHostStreams(const IStepIO &stepio) {

  if (useSyntheticData() == false) {
    std::string prefix = "     ";
    logging::devicex::debug(prefix + "Copying to h2d stream address(es) ");
    for (Tensor *tensor : ir().dataStreamTensors()) {
      ConstVoidData stepin = stepio.in(tensor->id);

      // where to write to on host,
      auto dst = static_cast<void *>(h2dBuffers.at(tensor->id).data());
      // where to read from on host,
      auto src = stepin.data;

      // we calculate the TensorInfo for dst. If batchesPerStep() = 1, then
      // it has the same dimensions as tensor->info. Otherwise it has has
      // an extra dimension of size batchesPerStep() to accommmodate all
      // step anchor tensors.
      auto stepDstShape = tensor->info.shape();
      if (ir().getDataFlow().batchesPerStep() > 1) {
        stepDstShape.insert(stepDstShape.begin(),
                            ir().getDataFlow().batchesPerStep());
      }
      TensorInfo dstInfo{tensor->info.dataType(), stepDstShape};

      // the info of the user provided src step tensor
      TensorInfo srcInfo = stepin.info;

      hostToHostStream(dst, src, dstInfo, srcInfo, tensor->id);
    }
  }
}

void Devicex::anchorsHostFromHostStreams(const IStepIO &stepio) {

  if (useSyntheticData() == false) {
    std::string prefix = "     ";
    logging::devicex::debug(prefix + "Copying from d2h stream address(es) ");
    for (TensorId anchorId : ir().getDataFlow().anchors()) {
      MutableVoidData stepout = stepio.out(anchorId);
      hostStreamToHost(stepout, anchorId);
    }
  }
}

void Devicex::infer(const IStepIO &stepio) {
  std::string prefix = "     ";
  logging::debug("Performing one inference step: ");
  anchorsHostToHostStreams(stepio);

  logging::debug(prefix + "Running the inference program ");
  pEngine->run(PopPrograms::ProgramIndex::INFER);

  anchorsHostFromHostStreams(stepio);
}

void Devicex::evaluate(const IStepIO &stepio) {
  std::string prefix = "     ";
  logging::debug("Performing one evaluate step: ");
  anchorsHostToHostStreams(stepio);

  logging::debug(prefix + "Running the evaluate program ");
  pEngine->run(PopPrograms::ProgramIndex::EVALUATE);

  anchorsHostFromHostStreams(stepio);
}

void Devicex::train(const IStepIO &stepio) {
  std::string prefix = "     ";
  logging::debug("Performing one train step: ");
  anchorsHostToHostStreams(stepio);

  logging::debug(prefix + "Running the train program ");
  pEngine->run(PopPrograms::ProgramIndex::TRAIN);

  anchorsHostFromHostStreams(stepio);
}

std::unique_ptr<Opx> Devicex::createOpx(Op *op) {

  auto opx = OpxManager::createOpx(op, this);

  if (!opx) {
    if (op->opid == Onnx::Operators::Constant) {
      throw error("ILE: No Opx for CONSTANT");
    } else {
      throw error("Could not create opx for '{}'", op->opid);
    }
  }

  return opx;
}

Opx *Devicex::getOpx(OpId id) { return opxs.at(id).get(); }

TaskId Devicex::taskWhichCreates(TensorId id) const {
  Tensor *tensor = ir().getTensors().get(id);
  // streamed and init tensors are created with
  // tasks with names from initTensorTaskId
  // These tensors are recognisable as having no producing Op.
  if (tensor->hasProducer() == false) {
    return initTensorTaskId(id);
  }

  else {
    return opTaskId(tensor->getProducer());
  }
}

// Design decision : leave the option for a Tensor to be
// created based on complex global criteria open.
PriTask Devicex::initTensorTask(Tensor *tensor) {

  auto errorbase = [&tensor]() {
    std::stringstream ss;
    ss << "Failed to add tensor " << tensor->id << '.';
    tensor->consumers.append(ss);
    return ss.str();
  };

  // Do any of the consumers know how to create a poplar::Tensor?
  // If so, collect those that do, and the index at which consumed.
  // Note that an Opx may appear several times, with different
  // consumption indices.
  std::vector<OpxAndInIndex> candidates;
  for (Op *op : tensor->consumers.getOps()) {
    for (int index : op->input->indices(tensor)) {
      auto conOpId = op->id;
      Opx *opx     = getOpx(conOpId);
      if (opx->canCreateInput(index)) {
        candidates.push_back({index, opx});
      }
    }
  }

  if (candidates.size() > 1) {
    // check that all creators are in agreement on how
    // to create the poplar::Tensor. If they are, just keep
    // the first one.
    bool allEquivalent = true;
    auto cand0         = candidates[0];
    for (int i = 1; i < candidates.size(); ++i) {
      auto cand1 = candidates[i];
      if (!cand0.opx->createsEquiv(cand0.index, cand1.opx, cand1.index)) {
        allEquivalent = false;
        break;
      }
    }

    // they're all equivalent, select the first candidate as the creator
    if (allEquivalent) {
      candidates.resize(1);
    }
  }

  // a unique candidate creator will create the tensor
  if (candidates.size() == 1) {
    Opx *creator = candidates[0].opx;
    int inIndex  = candidates[0].index;
    auto f       = [this, creator, inIndex, tensor]() {
      logging::devicex::debug("Creating poplar::Tensor " + tensor->id);
      tensors.insert(tensor->id, creator->createInput(inIndex));
    };
    // the inputs of creator which must have poplar::Tensors
    // before creator creates input tensor at index inIndex.
    std::vector<TaskId> deps;
    for (TensorId tenId : creator->mustExistBeforeCreate(inIndex)) {
      TaskId dep = taskWhichCreates(tenId);
      deps.push_back(dep);
    }

    // Discussion with David Norman suggests creating tensors as
    // late as possible gives better IPU memory use, so
    // giving this low priority.
    return {-1e6,
            initTensorTaskId(tensor->id), // the task name
            deps,
            f};
  }

  else if (candidates.size() > 1) {
    throw error(errorbase() + "\nConflicting creator candidates.");
  }

  else {

    auto f = [this, tensor]() {
      std::stringstream ss;
      ss << "Creating " << tensor->id << " linearly. "
         << "WARNING :  "
         << "No creator candidates. We should perform a "
         << "depth search to find a candidate. " << std::endl;
      logging::devicex::warn(ss.str());

      auto newTensor = graph().addVariable(
          popType(tensor->info), tensor->info.shape_szt(), tensor->id);
      poputil::mapTensorLinearly(graph(), newTensor);
      tensors.insert(tensor->id, newTensor);
    };

    return {1e6, initTensorTaskId(tensor->id), {}, f};
  }
}

PriTask Devicex::streamFromHostTask(Tensor *tensor) {
  auto f = [this, tensor]() {
    logging::devicex::debug("Creating host-to-device FIFO " + tensor->id);
    fromHostStreams.emplace(tensor->id,
                            graph().addHostToDeviceFIFO(h2dId(tensor->id),
                                                        popType(tensor->info),
                                                        tensor->info.nelms()));
  };

  return {
      0,                                // priority unimportant
      streamFromHostTaskId(tensor->id), // name of this task
      {initTensorTaskId(tensor->id)},   // poplar::Tensor must exist
      f                                 // what to run when the task is executed
  };
}

PriTask Devicex::streamToHostTask(Tensor *tensor) {
  auto f = [this, tensor]() {
    logging::devicex::debug("Creating device-to-host FIFO " + tensor->id);
    toHostStreams.emplace(tensor->id,
                          graph().addDeviceToHostFIFO(d2hId(tensor->id),
                                                      popType(tensor->info),
                                                      tensor->info.nelms()));
  };

  return {
      0,                              // priority unimportant
      streamToHostTaskId(tensor->id), // name of this task
      {taskWhichCreates(tensor->id)}, // poplar::Tensor must exist
      f                               // what to run when the task is executed
  };
}

PopPrograms::ProgramFragmentIndex
Devicex::programFragmentIndex(Vertex *vertex) {
  switch (vertex->getPhase()) {
  case Phase::BWD: {
    return PopPrograms::ProgramFragmentIndex::BACKWARD;
  }
  case Phase::LOSS: {
    return PopPrograms::ProgramFragmentIndex::LOSS;
  }
  case Phase::FWD: {
    return PopPrograms::ProgramFragmentIndex::FORWARD;
  }
  case Phase::UNDEFINED: {
    throw error("Failed to determine fragment of vertex " + vertex->str() +
                " from UNDEFINED phase. ");
  }
  default: { throw error("Failed to determine fragment of vertex"); }
  }
}

poplar::program::Sequence &Devicex::programFragment(Vertex *vertex) {
  return progs.programFragment(programFragmentIndex(vertex));
}

PriTask Devicex::opTask(Op *op, double priority) {

  OpId id  = op->id;
  Opx *opx = getOpx(id);

  // although priority should guarantee that this
  // task is only run after inputs are all created,
  // we add a dependency to the input tensors, just
  // in case someone plays with the priorities.
  // Moreover, we must state the copy-from-host deps
  std::vector<TaskId> deps;
  for (auto t_inds : op->input->indicesMap()) {
    Tensor *tensor = t_inds.first;

    auto creatorTask = taskWhichCreates(tensor->id);
    // Make sure we only add the creatorTask once in the dependency list
    if (std::find(deps.begin(), deps.end(), creatorTask) == deps.end())
      deps.push_back(creatorTask);

    // if the tensor is streamed on, we must wait
    // 'til the Copy has happened
    if (tensor->tensorType() == TensorType::Stream) {
      if (useSyntheticData() == false)
        deps.push_back(fromHostTaskId(tensor->id));
    }
  }

  auto f = [op, opx, this]() {
    logging::devicex::debug("Creating output tensors for " + opx->op_p->str());
    opx->grow(programFragment(op));
  };
  return {priority, opTaskId(op), deps, f};
}

OpxAndInIndex::OpxAndInIndex(int conIndex_, Opx *opx_)
    : index(conIndex_), opx(opx_) {}

// go all the way to creating the engine and connecting streams
void Devicex::prepare() {

  pGraph.reset(new poplar::Graph(popDevice));
  popops::addCodelets(graph());
  poplin::addCodelets(graph());
  popnn::addCodelets(graph());

  // create an Opx for every Op
  for (Op *op : ir().getOpSchedule({})) {
    opxs[op->id] = createOpx(op);
  }

  PriTasks tasks;

  // initializers :
  // 1) make tensor,
  // 2) make stream from host,
  // 3) create write prog,
  // 4) make stream to host,
  // 5) create read prog.
  for (auto id : ir().getTensors().getInitIds()) {
    Tensor *tensor = ir().getTensors().get(id);
    // 1
    tasks.add(initTensorTask(tensor));

    if (useSyntheticData() == false) {
      // 2
      tasks.add(streamFromHostTask(tensor));
      // 3
      tasks.add(fromHostTask(tensor, progs.weightsFromHostFragment()));
      // 4
      tasks.add(streamToHostTask(tensor));
      // 5
      tasks.add(toHostTask(tensor, progs.weightsToHostFragment()));
    }
  }

  // stream-to-device tensors : 1)  make tensor 2) make stream
  for (auto id : ir().getTensors().getIds(TensorType::Stream)) {
    Tensor *tensor = ir().getTensors().get(id);
    // 1
    tasks.add(initTensorTask(tensor));

    if (useSyntheticData() == false) {
      // 2
      tasks.add(streamFromHostTask(tensor));
    }
  }

  // Depending on anchor return types specified by the user, some
  // tensors may need to be added to the graph to keep track of
  // batch count.
  if (ir().getDataFlow().isBatchCountingRequired()) {
    tasks.add(initBatchCounterTensorsTask());
    tasks.add(updateBatchCoutTask(progs.forwardFragment()));
  }

  // stream-to-host tensors : 1) make streams 2) make copy programs
  // note that the order in which tasks are added does not matter,
  // they will be topologically sorted before running
  if (useSyntheticData() == false) {
    for (auto anchorId : ir().getDataFlow().anchors()) {
      // 1
      tasks.add(streamToHostTask(ir().getTensors().get(anchorId)));
      // 2
      auto *tensor = ir().getTensors().get(anchorId);
      switch (ir().getDataFlow().art(anchorId).id()) {
      // Copy program runs after every batch
      case (AnchorReturnTypeId::ALL): {
        tasks.add(toHostTask(tensor, programFragment(tensor)));
        break;
      }
      // Copy program runs at the end of the step
      case (AnchorReturnTypeId::FINAL): {
        tasks.add(toHostEveryNBatchesTask(tensor,
                                          ir().getDataFlow().batchesPerStep(),
                                          programFragment(tensor)));
        break;
      }
      // Copy program runs at the end of every N batches
      case (AnchorReturnTypeId::EVERYN): {
        tasks.add(toHostEveryNBatchesTask(tensor,
                                          ir().getDataFlow().art(anchorId).rp(),
                                          programFragment(tensor)));
        break;
      }
      }
    }

    // create Program to write optimizer tensors to device
    for (Tensor *tensor : ir().optimizerTensors()) {
      tasks.add(fromHostTask(tensor, progs.optimizerFromHostFragment()));
    }

    // making the network!
    for (Tensor *tensor : ir().dataStreamTensors()) {
      tasks.add(fromHostTask(tensor, programFragment(tensor)));
    }
  }

  std::vector<Op *> ops = ir().getOpSchedule({});
  double priority       = 0.;
  for (int i = 0; i < ops.size(); ++i) {
    Op *op = ops[i];
    tasks.add(opTask(op, priority));
    priority -= 1.;
  }

  for (auto &task : tasks.getLinearised()) {
    task.f();
  }

  logging::devicex::info("All tasks complete");

  pEngine.reset(new poplar::Engine(graph(), progs.progs(), engineOptions));
  logging::devicex::info("Engine created");

  pEngine->load(popDevice);
  logging::devicex::info("Engine loaded");

  if (useSyntheticData() == false) {
    logging::devicex::debug("Connecting initializer streams");
    for (auto id : ir().getTensors().getInitIds()) {
      Tensor *tensor = ir().getTensors().get(id);
      pEngine->connectStream(h2dId(id), tensor->tensorData()->data());
    }

    logging::devicex::debug("Connecting optimizer streams");
    for (Tensor *tensor : ir().optimizerTensors()) {
      pEngine->connectStream(h2dId(tensor->id), tensor->tensorData()->data());
    }

    auto engineToStream =
        [this](char *data0, int64_t n_bytes, PopStreamId streamId) {
          // Poplar has no const void * version, disappointing
          auto addr0 = static_cast<void *>(data0);
          auto addr1 = static_cast<void *>(data0 + n_bytes);
          // connect the stream (circular buffer)
          pEngine->connectStream(streamId, addr0, addr1);
        };

    logging::devicex::debug(
        "Creating host buffers for h2d streams, and connecting");
    for (Tensor *tensor : ir().dataStreamTensors()) {
      PopStreamId streamId = h2dId(tensor->id);
      // allocate host memory, where the poplar::Stream will read data from
      int64_t n_bytes =
          ir().getDataFlow().batchesPerStep() * tensor->info.nbytes();
      h2dBuffers[tensor->id] = std::vector<char>(n_bytes);
      char *data0            = h2dBuffers[tensor->id].data();
      engineToStream(data0, n_bytes, streamId);
    }

    logging::devicex::debug(
        "Creating host buffers for anchor d2h streams, connecting");
    for (TensorId anchorId : ir().getDataFlow().anchors()) {
      PopStreamId streamId = d2hId(anchorId);
      Tensor *tensor       = ir().getTensors().get(anchorId);
      int64_t batch_bytes  = tensor->info.nbytes();
      int64_t n_bytes;
      switch (ir().getDataFlow().art(anchorId).id()) {
      case (AnchorReturnTypeId::FINAL): {
        n_bytes = batch_bytes;
        break;
      }
      case (AnchorReturnTypeId::EVERYN): {
        n_bytes = batch_bytes * (ir().getDataFlow().batchesPerStep() /
                                 ir().getDataFlow().art(anchorId).rp());
        break;
      }
      case (AnchorReturnTypeId::ALL): {
        n_bytes = batch_bytes * ir().getDataFlow().batchesPerStep();
        break;
      }
      }
      d2hBuffers[anchorId] = std::vector<char>(n_bytes);
      char *data0          = d2hBuffers[tensor->id].data();
      engineToStream(data0, n_bytes, streamId);
    }

    logging::devicex::debug(
        "Creating host buffers for weight d2h streams, connecting");

    for (auto initId : ir().getTensors().getInitIds()) {
      PopStreamId streamId = d2hId(initId);
      Tensor *tensor       = ir().getTensors().get(initId);
      int64_t n_bytes      = tensor->info.nbytes();
      d2hBuffers[initId]   = std::vector<char>(n_bytes);
      char *data0          = d2hBuffers[initId].data();
      engineToStream(data0, n_bytes, streamId);
    }
  }
}

TaskId Devicex::streamFromHostTaskId(TensorId id) const {
  return "streamFromHostTask_" + id;
}

TaskId Devicex::streamToHostTaskId(TensorId id) const {
  return "streamToHostTask_" + id;
}

TaskId Devicex::fromHostTaskId(TensorId id) const {
  return "fromHostTask_" + id;
}

TaskId Devicex::toHostTaskId(TensorId id) const { return "toHostTask_" + id; }

TaskId Devicex::initBatchCounterTensorsTaskId() const {
  return "initBatchCounterTensorsTask";
}

TaskId Devicex::updateBatchCoutTaskId() const { return "updateBatchCoutTask"; }

TaskId Devicex::initTensorTaskId(TensorId id) const {
  return "initTensorTaskId_" + id;
}

TaskId Devicex::opTaskId(Op *op) const {

  std::stringstream ss;
  ss << "fromOpTask_" << op->id << '_' << op->opid;
  return ss.str();

  // return "fromOpTask_" + std::to_string(op->id) + '_' + op->opid;
}

PopStreamId Devicex::h2dId(TensorId id) const { return "h2d_" + id; }

PopStreamId Devicex::d2hId(TensorId id) const { return "d2h_" + id; }

PriTask Devicex::fromHostTask(Tensor *tensor,
                              poplar::program::Sequence &sq) const {

  auto f = [&sq, tensor, this]() {
    logging::devicex::debug("Adding poplar::program::Copy from host " +
                            tensor->id);
    sq.add(poplar::program::Copy(fromHostStreams.at(tensor->id),
                                 tensors.get(tensor->id)));
  };

  return {-1e6, // writes to device: always as late as possible
          fromHostTaskId(tensor->id),
          {
              streamFromHostTaskId(tensor->id), // poplar::Stream created
              initTensorTaskId(tensor->id)      // poplar::Tensor created
          },
          f};
}

PriTask Devicex::toHostTask(Tensor *tensor,
                            poplar::program::Sequence &sq) const {

  auto f = [&sq, tensor, this]() {
    logging::devicex::debug("Adding poplar::program::Copy to host " +
                            tensor->id);
    sq.add(poplar::program::Copy(tensors.get(tensor->id),
                                 toHostStreams.at(tensor->id)));
  };

  return {+1e6, // writes to host: always as early as possible
          toHostTaskId(tensor->id),
          {
              // the dependencies:
              streamToHostTaskId(tensor->id), // poplar::Stream creation task,
              taskWhichCreates(tensor->id)    // poplar::Tensor creation task.
          },
          f};
}

PriTask Devicex::initBatchCounterTensorsTask() {

  auto f = [this]() {
    logging::devicex::debug("Adding batch counter tensors");

    // Add scalar tensors outside of the ir to track the batch
    // Id and decide when to execute the copy to the host
    for (ReturnPeriod N : ir().getDataFlow().rps()) {
      // Add to map so copy task can access
      batchCountingTensors[N]      = graph().addVariable(poplar::INT, {});
      batchCountCheckingTensors[N] = graph().addVariable(poplar::BOOL, {});

      getConst(poplar::INT, {}, N);

      poputil::mapTensorLinearly(graph(), batchCountingTensors[N]);
      poputil::mapTensorLinearly(graph(), batchCountCheckingTensors[N]);
    }

    // Make sure const 1 tensor exists
    getConst(poplar::INT, {}, 1);
  };

  return {+1e6, // followed by writes to host: always as early as possible
          initBatchCounterTensorsTaskId(),
          {},
          f};
}

PriTask Devicex::updateBatchCoutTask(poplar::program::Sequence &sq) {

  auto f = [&sq, this]() {
    logging::devicex::debug("Adding batch count checker program");

    // Placeholder 'do nothing' branch if not running assign program
    poplar::program::Sequence emptyseq;

    // Increment the batch count at the at the earliest point
    // the anchor tensor is required, and check if it is a
    // copy batch
    for (ReturnPeriod N : ir().getDataFlow().rps()) {
      popops::addInPlace(
          graph(), batchCountingTensors[N], getConst(poplar::INT, {}, 1), sq);

      batchCountCheckingTensors[N] = popops::eq(
          graph(), batchCountingTensors[N], getConst(poplar::INT, {}, N), sq);

      // Reset batch count once it has reached N
      sq.add(poplar::program::If(
          batchCountCheckingTensors[N],
          poplar::program::Assign(batchCountingTensors[N], 0),
          emptyseq));
    }
  };

  return {+1e6, // followed by writes to host: always as early as possible
          updateBatchCoutTaskId(),
          {
              initBatchCounterTensorsTaskId() // poplar::Tensor creation task
          },
          f};
}

PriTask Devicex::toHostEveryNBatchesTask(Tensor *tensor,
                                         int N,
                                         poplar::program::Sequence &sq) {

  auto f = [&sq, tensor, N, this]() {
    logging::devicex::debug(
        "Adding conditional poplar::program::Copy to host " + tensor->id);

    poplar::Tensor isNthBatch = batchCountCheckingTensors.at(N);

    // Program to copy the anchor tensor and reset batch count
    poplar::program::Sequence copyseq;
    copyseq.add(poplar::program::Copy(tensors.get(tensor->id),
                                      toHostStreams.at(tensor->id)));

    // Placeholder 'do nothing' branch if not running copy program
    poplar::program::Sequence emptyseq;

    sq.add(poplar::program::If(isNthBatch, copyseq, emptyseq));
  };

  return {+1e6, // writes to host: always as early as possible
          toHostTaskId(tensor->id),
          {
              // the dependencies:
              updateBatchCoutTaskId(),        // updating poplar::Tensor task,
              streamToHostTaskId(tensor->id), // poplar::Stream creation task,
              taskWhichCreates(tensor->id)    // poplar::Tensor creation task.
          },
          f};
}

std::string Devicex::getSummaryReport() const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  std::stringstream ss;
  pEngine->printSummary(ss, reportOptions);
  return ss.str();
}

std::string Devicex::getGraphReport() const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  std::stringstream ss;
  auto report = pEngine->getGraphReport(reportOptions);
  report.serialize(ss, poplar::SerializationFormat::JSON);
  return ss.str();
}

std::string Devicex::getExecutionReport() const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  std::stringstream ss;
  auto report = pEngine->getExecutionReport(reportOptions);
  report.serialize(ss, poplar::SerializationFormat::JSON);
  return ss.str();
}

poplar::Type popType(const TensorInfo &info) {
  switch (info.dataType()) {
  case DataType::FLOAT: {
    return poplar::FLOAT;
  }
  case DataType::INT32: {
    return poplar::INT;
  }
  case DataType::FLOAT16: {
    return poplar::HALF;
  }

  case DataType::UNDEFINED:
  case DataType::UINT8:
  case DataType::INT8:
  case DataType::UINT16:
  case DataType::INT16:
  case DataType::INT64:
  case DataType::STRING:
  case DataType::BOOL:
  case DataType::BFLOAT16:
  case DataType::DOUBLE:
  case DataType::UINT32:
  case DataType::UINT64:
  case DataType::COMPLEX64:
  case DataType::COMPLEX128:
  default:
    throw error("Is there a poplar type for " + info.data_type() + "?");
  }
}

// piggy-backing on TensorInfo's data_type()
// function to get a string of the DataType
poplar::Type popType(DataType type) { return popType(TensorInfo(type, {1})); }

bool Devicex::useSyntheticData() {
  return (ir().getSessionOptions().ignoreData);
}

} // namespace popx
} // namespace poponnx
