// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_REDUCESUMX_HPP
#define GUARD_NEURALNET_REDUCESUMX_HPP

#include <popart/names.hpp>
#include <popart/popx/opx.hpp>

namespace popart {

class ReduceSumOp;

namespace popx {

class ReduceSumOpx : public Opx {
public:
  ReduceSumOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const override;
};

class ReduceSumGradOpx : public Opx {
public:
  ReduceSumGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const override;
};

} // namespace popx
} // namespace popart

#endif
