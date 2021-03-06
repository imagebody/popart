// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_DYNAMICSLICEX_HPP
#define GUARD_NEURALNET_DYNAMICSLICEX_HPP

#include <popart/names.hpp>
#include <popart/popx/opx.hpp>

namespace popart {

class DynamicSliceOp;

namespace popx {

class DynamicSliceOpx : public Opx {
public:
  DynamicSliceOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
  InputCreatorType getInputCreatorType(InIndex index) const final;
  poplar::Tensor
      unwindTensorLayout(poplar::Tensor, InIndex, OutIndex) const final;
  view::RegMap unwindRegion(InIndex, OutIndex) const final;
  poplar::Tensor createInput(InIndex index,
                             const std::string &name) const final;
  std::vector<TensorId> mustExistBeforeCreate(InIndex) const final {
    return {};
  }

protected:
  poplar::Tensor getTiledTensor(poplar::Tensor tensor) const;
};

} // namespace popx
} // namespace popart

#endif
