#define BOOST_TEST_MODULE ConstExprAddTest

#include <boost/test/unit_test.hpp>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/names.hpp>
#include <poponnx/op/l1.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensordata.hpp>
#include <poponnx/tensors.hpp>

using namespace poponnx;

BOOST_AUTO_TEST_CASE(ConstExprTest_Add0) {

  // The compute graph :
  //
  // data  -----------------------------|
  //                                    |
  //                                    |
  //                                    |- RESHAPE ---> output
  //                                    |
  // shape0 -------|                    |
  //               |                    |
  //               |- ADD - outshape ---|
  //               |
  // shape1 -------|

  // We will reshape a tensor from rank-4:
  Shape inShape = {2, 5, 3, 4};
  // to rank-2: {10, 12},
  // Note above that the total number elements of the tensor remains 120

  // where the output shape {10, 12} will be the sum of two tensors,
  // 1)
  Shape shape0 = {7, 4};
  // 2)
  Shape shape1 = {3, 8};

  Shape outShapeSize = {static_cast<int64_t>(shape0.size())};
  TensorInfo inInfo{"FLOAT", inShape};

  ConstVoidData out0ShapeData = {shape0.data(), {"INT64", outShapeSize}};
  ConstVoidData out1ShapeData = {shape1.data(), {"INT64", outShapeSize}};

  // Build an onnx model
  auto builder = Builder::create();
  // The two fixed-point tensors which are Constants
  auto shape0Id   = builder->constant(out0ShapeData, "out0ShapeData");
  auto shape1Id   = builder->constant(out1ShapeData, "out1ShapeData");
  auto inId       = builder->addInputTensor(inInfo);
  auto outShapeId = builder->add({shape0Id, shape1Id});
  auto outId      = builder->reshape({inId, outShapeId});
  builder->addOutputTensor(outId);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR, adding outId as an anchor
  auto art       = AnchorReturnType("ALL");
  auto dataFlow  = DataFlow(1, {{outId, art}});
  auto optimizer = ConstSGD(0.01);
  std::vector<Loss *> losses{new L1Loss(outId, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              InputShapeInfo(),
              dataFlow,
              losses,
              &optimizer,
              {}, // no SessionOptions
              Patterns({PatternType::POSTNREPL})});

  // Check the ir
  // 1) that the Reshape Op is present,
  BOOST_CHECK(ir.opsOfType(Onnx::AiOnnx::OpSet9::Reshape).size() == 1);
  // 2) that the shape of the output tensor is as specified.
  Shape outShape;
  for (int i = 0; i < outShapeSize[0]; ++i) {
    outShape.push_back(shape0[i] + shape1[i]);
  }
  BOOST_CHECK(ir.getTensors().get(outId)->info.shape() == outShape);
}

