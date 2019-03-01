
#include <poponnx/error.hpp>
#include <poponnx/op/mean.hpp>
#include <poponnx/popx/op/meanx.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/tensorindex.hpp>

#include <popops/ElementWise.hpp>
#include <popops/Expr.hpp>
#include <popops/Reduce.hpp>

namespace pe = popops::expr;

namespace poponnx {
namespace popx {

MeanOpx::MeanOpx(Op *op, Devicex *devicex) : ElementWiseUnaryOpx(op, devicex) {
  verifyOp<MeanOp>(op, {Onnx::Operators::Mean_8, Onnx::Operators::Mean_6});
}

void MeanOpx::grow(poplar::program::Sequence &prog) const {
  auto outTensor = cloneNcopy(prog, get(inId(0)));

  if (op_p->input->n() > 1) {

    for (int i = 1; i < op_p->input->n(); ++i) {
      outTensor = popops::map(graph(),
                              popops::expr::BinaryOpType::ADD,
                              outTensor,
                              get(inId(i)),
                              prog,
                              idStr());
    }

    outTensor = popops::map(graph(),
                            pe::Divide(pe::_1, pe::Const(op_p->input->n())),
                            {outTensor},
                            prog,
                            idStr());
  }

  insert(outId(MeanOp::getOutIndex()), outTensor);
}

MeanArgGradOpx::MeanArgGradOpx(Op *op_, Devicex *devicex_)
    : Opx(op_, devicex_) {}

void MeanArgGradOpx::grow(poplar::program::Sequence &prog) const {
  auto gradOp = getOp<MeanArgGradOp>();

  auto shapeOfInputToBwdOp = inInfo(MeanArgGradOp::getGradInIndex()).shape();
  auto shapeOfInputToFwdOp = gradOp.getFwdInputInfo().shape();

  // Create the axes to reduce along.
  std::vector<int64_t> axes =
      npReductionAxis(shapeOfInputToFwdOp, shapeOfInputToBwdOp);

  // Remove axes from the result that were not present ( or 1) in the input to
  // the fwd op
  auto out = popops::reduce(graph(),
                            get(inId(MeanArgGradOp::getGradInIndex())),
                            vXtoY<int64_t, std::size_t>(axes),
                            {popops::Operation::ADD},
                            prog,
                            idStr());

  // scale the grad input (reduced)
  popops::mapInPlace(graph(),
                     pe::Mul(pe::_1, pe::Const(gradOp.getScale())),
                     {out},
                     prog,
                     idStr());

  // Reshape the output, to add 1's if needed
  insert(outId(MeanArgGradOp::getOutIndex()),
         out.reshape(outInfo(MeanArgGradOp::getOutIndex()).shape_szt()));
}

namespace {
OpxCreator<MeanOpx> meanOpxCreator({Onnx::Operators::Mean_6,
                                    Onnx::Operators::Mean_8});
OpxCreator<MeanArgGradOpx> meanGradOpxCreator(Onnx::GradOperators::MeanArgGrad);
} // namespace

} // namespace popx
} // namespace poponnx
