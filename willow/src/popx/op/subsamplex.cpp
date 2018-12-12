#include <popops/ElementWise.hpp>
#include <poponnx/error.hpp>
#include <poponnx/op/subsample.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/op/subsamplex.hpp>
#include <poponnx/tensor.hpp>

#include <ostream>
namespace poponnx {
namespace popx {

SubsampleOpx::SubsampleOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (!op->isConvertibleTo<SubsampleOp>()) {
    throw error("cannot create SubsampleOpx from " + op->op_type());
  }
}

void SubsampleOpx::grow(poplar::program::Sequence &prog) const {

  SubsampleOp &op = getOp<SubsampleOp>();

  auto outTensor = get(inId(0));
  int dimension  = 0;
  for (auto stride : op.strides_u32()) {
    outTensor = outTensor.subSample(stride, dimension++);
  }

  // Need to clone/copy a new output tensor so is not in place
  insert(outId(0), cloneNcopy(prog, outTensor));
}

SubsampleGradOpx::SubsampleGradOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  if (!op->isConvertibleTo<SubsampleGradOp>()) {
    throw error("cannot create SubsampleGradOpx from " + op->op_type());
  }
}

// Starting from the gradient of the output of Subsample, we iteratively expand
// the tensor by inserting zeros in the positions which were not sampled by
// Subsample.
void SubsampleGradOpx::grow(poplar::program::Sequence &prog) const {

  SubsampleGradOp &gradOp = getOp<SubsampleGradOp>();
  SubsampleOp *op         = gradOp.getFwdOp();

  std::vector<unsigned int> strides = op->strides_u32();
  Shape fwdOpInputShape             = op->inShape(0);

  auto outTensor = get(inId(0));

  // for each dimension of the input
  for (int d = 0; d < inInfo(0).rank(); ++d) {

    // The out of the shape keeps changing
    std::vector<size_t> shape = outTensor.shape();

    // Create a padding tensor the same dimension as the out shape
    auto padded = dv_p->getConst(outTensor.elementType(), shape, 0);

    // Slice the out and padding so we can concatenate them
    auto sliced_padding = padded.slice(0, 1, d);

    // First iteration
    auto interleaved = outTensor.slice(0, 1, d);
    for (int p = 1; p < strides[d]; ++p) {
      interleaved = poplar::concat(interleaved, sliced_padding, d);
    }

    // Subsequent iterations
    for (int s = 1; s < outTensor.dim(d); ++s) {

      interleaved =
          poplar::concat(interleaved, outTensor.slice(s, s + 1, d), d);
      for (int p = 1; p < strides[d]; ++p) {

        // while we are smaller than the original input to the fwd op
        if (interleaved.dim(d) < fwdOpInputShape[d]) {
          interleaved = poplar::concat(interleaved, sliced_padding, d);
        }
      }
    }

    outTensor = interleaved;
  }

  insert(outId(0), cloneNcopy(prog, outTensor));
}

} // namespace popx
} // namespace poponnx
