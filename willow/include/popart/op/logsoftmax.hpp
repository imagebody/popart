// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_LOGSOFTMAX_HPP
#define GUARD_NEURALNET_LOGSOFTMAX_HPP

#include <popart/op/elementwise.hpp>

namespace popart {

class LogSoftmaxOp : public ElementWiseUnaryOp {
public:
  LogSoftmaxOp(const OperatorIdentifier &_opid,
               int64_t axis,
               const Op::Settings &settings_);
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  std::unique_ptr<Op> clone() const final;

  int64_t getAxis() { return axis; }

private:
  int64_t axis;
};

// Has no grad op. LogSoftmaxOp pattern converts
// op into a sequence of log and softmax ops

} // namespace popart

#endif
