#ifndef GUARD_NEURALNET_MAX_HPP
#define GUARD_NEURALNET_MAX_HPP

#include <poponnx/op.hpp>

namespace poponnx {

class MaxOp : public Op {
public:
  MaxOp(const OperatorIdentifier &_opid, const Op::Settings &settings);
  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;

  // Max has a variable number of inputs
  static OutIndex getOutIndex() { return 0; }

  bool canBeReplacedByIdentity() override;
};

// A MaxGradOp will be created for each input to MaxOp i.e. it will compute the
// gradient of a single input argument
class MaxGradOp : public Op {
public:
  MaxGradOp(const MaxOp &, InIndex);

  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  static InIndex getGradInIndex() { return 0; }
  static InIndex getFwdInIndex() { return 1; }
  static InIndex getFwdOutInIndex() { return 2; }
  static OutIndex getOutIndex() { return 0; }

private:
  InIndex fwdIndex;
  std::map<int, int> gradOutToNonGradInInfo;
  std::vector<GradInOutMapper> gradInputInfoVec;
};

} // namespace poponnx

#endif