// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_CONSTEXPRS_ELEMENTWISECE_HPP
#define GUARD_NEURALNET_CONSTEXPRS_ELEMENTWISECE_HPP

#include <popart/ces/constexpr.hpp>

namespace popart {

class ConstExprAdd : public ConstExprOp {
public:
  ConstExprAdd(Op *op);
  std::vector<char> compute() final;
};

class ConstExprDiv : public ConstExprOp {
public:
  ConstExprDiv(Op *op);
  std::vector<char> compute() final;
};

class ConstExprMul : public ConstExprOp {
public:
  ConstExprMul(Op *op);
  std::vector<char> compute() final;
};

class ConstExprSub : public ConstExprOp {
public:
  ConstExprSub(Op *op);
  std::vector<char> compute() final;
};

class ConstExprMod : public ConstExprOp {
public:
  ConstExprMod(Op *op);
  std::vector<char> compute() final;
};

} // namespace popart

#endif
