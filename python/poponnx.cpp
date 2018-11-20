#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <poponnx/builder.hpp>
#include <poponnx/error.hpp>
#include <poponnx/l1.hpp>
#include <poponnx/loss.hpp>
#include <poponnx/nll.hpp>
#include <poponnx/numerics.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/optionflags.hpp>
#include <poponnx/session.hpp>
#include <poponnx/tensordata.hpp>

namespace py = pybind11;
using namespace willow;

std::map<std::string, DataType> initNpTypeMap() {
  std::map<std::string, DataType> M;
  // see tensorinfo.hpp for the complete list of
  // DataTypes (defined originally in ONNX)
  M["float16"] = TP::FLOAT16;
  M["float32"] = TP::FLOAT;
  M["float64"] = TP::DOUBLE;
  M["int16"]   = TP::INT16;
  M["int32"]   = TP::INT32;
  M["int64"]   = TP::INT64;
  return M;
}

DataType getDataTypeFromNpType(std::string npType) {
  const static std::map<std::string, DataType> M = initNpTypeMap();
  auto found                                     = M.find(npType);
  if (found == M.end()) {
    throw error("No numpy type " + npType + " registered in map to DataType");
  }
  return found->second;
}

TensorInfo getTensorInfo(py::array npArr) {
  auto dtype      = npArr.dtype();
  auto typeString = py::str(dtype);
  auto tRank      = npArr.ndim();
  std::vector<int64_t> shape;
  for (int i = 0; i < tRank; ++i) {
    shape.push_back(npArr.shape(i));
  }
  return TensorInfo(getDataTypeFromNpType(typeString), shape);
}

class PyStepIO : public StepIO {
public:
  PyStepIO(std::map<TensorId, py::array> inputs_,
           std::map<TensorId, py::array> outputs_)
      : inputs(inputs_), outputs(outputs_) {}

  template <typename T>
  T get(TensorId id,
        const std::map<TensorId, py::array> &M,
        std::string mapName) const {
    auto found = M.find(id);
    if (found == M.end()) {
      throw error("No tensor " + id + " provided in PyStepIO's " + mapName);
    }
    py::array npArr = found->second;
    T stepData;
    stepData.data = npArr.request().ptr;
    stepData.info = getTensorInfo(npArr);
    return stepData;
  }

  ConstVoidData in(TensorId id) const final {
    return get<ConstVoidData>(id, inputs, "inputs");
  }

  MutableVoidData out(TensorId id) const final {
    return get<MutableVoidData>(id, outputs, "outputs");
  }

private:
  std::map<TensorId, py::array> inputs;
  std::map<TensorId, py::array> outputs;
};

