// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#include <popops/ElementWise.hpp>
#include <popart/error.hpp>
#include <popart/op/exp.hpp>
#include <popart/popx/op/expx.hpp>
#include <popart/popx/opxmanager.hpp>

namespace popart {
namespace popx {

ExpInplaceOpx::ExpInplaceOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryInplaceOpx(op, devicex, ExpComputex::get()) {
  verifyOp<ExpInplaceOp>(op, Onnx::CustomOperators::ExpInplace);
}

ExpOpx::ExpOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryOutplaceOpx(op, devicex, ExpComputex::get()) {
  verifyOp<ExpOp>(op, Onnx::Operators::Exp_6);
}

poplar::Tensor ExpComputex::outplace(poplar::program::Sequence &p,
                                     poplar::Graph &g,
                                     const poplar::Tensor &t,
                                     const std::string &dbs) const {

  return popops::map(g, popops::expr::UnaryOpType::EXPONENT, t, p, dbs);
}

void ExpComputex::inplace(poplar::program::Sequence &p,
                          poplar::Graph &g,
                          const poplar::Tensor &t,
                          const std::string &dbs) const {

  popops::mapInPlace(g, popops::expr::UnaryOpType::EXPONENT, t, p, dbs);
}

namespace {
OpxCreator<ExpOpx> expOpxCreator(Onnx::Operators::Exp_6);
OpxCreator<ExpInplaceOpx>
    expxInplaceOpxCreator(Onnx::CustomOperators::ExpInplace);
} // namespace

} // namespace popx
} // namespace popart
