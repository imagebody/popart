// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#define BOOST_TEST_MODULE SyntheticDataTest

#include <boost/test/unit_test.hpp>
#include <vector>

// Hack to allow the test to view the private data of classes
#define private public
#define protected public

#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/devicemanager.hpp>
#include <popart/filereader.hpp>
#include <popart/inputshapeinfo.hpp>
#include <popart/ir.hpp>
#include <popart/op/l1.hpp>
#include <popart/op/nll.hpp>
#include <popart/optimizer.hpp>
#include <popart/session.hpp>
#include <popart/tensor.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/tensornames.hpp>
#include <popart/testdevice.hpp>

#include <popart/popx/devicex.hpp>

using namespace popart;

BOOST_AUTO_TEST_CASE(SyntheticData_False) {

  // Build an onnx model
  auto builder = Builder::create();
  auto aiOnnx  = builder->aiOnnxOpset9();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto i1 = builder->addInputTensor(shape);
  std::vector<TensorId> tensorIds{i1};
  // Create a chain of identity ops
  for (int i = 0; i < 6; i++) {
    auto x = aiOnnx.identity({tensorIds[tensorIds.size() - 1]});
    tensorIds.push_back(x);
  }
  builder->addOutputTensor(tensorIds.back());

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  // Add the last tensor, and the 3rd tensor as anchors
  auto art       = AnchorReturnType("All");
  auto dataFlow  = DataFlow(1, {{tensorIds.back(), art}, {tensorIds[2], art}});
  auto optimizer = ConstSGD(0.01);
  std::vector<Loss *> losses{
      new L1Loss(tensorIds.back(), "l1LossVal", 0.1, ReductionType::Sum)};

  auto device = popart::createTestDevice(TEST_TARGET);

  auto session = popart::TrainingSession::createFromOnnxModel(
      proto,
      dataFlow,
      losses,
      optimizer,
      device,
      InputShapeInfo(),
      {},
      Patterns({popart::PreAliasPatternType::PostNRepl}));

  session->prepareDevice();

  popart::popx::Devicex *devicex =
      dynamic_cast<popart::popx::Devicex *>(session->device_.get());

  BOOST_TEST(devicex->ir().useSyntheticData() == false);
  BOOST_TEST(devicex->d2hWeightBuffers.size() == 0);
  // The one input tensor
  BOOST_TEST(devicex->fromHostStreams.size() == 1);
  // The two anchor tensors
  BOOST_TEST(devicex->toHostAnchorStreams.size() == 2);
}
BOOST_AUTO_TEST_CASE(SyntheticData_True) {

  // Build an onnx model
  auto builder = Builder::create();
  auto aiOnnx  = builder->aiOnnxOpset9();

  TensorInfo shape{"FLOAT", std::vector<int64_t>{2}};

  auto i1 = builder->addInputTensor(shape);
  std::vector<TensorId> tensorIds{i1};
  // Create a chain of identity ops
  for (int i = 0; i < 6; i++) {
    auto x = aiOnnx.identity({tensorIds[tensorIds.size() - 1]});
    tensorIds.push_back(x);
  }
  builder->addOutputTensor(tensorIds.back());

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR
  // Add the last tensor, and the 3rd tensor as anchors
  auto art       = AnchorReturnType("All");
  auto dataFlow  = DataFlow(1, {{tensorIds.back(), art}, {tensorIds[2], art}});
  auto optimizer = ConstSGD(0.01);
  std::vector<Loss *> losses{
      new L1Loss(tensorIds.back(), "l1LossVal", 0.1, ReductionType::Sum)};

  SessionOptions options;
  options.syntheticDataMode = SyntheticDataMode::Zeros;

  auto device = popart::createTestDevice(TEST_TARGET);

  auto session = popart::TrainingSession::createFromOnnxModel(
      proto,
      dataFlow,
      losses,
      optimizer,
      device,
      InputShapeInfo(),
      options,
      Patterns({popart::PreAliasPatternType::PostNRepl}));

  session->prepareDevice();

  popart::popx::Devicex *devicex =
      dynamic_cast<popart::popx::Devicex *>(session->device_.get());

  BOOST_TEST(devicex->ir().useSyntheticData() == true);
  BOOST_TEST(devicex->d2hWeightBuffers.size() == 0);
  BOOST_TEST(devicex->fromHostStreams.size() == 0);
  BOOST_TEST(devicex->toHostAnchorStreams.size() == 0);
  BOOST_TEST(devicex->toHostWeightStreams.size() == 0);
}
