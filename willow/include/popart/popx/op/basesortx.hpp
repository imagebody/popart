// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_BASESORTX_HPP
#define GUARD_NEURALNET_BASESORTX_HPP

#include <popart/names.hpp>
#include <popart/popx/opx.hpp>

namespace popart {

namespace popx {

struct FullSortResult {

  FullSortResult(poplar::Tensor indices_, poplar::Tensor values_, int axis_)
      : indices(indices_), values(values_), axis(axis_) {}

  poplar::Tensor indices;
  poplar::Tensor values;
  int axis;
};

class BaseSortOpx : public Opx {
public:
  BaseSortOpx(Op *, Devicex *);

  poplar::Tensor createInput(InIndex index,
                             const std::string &name) const final;
  InputCreatorType getInputCreatorType(InIndex index) const final;
  std::vector<TensorId> mustExistBeforeCreate(InIndex index0) const final;

protected:
  // sorted values, and indices of sorted values
  FullSortResult growFullSortResult(poplar::program::Sequence &prog) const;

  // indices of sorted values
  poplar::Tensor growIndicesSort(poplar::program::Sequence &prog) const;

  // axis to sort on
  unsigned axis;

private:
  poplar::Tensor getIotaTensor(poplar::program::Sequence &prog) const;
};

} // namespace popx
} // namespace popart

#endif
