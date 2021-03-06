// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <iterator>
#include <vector>
#include <popart/error.hpp>
#include <popart/graph.hpp>
#include <popart/op/sinh.hpp>
#include <popart/popx/op/sinhx.hpp>
#include <popart/popx/opxmanager.hpp>
#include <popart/tensorindex.hpp>

#include <popops/ElementWise.hpp>

namespace pe = popops::expr;

namespace popart {
namespace popx {

SinhInplaceOpx::SinhInplaceOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryInplaceOpx(op, devicex, SinhComputex::get()) {
  verifyOp<SinhInplaceOp>(op, Onnx::CustomOperators::SinhInplace);
}

SinhOpx::SinhOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryOutplaceOpx(op, devicex, SinhComputex::get()) {
  verifyOp<SinhOp>(op, Onnx::Operators::Sinh_9);
}

poplar::Tensor SinhComputex::outplace(poplar::program::Sequence &p,
                                      poplar::Graph &g,
                                      const poplar::Tensor &t,
                                      const std::string &s) const {
  auto outTensor = cloneNcopy(p, g, t);
  inplace(p, g, outTensor, s);
  return outTensor;
}

void SinhComputex::inplace(poplar::program::Sequence &p,
                           poplar::Graph &g,
                           const poplar::Tensor &t,
                           const std::string &s) const {

  std::vector<std::unique_ptr<popops::expr::Expr>> exprs;
  exprs.push_back(
      std::make_unique<pe::Divide>(pe::Const(1.0f), pe::Exp(pe::_1)));
  exprs.push_back(std::make_unique<pe::Sub>(pe::Exp(pe::_1), *exprs.back()));
  exprs.push_back(std::make_unique<pe::Mul>(pe::Const(0.5f), *exprs.back()));

  // apply the inplace SINH
  popops::mapInPlace(g, *exprs.back(), {t}, p, s);
}

SinhGradOpx::SinhGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<SinhGradOp>(op, Onnx::GradOperators::SinhGrad);
}

void SinhGradOpx::grow(poplar::program::Sequence &prog) const {
  auto op              = getOp<SinhGradOp>();
  const auto input     = getInTensor(SinhGradOp::getGradInIndex());
  const auto fwd_input = getInTensor(SinhGradOp::getFwdArgInIndex());

  std::vector<std::unique_ptr<popops::expr::Expr>> exprs;
  exprs.push_back(
      std::make_unique<pe::Divide>(pe::Const(1.0f), pe::Exp(pe::_2)));
  exprs.push_back(std::make_unique<pe::Add>(pe::Exp(pe::_2), *exprs.back()));
  exprs.push_back(std::make_unique<pe::Mul>(pe::Const(0.5f), *exprs.back()));
  exprs.push_back(std::make_unique<pe::Mul>(pe::_1, *exprs.back()));

  auto output = popops::map(graph(),
                            *exprs.back(),
                            {input, fwd_input},
                            prog,
                            debugPrefix("output_grad"));

  setOutTensor(SinhGradOp::getOutIndex(), output);
}

namespace {
OpxCreator<SinhOpx> sinhOpxCreator(Onnx::Operators::Sinh_9);
OpxCreator<SinhInplaceOpx>
    sinhInplaceOpxCreator(Onnx::CustomOperators::SinhInplace);
OpxCreator<SinhGradOpx> sinhGradOpxCreator(Onnx::GradOperators::SinhGrad);
} // namespace

} // namespace popx
} // namespace popart