BOOST_AUTO_TEST_CASE(ConstExprTest_Add1) {
  // Testing ConstExpr folding on broadcast adds of
  // initializers and constants

  // The compute graph :
  //
  // w0 --|
  //      |--[bc-add]-- a0 --|
  // w1 --|                  |
  //                         |--[bc-add]-- a2 -|
  // c0 --|                  |                 |
  //      |--[bc-add]-- a1 --|                 |-- [matmul] -- o
  // c1 --|                                    |
  //                                           |
  // i1 ---------------------------------------|
  //

  // weights
  TensorInfo w0Shape{"FLOAT", std::vector<int64_t>{1, 3}};
  float w0Vals[1 * 3]  = {0};
  ConstVoidData w0Data = {w0Vals, w0Shape};

  TensorInfo w1Shape{"FLOAT", std::vector<int64_t>{3, 3}};
  float w1Vals[3 * 3]  = {1};
  ConstVoidData w1Data = {w1Vals, w1Shape};

  // consts
  TensorInfo c0Shape{"FLOAT", std::vector<int64_t>{1, 3}};
  float c0Vals[1 * 3]  = {2};
  ConstVoidData c0Data = {c0Vals, c0Shape};

  TensorInfo c1Shape{"FLOAT", std::vector<int64_t>{1}};
  float c1Vals[1]      = {3};
  ConstVoidData c1Data = {c1Vals, c1Shape};

  // input
  TensorInfo inputInfo{"FLOAT", std::vector<int64_t>{3, 4}};

  // Build an onnx model
  auto builder = Builder::create();

  auto w0Id = builder->addInitializedInputTensor(w0Data);
  auto w1Id = builder->addInitializedInputTensor(w1Data);
  auto a0   = builder->add({w0Id, w1Id}, "a0");

  auto c0Id = builder->constant(c0Data, "c0Data");
  auto c1Id = builder->constant(c1Data, "c1Data");
  auto a1   = builder->add({c0Id, c1Id}, "a1");

  auto a2      = builder->add({a0, a1}, "a2");
  auto inputId = builder->addInputTensor(inputInfo);
  auto outId   = builder->matmul({a2, inputId});
  builder->addOutputTensor(outId);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR, adding outId as an anchor
  auto art       = AnchorReturnType("ALL");
  auto dataFlow  = DataFlow(1, {{outId, art}});
  auto optimizer = ConstSGD(0.01);
  std::vector<Loss *> losses{new L1Loss(outId, "l1LossVal", 0.1)};

  Ir ir;
  ir.prepare({modelProto,
              InputShapeInfo(),
              dataFlow,
              {}, // no loss
              {}, // no optimizer
              {}, // no SessionOptions
              Patterns({PatternType::POSTNREPL})});

  // Check that the Add Op is has been removed from the IR
  // by ConstExpr folding
  // TODO: this test will give a false pass if the model is using
  // a newer opset. Fix when T6274 is complete
  BOOST_CHECK(ir.opsOfType(Onnx::AiOnnx::OpSet9::Add).size() == 0);
}

BOOST_AUTO_TEST_CASE(ConstExprTest_Add2) {
  // Testing ConstExpr folding on an input tensor whose consumer
  // has another input that cannot be removed by ConstExpr folding

  // Model from the builder:
  //
  // v0 ----|
  //        |--[add]-- a0 --|
  //        |               |
  // c0 --|-|               |--[add]-- o
  //      |                 |
  //      |--[add]---- a1 --|
  // c1 --|
  //
  // Expected outcome after ConstExpr folding :
  //
  // v0 ---|
  //       |--[add]-- a0 --|
  // c0 ---|               |--[add]-- o
  //                       |
  // a1 -------------------|
  //

  // consts
  TensorInfo c0Shape{"FLOAT", std::vector<int64_t>{2, 2}};
  float c0Vals[2 * 2]  = {2};
  ConstVoidData c0Data = {c0Vals, c0Shape};

  TensorInfo c1Shape{"FLOAT", std::vector<int64_t>{2, 2}};
  float c1Vals[2 * 2]  = {3};
  ConstVoidData c1Data = {c1Vals, c1Shape};

  // input
  TensorInfo inputInfo{"FLOAT", std::vector<int64_t>{2, 2}};

  // Build an onnx model
  auto builder = Builder::create();

  auto v0Id = builder->addInputTensor(inputInfo);
  auto c0Id = builder->constant(c0Data, "c0Data");
  auto c1Id = builder->constant(c1Data, "c1Data");

  auto a0 = builder->add({v0Id, c0Id}, "a0");
  auto a1 = builder->add({c0Id, c1Id}, "a1");

  auto o = builder->add({a0, a1}, "o");
  builder->addOutputTensor(o);

  auto proto      = builder->getModelProto();
  auto modelProto = io::getModelFromString(proto);

  // Create the IR, adding outId as an anchor
  auto art      = AnchorReturnType("ALL");
  auto dataFlow = DataFlow(1, {{o, art}});

  Ir ir;
  ir.prepare({modelProto,
              InputShapeInfo(),
              dataFlow,
              {}, // no loss
              {}, // no optimizer
              {}, // no SessionOptions
              Patterns({PatternType::POSTNREPL})});

  // Check that the producer of a1 Add Op is has been removed from the IR
  // by ConstExpr folding
  BOOST_CHECK(ir.opsOfType(Onnx::AiOnnx::OpSet9::Add).size() == 2);
}