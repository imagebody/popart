#ifndef GUARD_NEURALNET_IFX_HPP
#define GUARD_NEURALNET_IFX_HPP

#include <poponnx/popx/opx.hpp>

namespace poponnx {

namespace popx {

class IfOpx : public Opx {
public:
  IfOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;

private:
  void copyInputs(poplar::program::Sequence &prog,
                  const Graph &graph,
                  const std::vector<TensorId> &input_ids) const;

  void copyOutputs(poplar::program::Sequence &prog,
                   const Graph &graph,
                   const std::vector<TensorId> &output_ids,
                   const std::vector<poplar::Tensor> &outputs) const;

  void callBranch(poplar::program::Sequence &, const Graph &) const;
  std::vector<poplar::Tensor> prepareOutputs() const;
};

} // namespace popx
} // namespace poponnx

#endif
