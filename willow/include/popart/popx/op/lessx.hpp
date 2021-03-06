// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_LESSX_HPP
#define GUARD_NEURALNET_LESSX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/elementwisex.hpp>

namespace popart {

class LessOp;

namespace popx {

class LessOpx : public BinaryComparisonOpx {
public:
  LessOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace popart

#endif
