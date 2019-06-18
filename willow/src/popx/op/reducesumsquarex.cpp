#include <algorithm>
#include <iterator>
#include <vector>

#include <poponnx/error.hpp>
#include <poponnx/op/reducesumsquare.hpp>
#include <poponnx/popx/op/reducesumsquarex.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/util.hpp>

#include <popops/ElementWise.hpp>
#include <popops/Reduce.hpp>

namespace pe = popops::expr;

namespace poponnx {
namespace popx {

ReduceSumSquareOpx::ReduceSumSquareOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  verifyOp<ReduceSumSquareOp>(op);
}

void ReduceSumSquareOpx::grow(poplar::program::Sequence &prog) const {
  const auto op    = getOp<ReduceSumSquareOp>();
  const auto input = getInTensor(ReduceSumSquareOp::getInIndex());

  auto output_tensor = popops::reduce(graph(),
                                      input,
                                      vector_cast<std::size_t>(op.getAxes()),
                                      {popops::Operation::SQUARE_ADD},
                                      prog);

  setOutTensor(ReduceSumSquareOp::getOutIndex(),
               output_tensor.reshape(
                   outInfo(ReduceSumSquareOp::getOutIndex()).shape_szt()));
}

ReduceSumSquareGradOpx::ReduceSumSquareGradOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  verifyOp<ReduceSumSquareGradOp>(op, Onnx::GradOperators::ReduceSumSquareGrad);
}

void ReduceSumSquareGradOpx::grow(poplar::program::Sequence &prog) const {
  const auto op = getOp<ReduceSumSquareGradOp>();
  auto output =
      cloneNcopy(prog, getInTensor(ReduceSumSquareGradOp::getOutIndex()));
  auto input_shape     = inShape(ReduceSumSquareGradOp::getInIndex());
  auto output_shape    = outShape(ReduceSumSquareGradOp::getOutIndex());
  const auto new_shape = vector_cast<std::size_t>(op.backwardShape());

  output = output.reshape(new_shape);

  // Broadcasting across each dimension
  for (int dim = 0; dim < new_shape.size(); ++dim) {
    if (new_shape[dim] != output_shape[dim]) {
      output = output.broadcast(static_cast<uint32_t>(output_shape[dim]), dim);
    }
  }

  output = popops::map(
      graph(),
      pe::Mul(pe::Mul(pe::_1, pe::_2), pe::Const(2)),
      {output, getInTensor(ReduceSumSquareGradOp::getFwdInInIndex())},
      prog);

  // output now matches the shape of output_shape
  setOutTensor(ReduceSumSquareGradOp::getOutIndex(), output);
}

namespace {
OpxCreator<ReduceSumSquareOpx>
    reduceSumSquareOpxCreator(Onnx::Operators::ReduceSumSquare_1);
OpxCreator<ReduceSumSquareGradOpx>
    reduceSumSquareGradGradOpxCreator(Onnx::GradOperators::ReduceSumSquareGrad);
} // namespace

} // namespace popx
} // namespace poponnx