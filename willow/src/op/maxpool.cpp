#include <popart/error.hpp>
#include <popart/op/maxpool.hpp>

#include <memory>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

MaxPoolOp::MaxPoolOp(const OperatorIdentifier &_opid,
                     const std::vector<int64_t> &kernelShape_,
                     int64_t storageOrder_,
                     int64_t ceilMode_,
                     const HasReceptiveFieldOp::Settings &settings_)
    : HasReceptiveFieldOp(_opid, settings_), storageOrder(storageOrder_),
      ceilMode(ceilMode_), kernelShape(kernelShape_) {}

void MaxPoolOp::setup0() {

  if (storageOrder != 0) {
    throw error("storage_order != 0, not supported");
  }
}

void MaxPoolOp::setSpatialK() {
  spatialK.resize(nSpatialDims);

  if (kernelShape.size() != inRank(getInIndex()) - 2) {
    throw error(
        "invalid kernel_shape, not same rank as the tensor operated on");
  }
  for (int spDim = 0; spDim < nSpatialDims; ++spDim) {
    spatialK[spDim] = kernelShape[spDim];
  }
}

const MaxPoolOp *MaxPoolGradOp::getCloneOfCreator() const {
  return dynamic_cast<MaxPoolOp *>(cloneOfCreator.get());
}

std::unique_ptr<Op> MaxPoolOp::clone() const {
  return std::make_unique<MaxPoolOp>(*this);
}

// Pooling does not change the number of channels,
// i.e it is the same as the number of input channels
int64_t MaxPoolOp::getNOutChans() const { return nInChans; }

std::vector<std::unique_ptr<Op>> MaxPoolOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<MaxPoolGradOp>(*this));
  return upops;
}

void MaxPoolOp::appendAttributes(OpSerialiserBase &os) const {
  HasReceptiveFieldOp::appendAttributes(os);
  os.appendAttribute("storage_order", storageOrder);
  os.appendAttribute("kernel_shape", kernelShape);
  os.appendAttribute("ceil_mode", ceilMode);
}

MaxPoolGradOp::MaxPoolGradOp(const MaxPoolOp &op_)
    : Op(Onnx::GradOperators::MaxPoolGrad, op_.getSettings()),
      unpooledInfo(op_.inInfo(MaxPoolOp::getInIndex())),
      cloneOfCreator(op_.clone()) {}

void MaxPoolGradOp::appendAttributes(OpSerialiserBase &os) const {
  Op::appendAttributes(os);
  os.appendForwardOp(getCloneOfCreator());
}

const std::vector<GradInOutMapper> &MaxPoolGradOp::gradInputInfo() const {

  // the input to the grad-op at index getGradPooledIn()
  // is the gradient of the output of the max pool
  // at index 0.
  // the input to the grad-op at index getPooledIn()
  // is the output of the max pool at index 0
  // etc for getPrePooledIn()
  static const std::vector<GradInOutMapper> inInfo = {
      {getGradPooledInIndex(), MaxPoolOp::getOutIndex(), GradOpInType::GRADOUT},
      {getPooledInIndex(), MaxPoolOp::getOutIndex(), GradOpInType::OUT},
      {getPrePooledInIndex(), MaxPoolOp::getInIndex(), GradOpInType::IN}};
  return inInfo;
}

// The input to the max pool (PrePooled) is
// the input to the grad op at index 0.

const std::map<int, int> &MaxPoolGradOp::gradOutToNonGradIn() const {
  // the grad-op output at index 0 corresponds
  // to the non-grad-op's input at index 0
  static const std::map<int, int> outInfo = {
      {getOutIndex(), MaxPoolOp::getInIndex()}};
  return outInfo;
}

void MaxPoolGradOp::setup() { outInfo(getOutIndex()) = unpooledInfo; }

std::unique_ptr<Op> MaxPoolGradOp::clone() const {
  return std::make_unique<MaxPoolGradOp>(*this);
}

namespace {
static OpCreator<MaxPoolOp> maxPoolOpCreator(
    {Onnx::Operators::MaxPool_8, Onnx::Operators::MaxPool_1},
    [](const OperatorIdentifier &_opid,
       const Op::Settings &settings,
       const Attributes &attr) -> std::unique_ptr<Op> {
      HasReceptiveFieldOp::Settings receptiveSettings(
          settings.graph, settings.name, settings.scope);
      receptiveSettings.setFromAttributes(attr);

      int64_t storageOrder =
          attr.getAttribute<Attributes::Int>("storage_order", 0);
      int64_t ceilMode = attr.getAttribute<Attributes::Int>("ceil_mode", 0);
      std::vector<int64_t> kernelShape =
          attr.getAttribute<Attributes::Ints>("kernel_shape", {});

      return std::unique_ptr<Op>(new MaxPoolOp(
          _opid, kernelShape, ceilMode, storageOrder, receptiveSettings));
    },
    true);
} // namespace

} // namespace popart
