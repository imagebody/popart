#include <popart/error.hpp>
#include <popart/op/equal.hpp>
#include <popart/popx/devicex.hpp>

#include <popart/popx/op/equalx.hpp>
#include <popart/popx/opxmanager.hpp>

#include <popops/ElementWise.hpp>

namespace popart {
namespace popx {

EqualOpx::EqualOpx(Op *op, Devicex *devicex)
    : BinaryComparisonOpx(op, devicex) {
  verifyOp<EqualOp>(op, {Onnx::Operators::Equal_1, Onnx::Operators::Equal_7});
}

void EqualOpx::grow(poplar::program::Sequence &prog) const {

  insert(outId(EqualOp::getOutIndex()),
         popops::map(graph(),
                     popops::expr::BinaryOpType::EQUAL,
                     get(inId(EqualOp::getArg0InIndex())),
                     get(inId(EqualOp::getArg1InIndex())),
                     prog,
                     idStr()));
}

namespace {

OpxCreator<EqualOpx> greaterOpxCreator_7(Onnx::Operators::Equal_1);
OpxCreator<EqualOpx> greaterOpxCreator_9(Onnx::Operators::Equal_7);

} // namespace

} // namespace popx
} // namespace popart
