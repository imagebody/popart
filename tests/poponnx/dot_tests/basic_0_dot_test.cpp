#define BOOST_TEST_MODULE BasicDotTest

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <random>
#include <vector>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/onnxutil.hpp>
#include <poponnx/op/l1.hpp>
#include <poponnx/op/nll.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/tensornames.hpp>
#include <poponnx/tensors.hpp>

using namespace poponnx;

std::string random_string(size_t length) {

  std::default_random_engine eng((std::random_device())());
  std::uniform_int_distribution<uint64_t> idis(
      0, std::numeric_limits<uint64_t>::max());

  auto randchar = [&idis, &eng]() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[idis(eng) % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

BOOST_AUTO_TEST_CASE(Dot_basic0) {

  // Consider the series of Ops:
  //
  // (in0) -> [Relu] -> (h0)
  //       -> [Exp]  -> (preId)
  //       -> [Identity] -> (out),

  // Build an onnx model
  auto builder = Builder::create();
  auto aiOnnx  = builder->aiOnnxOpset9();

  auto opts = SessionOptions();
  opts.dotChecks.insert(DotCheck::FWD0);
  opts.dotChecks.insert(DotCheck::FWD1);
  opts.dotChecks.insert(DotCheck::FINAL);

  opts.logDir = "./dotTestTmp" + random_string(14);
  boost::filesystem::create_directory(opts.logDir);

  TensorInfo shape{"FLOAT", std::vector<int64_t>{1}};
  auto in0   = builder->addInputTensor(shape);
  auto h0    = aiOnnx.relu({in0});
  auto preId = aiOnnx.exp({h0});
  auto out   = aiOnnx.identity({preId});
  builder->addOutputTensor(out);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  out           = modelProto.graph().output(0).name();
  auto dataFlow = DataFlow(1, {{out, AnchorReturnType("ALL")}});

  Ir ir;
  ir.prepare({modelProto,
              InputShapeInfo(),
              dataFlow,
              {},      // in inference mode, so no losses,
              nullptr, // and no optimizer
              opts,
              Patterns(PatternsLevel::NONE).enableInPlace(true)});

  // verify that there are 3 newly created dot_files
  auto dotFileNames =
      io::getMatchFns(io::getCanonicalDirName(opts.logDir), ".dot");
  BOOST_CHECK(dotFileNames.size() == 3);
}

// check that dotOpNames field functions correctly
BOOST_AUTO_TEST_CASE(Dot_dotOpNames0) {

  // For the simple model,
  // (in0) -> [Exp]  -> (preId)
  //       -> [Identity] -> (out),
  //
  // we give Exp the name,
  std::string expName = "sdgoimsdgpoisndglskdtjlsgilnsrkgnl";
  // and then test that setting .dotOpNames true (false)
  // does (does not) export the name to the .dot file

  auto getFullDotString = [expName](bool dotOpNames) {
    auto builder = Builder::create();
    auto aiOnnx  = builder->aiOnnxOpset9();
    auto opts    = SessionOptions();
    // just the one .dot file will be written
    opts.dotChecks.insert(DotCheck::BWD0);
    opts.dotOpNames = dotOpNames;
    opts.logDir     = "./dotTestTmp" + random_string(14);
    boost::filesystem::create_directory(opts.logDir);
    TensorInfo shape{"FLOAT", std::vector<int64_t>{1}};
    auto in0   = builder->addInputTensor(shape);
    auto preId = aiOnnx.exp({in0}, expName);
    auto out   = aiOnnx.identity({preId});
    builder->addOutputTensor(out);
    auto proto      = builder->getModelProto();
    auto modelProto = io::getModelFromString(proto);
    out             = modelProto.graph().output(0).name();
    auto dataFlow   = DataFlow(1, {{out, AnchorReturnType("ALL")}});
    Ir ir;

    // note that we are not in training mode,
    // but BWD0 is still a valid checkpoint
    ir.prepare({modelProto,
                InputShapeInfo(),
                dataFlow,
                {},      // in inference mode, so no losses,
                nullptr, // and no optimizer
                opts,
                Patterns(PatternsLevel::NONE)});

    // verify that there is 1 newly created dot_file
    auto dotFileNames =
        io::getMatchFns(io::getCanonicalDirName(opts.logDir), ".dot");
    BOOST_CHECK(dotFileNames.size() == 1);

    // we have just verified that there is just the 1 .dot file,
    if (dotFileNames.size() == 1) {
      auto fn = dotFileNames.back();
      io::confirmRegularFile(fn);
      std::ifstream ifs(fn);
      return std::string((std::istreambuf_iterator<char>(ifs)),
                         (std::istreambuf_iterator<char>()));

    } else {
      throw error("dotFileNames not of size 1");
    }
  };

  auto fullDot = getFullDotString(true);
  BOOST_CHECK(fullDot.find(expName) != std::string::npos);

  fullDot = getFullDotString(false);
  BOOST_CHECK(fullDot.find(expName) == std::string::npos);
}