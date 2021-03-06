// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_GATHER_HPP
#define GUARD_NEURALNET_GATHER_HPP

#include <popart/op.hpp>

namespace popart {

class GatherOp : public Op {
public:
  GatherOp(const OperatorIdentifier &_opid,
           int64_t axis_,
           const Op::Settings &settings_);

  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() override;
  void setup() final;

  // Which axis to gather on.
  int64_t getAxis() const;

  static InIndex dataInIndex() { return 0; }
  static InIndex indicesInIndex() { return 1; }
  static InIndex outIndex() { return 0; }

  void appendOutlineAttributes(OpSerialiserBase &) const override;

  bool canBeReplacedByIdentity() override;

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

private:
  int64_t axis = 0;
};

class GatherGradOp : public Op {
public:
  GatherGradOp(const GatherOp &op, int64_t axis);

  std::unique_ptr<Op> clone() const override;
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  // Which axis to gather on.
  int64_t getAxis() const;

  static InIndex gradInIndex() { return 0; }
  static InIndex indicesInIndex() { return 1; }
  static InIndex gradOutIndex() { return 0; }

  void appendOutlineAttributes(OpSerialiserBase &) const override;

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

private:
  int64_t axis;
  TensorInfo fwdDataInfo;
};

} // namespace popart

#endif
