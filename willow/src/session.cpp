#include <poponnx/device.hpp>
#include <poponnx/error.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/logging.hpp>
#include <poponnx/onnxutil.hpp>
#include <poponnx/optionflags.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/session.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensordata.hpp>
#include <poponnx/tensors.hpp>
#include <poponnx/util.hpp>

namespace poponnx {

Session::Session() {}

void Session::configureFromOnnx(const std::string &modelProtoOrFilename,
                                const DataFlow &df,
                                const InputShapeInfo &perk,
                                const std::vector<Loss *> &lossesIn,
                                const Optimizer *optimizerIn,
                                const SessionOptions &userOptions,
                                const Patterns &patterns) {

  logging::session::trace("Session::configureFromOnnx");

  auto modelProto = onnxutil::getModelProto(modelProtoOrFilename);

  ir.prepare(
      {modelProto, perk, df, lossesIn, optimizerIn, userOptions, patterns});
}

std::unique_ptr<Session>
Session::createFromOnnxModel(const std::string &model,
                             const DataFlow &dataFlow,
                             const InputShapeInfo &inputShapeInfo,
                             const std::vector<Loss *> &losses,
                             const Optimizer *optimizer,
                             const SessionOptions &userOptions,
                             const Patterns &patterns) {

  // Needs to be the first call to initialise the logging settings
  logging::configure(userOptions.loggingOptions);

  logging::session::trace("Session::createFromOnnx");

  // Note : Can not use make_unique as the implementation can not acces the
  // private constructor
  auto session = std::unique_ptr<Session>(new Session());
  session->configureFromOnnx(model,
                             dataFlow,
                             inputShapeInfo,
                             losses,
                             optimizer,
                             userOptions,
                             patterns);
  return session;
}

void Session::updateOptimizer(const Optimizer *optimizer) {
  logging::session::trace("Session::updateOptimizer");
  ir.updateOptimizer(optimizer);
}

void Session::setDevice(DeviceInfo &deviceInfo) {
  logging::session::trace("Session::setDevice({})", deviceInfo);
  device_.reset(new popx::Devicex(ir, deviceInfo));
}

// get the TensorInfo on a Tensor
TensorInfo Session::getInfo(TensorId id) const {
  logging::session::trace("Session::getInfo({})", id);
  TensorInfo info = ir.getTensors().get(id)->info;
  if (!info.isSet()) {
    throw error("TensorInfo for `" + id + "' not set");
  }
  return info;
}

Session::~Session() = default;

void Session::prepareDevice() {
  logging::session::trace("Session::prepareDevice()");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  device_->prepare();
}

void Session::weightsFromHost() {
  logging::session::trace("Sessions::weightsFromHost");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  device_->weightsFromHost();
  weightsFromHostCalled = true;
}

// write whatever optimizer tensors (learning rates,
// momentum, initial momentum tensors (zero)) there are to device
void Session::optimizerFromHost() {
  logging::session::trace("Session::optimizerFromHost");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  device_->optimizerFromHost();
}

void Session::train(const IStepIO &stepio) {
  logging::session::trace("Session::train");
  if (!ir.canTrain()) {
    throw error("Trying to train when not in training mode");
  }

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  if (ir.containsInitialisers() && weightsFromHostCalled == false) {
    throw error(
        "Must call weightsFromHost before {} as the model has initializers",
        __func__);
  }

  device_->train(stepio);
}

void Session::evaluate(const IStepIO &stepio) {
  logging::session::trace("Session::evaluate");
  if (!ir.canEvaluate()) {
    throw error("Trying to evaluate when not in evaluation mode");
  }

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  if (ir.containsInitialisers() && ir.isTraining() &&
      weightsFromHostCalled == false) {
    throw error("Must call weightsFromHost before evaluate as the model has "
                "initializers "
                "and the session has been created in training mode");
  }

  device_->evaluate(stepio);
}

void Session::infer(const IStepIO &stepio) {
  logging::session::trace("Session::infer");
  if (!ir.canInfer()) {
    throw error("Trying to infer when not in inference mode");
  }

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  if (ir.containsInitialisers() && ir.isTraining() &&
      weightsFromHostCalled == false) {
    throw error(
        "Must call weightsFromHost before infer as the model has initializers "
        "and the session has been created in training mode");
  }

  device_->infer(stepio);
}

// write current model to ONNX file
void Session::modelToHost(const std::string &fn) {
  logging::session::trace("Session::modelToHost");

  onnx::ModelProto model = ir.getModel();

  std::map<TensorId, MutableVoidData> initMap;
  for (int init_index = 0; init_index < model.graph().initializer_size();
       ++init_index) {
    onnx::TensorProto &tp =
        *model.mutable_graph()->mutable_initializer(init_index);
    TensorId tenId = tp.name();
    initMap[tenId] = onnxutil::getMutableData(tp);
  }
  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  device_->weightsToHost(initMap);

  io::writeModel(model, fn);
}

std::string Session::getSummaryReport() const {
  logging::session::trace("Session::getSummaryReport");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  return device_->getSummaryReport();
}

std::string Session::getGraphReport() const {
  logging::session::trace("Session::getGraphReport");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  return device_->getGraphReport();
}

std::string Session::getExecutionReport() const {
  logging::session::trace("Session::getExecutionReport");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  return device_->getExecutionReport();
}

TensorTileMap Session::getTensorTileMap() const {
  logging::session::trace("Session::getTensorTileMap");

  if (!device_) {
    throw error("Must call setDevice before {}", __func__);
  }

  return device_->getTensorTileMap();
}

void Session::resetHostWeights(const std::string &modelProtoOrFilename) {
  logging::session::trace("Session::resetHostWeights");
  auto modelProto = onnxutil::getModelProto(modelProtoOrFilename);
  ir.resetWeights(modelProto);

  // After the weights has been reset they must be rewritten to the target
  weightsFromHostCalled = false;
}

} // namespace poponnx
