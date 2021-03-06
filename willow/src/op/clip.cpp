// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <memory>
#include <popart/op/clip.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

std::vector<std::tuple<OperatorIdentifier, float>>
ClipOp::inplacePriorityDefault() const {
  // see T6768: choosing default inplace priorities
  return {{Onnx::CustomOperators::ClipInplace, 10}};
}

ClipInplaceOp::ClipInplaceOp(const ClipOp &clip_op)
    : ElementWiseInplaceUnaryOp(Onnx::CustomOperators::ClipInplace,
                                clip_op.getSettings()),
      min(clip_op.getClipMin()), max(clip_op.getClipMax()) {}

std::unique_ptr<Op>
ClipOp::getInplaceVariant(const OperatorIdentifier &operator_id) const {
  if (operator_id == Onnx::CustomOperators::ClipInplace) {
    return std::make_unique<ClipInplaceOp>(*this);
  }
  // catch remaining cases and throw an error
  return Op::getInplaceVariant(operator_id);
}

ClipOp::ClipOp(const OperatorIdentifier &_opid,
               float min_,
               float max_,
               const Op::Settings &settings_)
    : ElementWiseUnaryOp(_opid, settings_), min(min_), max(max_) {}

std::unique_ptr<Op> ClipOp::clone() const {
  return std::make_unique<ClipOp>(*this);
}

std::vector<std::unique_ptr<Op>> ClipOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<ClipGradOp>(*this));
  return upops;
}

float ClipOp::getClipMin() const { return min; }
float ClipOp::getClipMax() const { return max; }
float ClipInplaceOp::getClipMin() const { return min; }
float ClipInplaceOp::getClipMax() const { return max; }

void ClipOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);
  os.appendAttribute("min", min);
  os.appendAttribute("max", max);
}

void ClipInplaceOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);
  os.appendAttribute("min", min);
  os.appendAttribute("max", max);
}

std::unique_ptr<Op> ClipInplaceOp::clone() const {
  return std::make_unique<ClipInplaceOp>(*this);
}

// A clip op with a clipping range of min and max numbers than
// can be expresses as a float can be replaced by identity
bool ClipOp::canBeReplacedByIdentity() {
  if (getClipMin() > std::numeric_limits<float>::lowest()) {
    return false;
  } else if (getClipMax() < std::numeric_limits<float>::max()) {
    return false;
  }
  return true;
}

ClipGradOp::ClipGradOp(const ClipOp &fwdOp)
    : ClipOp(Onnx::GradOperators::ClipGrad,
             fwdOp.getClipMin(),
             fwdOp.getClipMax(),
             fwdOp.getSettings()) {}

std::unique_ptr<Op> ClipGradOp::clone() const {
  return std::make_unique<ClipGradOp>(*this);
}

const std::vector<GradInOutMapper> &ClipGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getGradClippedInIndex(), ClipOp::getOutIndex(), GradOpInType::GradOut},
      {getClippedInIndex(), ClipOp::getOutIndex(), GradOpInType::Out}};
  return inInfo;
}

const std::map<int, int> &ClipGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), ClipOp::getInIndex()}};
  return outInfo;
}

static OpDefinition::DataTypes T = {DataType::FLOAT16, DataType::FLOAT};

static OpDefinition
    clipOpV6Def({OpDefinition::Inputs({{"input", T}}),
                 OpDefinition::Outputs({{"output", T}}),
                 OpDefinition::Attributes({{"min", {"*"}}, {"max", {"*"}}})});

static OpDefinition
    clipOpV11Def({OpDefinition::Inputs({{"input", T}, {"min", T}, {"max", T}}),
                  OpDefinition::Outputs({{"output", T}}),
                  OpDefinition::Attributes({})});

namespace {
static OpCreator<ClipOp> clipOpCreator(
    OpDefinitions({{Onnx::Operators::Clip_6, clipOpV6Def},
                   {Onnx::Operators::Clip_11, clipOpV11Def}}),
    [](const OperatorIdentifier &_opid,
       const Op::Settings &settings,
       const Attributes &attr) -> std::unique_ptr<Op> {
      float min = attr.getAttribute<Attributes::Float>(
          "min", std::numeric_limits<float>::lowest());
      float max = attr.getAttribute<Attributes::Float>(
          "max", std::numeric_limits<float>::max());

      return std::unique_ptr<Op>(new ClipOp(_opid, min, max, settings));
    },
    true);

} // namespace
} // namespace popart
