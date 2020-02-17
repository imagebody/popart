#define BOOST_TEST_MODULE SliceOutlining0NumericalTest

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <complex>
#include <iostream>
#include <random>
#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/devicemanager.hpp>
#include <popart/filereader.hpp>
#include <popart/inputshapeinfo.hpp>
#include <popart/ir.hpp>
#include <popart/names.hpp>
#include <popart/ndarraywrapper.hpp>
#include <popart/op/l1.hpp>
#include <popart/optimizer.hpp>
#include <popart/session.hpp>
#include <popart/tensordata.hpp>

using namespace popart;

BOOST_AUTO_TEST_CASE(SliceTest0) {
  auto getValue = [](bool outline, bool inplace) {
    //
    //
    //
    // 1  2  3  4  5  6  7
    //||    ||    ||     |
    // scale  scale scale  scale (by index)
    //
    // sum all -> 1 + 2 + 9 + 12 + 25 + 30 + 49 = 128
    //
    //
    Shape inShape{7};
    TensorInfo inInfo{"FLOAT", inShape};

    std::vector<float> vInData(7, 0);
    for (uint64_t i = 0; i < 7; ++i) {
      vInData[i] = static_cast<float>(i + 1);
    }

    // Build an onnx model
    auto builder     = Builder::create();
    auto aiOnnx      = builder->aiOnnxOpset9();
    auto aiGraphcore = builder->aiGraphcoreOpset1();

    std::vector<TensorId> sliceIds;
    auto in0 = builder->addInputTensor(inInfo);
    for (int i = 0; i < 7; ++i) {
      auto sliceStart = i - i % 2;
      auto sliceOut = aiOnnx.slice({in0}, {sliceStart + 1}, {sliceStart}, {0});
      auto scaled = aiGraphcore.scale({sliceOut}, 1.0 + static_cast<float>(i));
      sliceIds.push_back(scaled);
    }

    auto out = aiOnnx.sum(sliceIds);
    builder->addOutputTensor(out);

    auto proto      = builder->getModelProto();
    auto modelProto = io::getModelFromString(proto);

    // Create the IR, adding outId as an anchor
    auto art      = AnchorReturnType("ALL");
    auto dataFlow = DataFlow(1, {{out, art}});

    auto opts             = SessionOptions();
    opts.enableOutlining  = outline;
    opts.outlineThreshold = 0.0f;
    auto cpuDevice =
        popart::DeviceManager::createDeviceManager().createCpuDevice();
    auto session = popart::InferenceSession::createFromOnnxModel(
        proto,
        dataFlow,
        cpuDevice,
        {},
        popart::InputShapeInfo(),
        opts,
        popart::Patterns(PatternsLevel::NONE).enableInPlace(inplace));

    // prepare the anchors
    float rawOutputData;
    Shape outShape{};
    popart::NDArrayWrapper<float> outData(&rawOutputData, outShape);

    std::map<popart::TensorId, popart::IArray &> anchors = {
        {out, outData},
    };

    session->prepareDevice();

    auto ngraphs = session->getIr().getGraphs().size();
    if (outline == true) {
      // we expect 4 graphs. the main graph, and 3 for slices at 0, 2, 4, each
      // of which has 2 instances
      std::cout << "ngraphs = " << ngraphs << std::endl;
      BOOST_CHECK(ngraphs == 4);
    } else {
      BOOST_CHECK(ngraphs == 1);
    }

    popart::NDArrayWrapper<float> inData(vInData.data(), inShape);
    std::map<popart::TensorId, popart::IArray &> inputs = {{in0, inData}};

    popart::StepIO stepio(inputs, anchors);
    session->run(stepio);
    return rawOutputData;
  };

  // outlining, no inplacing
  auto vOutline = getValue(true, false);
  // no outlining, no inplacing
  auto vBase = getValue(false, false);
  // no outlining, inplacing
  auto vInplace = getValue(false, true);
  // outlining, inplacing
  auto vAll = getValue(true, true);

  std::cout << vBase << "  == " << vOutline << " == " << vInplace
            << " == " << vAll << " ?" << std::endl;
  BOOST_CHECK(vBase == 128);
  BOOST_CHECK(vBase - vOutline == 0.0);
  BOOST_CHECK(vOutline - vInplace == 0.0);
  BOOST_CHECK(vInplace - vAll == 0.0);
}