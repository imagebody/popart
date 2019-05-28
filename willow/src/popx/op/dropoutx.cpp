#include <poprand/RandomGen.hpp>
#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/op/dropout.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/op/dropoutx.hpp>
#include <poponnx/popx/opxmanager.hpp>

namespace poponnx {
namespace popx {

DropoutOpx::DropoutOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryOpx(op, devicex) {
  verifyOp<DropoutOp>(op,
                      {Onnx::Operators::Dropout_6, Onnx::Operators::Dropout_7});

  if (dv_p->isDropoutRandomSeedRequired() == false) {
    dv_p->setDropoutRandomSeedIsRequired(true);
  }
}

void DropoutOpx::grow(poplar::program::Sequence &prog) const {
  if (op_p->getIr().canTrain()) {
    auto dropoutOp    = dynamic_cast<DropoutOp *>(op_p);
    auto seedModifier = dropoutOp->getSeedModifier();

    // Converting from poponnx standard (float) to poplar (double) for ratio
    auto ratio              = static_cast<double>(dropoutOp->getRatio());
    auto dropoutProbability = 1 - ratio;

    // If fwd dropout op, add reference tensor for layer to map.
    // If a bwd dropout op, or recomputation op, retrieve the reference
    // tensor for that layer.
    poplar::Tensor refTensor;
    if (dv_p->dropoutReferenceTensors.find(seedModifier) ==
        dv_p->dropoutReferenceTensors.end()) {
      refTensor = getInTensor(DropoutOp::getInIndex());
      dv_p->dropoutReferenceTensors.emplace(seedModifier, refTensor);
    } else {
      refTensor = dv_p->dropoutReferenceTensors.at(seedModifier);
    }

    auto dropout = poprand::dropout(graph(),
                                    dv_p->getDropoutRandomSeed(),
                                    seedModifier,
                                    getInTensor(DropoutOp::getInIndex()),
                                    refTensor,
                                    dropoutProbability,
                                    1 / dropoutProbability,
                                    prog,
                                    idStr());

    setOutTensor(dropoutOp->getOutIndex(), dropout);
  } else {
    // In inference/evaluation mode, dropout is an idendity function
    setOutTensor(DropoutOp::getOutIndex(),
                 getInTensor(DropoutOp::getInIndex()));
  }
}

namespace {
OpxCreator<DropoutOpx> dropoutOpxCreator({Onnx::Operators::Dropout_6,
                                          Onnx::Operators::Dropout_7});
OpxCreator<Opx> dropoutGradOpxCreator(Onnx::GradOperators::DropoutGrad,
                                      "DropoutGradOp should be optimised out, "
                                      "\"DropoutGradOp\" pattern is required");
} // namespace

} // namespace popx
} // namespace poponnx