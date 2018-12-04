#ifndef GUARD_NEURALNET_MULX_HPP
#define GUARD_NEURALNET_MULX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/op/reducesumx.hpp>
#include <poponnx/popx/opx.hpp>

namespace poponnx {

class MulOp;

namespace popx {

class MulOpx : public Opx {
public:
  MulOpx(Op *, Devicex *);
  MulOp *getMulOp() const;
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace poponnx

#endif