PYBIND11_MODULE(poponnx_core, m) {
  m.doc() = "binding for C++ poponnx library";

  m.def("getTensorInfo", &getTensorInfo);

  py::class_<StepIO> stepio(m, "StepIO");

  py::enum_<AnchorReturnType>(m, "AnchorReturnType")
      .value("FINAL", AnchorReturnType::FINAL)
      .value("SUM", AnchorReturnType::SUM)
      .value("ALL", AnchorReturnType::ALL);

  py::class_<PyStepIO>(m, "PyStepIO", stepio)
      .def(py::init<std::map<TensorId, py::array>,
                    std::map<TensorId, py::array>>(),
           py::arg("inputs"),
           py::arg("outputs"));

  py::class_<DataFlow>(m, "DataFlow")
      .def(
          py::init<int, int, const std::vector<TensorId> &, AnchorReturnType>(),
          py::arg("batchesPerStep"),
          py::arg("batchSize"),
          py::arg("anchorTensors"),
          py::arg("anchorReturnType"))
      .def("nAnchors", &DataFlow::nAnchors)
      .def("batchSize", &DataFlow::batchSize)
      .def("batchesPerStep", &DataFlow::batchesPerStep)
      .def("anchors", &DataFlow::anchors, pybind11::return_value_policy::copy)
      .def("art", &DataFlow::art);

  py::class_<TensorInfo>(m, "TensorInfo")
      .def(py::init<std::string, const std::vector<int64_t> &>(),
           py::arg("dataType"),
           py::arg("shape"))
      .def("data_type_lcase", &TensorInfo::data_type_lcase)
      .def("shape", &TensorInfo::shape);

  py::class_<numerics::NumericsReport>(m, "NumericsReport")
      .def(py::init<std::string, std::string, std::string, std::string>(),
           py::arg("A0"),
           py::arg("A1"),
           py::arg("B0"),
           py::arg("B1"))
      .def("report", &numerics::NumericsReport::report)
      .def("fullReport", &numerics::NumericsReport::fullReport);

  py::class_<EarlyInfo>(m, "EarlyInfo")
      .def(py::init<>())
      .def("add", &EarlyInfo::add)
      .def("get", &EarlyInfo::get)
      .def("has", &EarlyInfo::has);

  py::class_<Loss> loss(m, "Loss");
  loss.def("input", &Loss::input);

  py::class_<NllLoss>(m, "NllLoss", loss)
      .def(py::init<TensorId, TensorId, TensorId>(),
           py::arg("probabilities"),
           py::arg("labels"),
           py::arg("output"))
      .def("probsTensorId", &NllLoss::probsTensorId)
      .def("labelTensorId", &NllLoss::labelTensorId);

  py::class_<L1Loss>(m, "L1Loss", loss)
      .def(py::init<TensorId, TensorId, float>(),
           py::arg("input"),
           py::arg("output"),
           py::arg("lambda"))
      .def("getInputId", &L1Loss::getInputId)
      .def("getLambda", &L1Loss::getLambda);

  py::class_<Optimizer> optimizer(m, "Optimizer");

  py::class_<BaseSGD> basesgd(m, "BaseSGD", optimizer);
  basesgd.def("learnRate", &BaseSGD::learnRate);

  py::class_<SGD>(m, "SGD", basesgd)
      .def(py::init<float>(), py::arg("learning_rate"));

  py::class_<ConstSGD>(m, "ConstSGD", basesgd)
      .def(py::init<float>(), py::arg("learning_rate"));

  py::class_<SessionOptions>(m, "SessionOptionsCore")
      .def(py::init<>())
      .def_readwrite("exportDot", &SessionOptions::exportDot)
      .def_readwrite("engineOptions", &SessionOptions::engineOptions)
      .def_readwrite("convolutionOptions", &SessionOptions::convolutionOptions)
      .def_readwrite("reportOptions", &SessionOptions::reportOptions)
      .def_readwrite("logging", &SessionOptions::loggingOptions);

  py::class_<Session>(m, "SessionCore")
      .def(py::init(&Session::createFromOnnxModel),
           py::arg("model"),
           py::arg("earlyInfo").none(),
           py::arg("dataFlow").none(),
           py::arg("losses"),
           py::arg("optimizer").none(),
           py::arg("cTens"),
           py::arg("logdir"),
           py::arg("userOptions"),
           py::arg("patternNames"))
      .def("updateOptimizer", &Session::updateOptimizer)
      .def("setDevice", &Session::setDevice)
      .def("prepareDevice", &Session::prepareDevice)
      .def("weightsFromHost", &Session::weightsFromHost)
      .def("optimizerFromHost", &Session::optimizerFromHost)
      .def("train", &Session::train)
      .def("evaluate", &Session::evaluate)
      .def("infer", &Session::infer)
      .def("modelToHost", &Session::modelToHost)
      .def("getInfo", &Session::getInfo)
      .def("getSummaryReport", &Session::getSummaryReport)
      .def("getGraphReport", &Session::getGraphReport)
      .def("getExecutionReport", &Session::getExecutionReport)
      .def("resetHostWeights", &Session::resetHostWeights);

  py::class_<Builder>(m, "BuilderCore")
      .def(py::init<>())
      .def("addInputTensor", &Builder::addInputTensor, py::arg("tensorInfo"))
      .def("addOutputTensor", &Builder::addOutputTensor, py::arg("outputName"))
      .def("add", &Builder::add, py::arg("lhs"), py::arg("rhs"))
      .def("convolution",
           &Builder::convolution,
           py::arg("input"),
           py::arg("kernel"),
           py::arg("strides"),
           py::arg("padding"),
           py::arg("dilation"),
           py::arg("groups"))
      .def("convolutionWithBias",
           &Builder::convolutionWithBias,
           py::arg("input"),
           py::arg("kernel"),
           py::arg("bias"),
           py::arg("strides"),
           py::arg("padding"),
           py::arg("dilation"),
           py::arg("groups"))
      .def("gemm",
           &Builder::gemm,
           py::arg("lhs"),
           py::arg("rhs"),
           py::arg("bias"),
           py::arg("alpha"),
           py::arg("beta"),
           py::arg("transA"),
           py::arg("transB"))
      .def("matmul", &Builder::matmul, py::arg("lhs"), py::arg("rhs"))
      .def("getModelProto", [](const Builder &builder) {
        return py::bytes(builder.getModelProto());
      });

  py::register_exception<willow::error>(m, "exception");
}
