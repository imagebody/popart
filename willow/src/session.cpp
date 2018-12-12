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
#include <poponnx/util.hpp>

namespace poponnx {

Session::Session() {}

void Session::configureFromOnnx(const std::string &modelProtoOrFilename,
                                const DataFlow &df,
                                const InputShapeInfo &perk,
                                const std::vector<Loss *> &lossesIn,
                                const Optimizer *optimizerIn,
                                const std::vector<TensorId> &cTens,
                                const SessionOptions &userOptions,
                                const Patterns &patterns) {

  logging::session::trace("Session::configureFromOnnx");

  auto modelProto = onnxutil::getModelProto(modelProtoOrFilename);

  ir.prepare({modelProto,
              perk,
              df,
              lossesIn,
              optimizerIn,
              cTens,
              userOptions,
              patterns});
}

std::unique_ptr<Session>
Session::createFromOnnxModel(const std::string &model,
                             const DataFlow &dataFlow,
                             const InputShapeInfo &inputShapeInfo,
                             const std::vector<Loss *> &losses,
                             const Optimizer *optimizer,
                             const std::vector<std::string> &cTens,
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
                             cTens,
                             userOptions,
                             patterns);
  return session;
}

void Session::updateOptimizer(const Optimizer *optimizer) {
  logging::session::trace("Session::updateOptimzier");
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
  device_->prepare();
}

void Session::weightsFromHost() {
  logging::session::trace("Sessions::weightsFromHost");
  device_->weightsFromHost();
}

// write whatever optimizer tensors (learning rates,
// momentum, initial momentum tensors (zero)) there are to device
void Session::optimizerFromHost() {
  logging::session::trace("Session::optimizerFromHost");
  device_->optimizerFromHost();
}

void Session::train(const StepIO &stepio) {
  logging::session::trace("Session::train");
  if (!ir.canTrain()) {
    throw error("Trying to train when not in training mode");
  }

  device_->train(stepio);
}

void Session::evaluate(const StepIO &stepio) {
  logging::session::trace("Session::evaluate");
  if (!ir.canEvaluate()) {
    throw error("Trying to evaluate when not in evaluation mode");
  }

  device_->evaluate(stepio);
}

void Session::infer(const StepIO &stepio) {
  logging::session::trace("Session::infer");
  if (!ir.canInfer()) {
    throw error("Trying to infer when not in inference mode");
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

  device_->weightsToHost(initMap);

  io::writeModel(model, fn);
}

std::string Session::getSummaryReport() const {
  logging::session::trace("Session::getSummaryReport");
  return device_->getSummaryReport();
}

std::string Session::getGraphReport() const {
  logging::session::trace("Session::getGraphReport");
  return device_->getGraphReport();
}

std::string Session::getExecutionReport() const {
  logging::session::trace("Session::getExecutionReport");
  return device_->getExecutionReport();
}

void Session::resetHostWeights(const std::string &modelProtoOrFilename) {
  logging::session::trace("Session::updateModel");
  auto modelProto = onnxutil::getModelProto(modelProtoOrFilename);
  ir.resetWeights(modelProto);
}

} // namespace poponnx
