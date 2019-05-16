#include <algorithm>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/flatten.hpp>
#include <poponnx/opmanager.hpp>
#include <poponnx/opserialiser.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {

std::unique_ptr<Op>
FlattenOp::getInplaceVariant(const OperatorIdentifier &operator_id) const {
  if (operator_id == Onnx::CustomOperators::FlattenInplace) {
    return make_unique<FlattenInplaceOp>(*this);
  }
  // catch remaining cases and throw an error
  return Op::getInplaceVariant(operator_id);
}

view::RegMap FlattenBaseOp::fwdRegMap(InIndex inIndex) const {
  if (inIndex != 0) {
    throw error("Internal Logic Error in FlattenBaseOp::fwdRegMap."
                "Received input index {} but only 0 allowed, "
                "This for Op {}, ",
                inIndex,
                str());
  }
  // being conservative and returning the full region,
  // even for non-full input region :
  auto outRegion = view::Region::getFull(outInfo(getOutIndex()).shape());
  return [outRegion](const view::Region &) { return outRegion; };
}

view::RegMap FlattenBaseOp::bwdRegMap(InIndex inIndex) const {
  if (inIndex != 0) {
    throw error("Internal Logic Error in FlattenBaseOp::bwdRegMap."
                "Received input index {} but only 0 allowed, "
                "This for Op {}, ",
                inIndex,
                str());
  }
  auto inRegion = view::Region::getFull(inInfo(getInIndex()).shape());
  return [inRegion](const view::Region &) { return inRegion; };
}

FlattenBaseOp::FlattenBaseOp(const OperatorIdentifier &_opid,
                             int64_t axis_,
                             const Op::Settings &settings_)
    : Op(_opid, settings_), axis(axis_) {}

FlattenInplaceOp::FlattenInplaceOp(const FlattenOp &op)
    : FlattenBaseOp(Onnx::CustomOperators::FlattenInplace,
                    op.getAxis(),
                    op.settings) {}

std::unique_ptr<Op> FlattenInplaceOp::clone() const {
  return make_unique<FlattenInplaceOp>(*this);
}

std::unique_ptr<Op> FlattenOp::clone() const {
  return make_unique<FlattenOp>(*this);
}

FlattenInplaceOp::FlattenInplaceOp(const OperatorIdentifier &_opid,
                                   int64_t axis_,
                                   const Op::Settings &settings_)
    : FlattenBaseOp(_opid, axis_, settings_) {}

FlattenOp::FlattenOp(const OperatorIdentifier &_opid,
                     int64_t axis_,
                     const Op::Settings &settings_)
    : FlattenBaseOp(_opid, axis_, settings_) {}

void FlattenBaseOp::setup() {
  const auto in_shape = inInfo(getInIndex()).shape();
  const auto begin    = in_shape.begin();
  const auto mid      = in_shape.begin() + axis;
  const auto end      = in_shape.end();

  // The product of the first axis dimensions to flatten
  const auto m = std::accumulate(begin, mid, 1, std::multiplies<int64_t>());

  // The product of the remaining dimensions
  const auto n = std::accumulate(mid, end, 1, std::multiplies<int64_t>());

  // The "flattened" shape
  const Shape out_shape = {m, n};

  outInfo(getOutIndex()) = {inInfo(getInIndex()).data_type(), out_shape};
}

std::vector<std::unique_ptr<Op>> FlattenBaseOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> result;

  result.push_back(make_unique<FlattenGradOp>(*this));

  return result;
}

int64_t FlattenBaseOp::getAxis() const { return axis; }

void FlattenBaseOp::setAxis(int64_t value) { axis = value; }

void FlattenBaseOp::appendAttributes(OpSerialiserBase &os) const {
  Op::appendAttributes(os);
  os.appendAttribute("axis", axis);
}

FlattenGradOp::FlattenGradOp(const FlattenBaseOp &fwdOp)
    : ReshapeOp(Onnx::GradOperators::FlattenGrad,
                fwdOp.inShape(FlattenBaseOp::getInIndex()),
                fwdOp.getSettings()) {}

const std::vector<GradInOutMapper> &FlattenGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), FlattenBaseOp::getOutIndex(), GradOpInType::GRADOUT}};
  return inInfo;
}

const std::map<int, int> &FlattenGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), FlattenBaseOp::getInIndex()}};
  return outInfo;
}

view::Region FlattenInplaceOp::aliases(InIndex index) const {
  return uses(index);
}

namespace {
static std::unique_ptr<Op> flattenOpFactory(const OperatorIdentifier &_opid,
                                            const Op::Settings &settings,
                                            const Attributes &attr) {
  int64_t axis = attr.getAttribute<Attributes::Int>("axis", 1);
  return make_unique<FlattenOp>(_opid, axis, settings);
}

static std::unique_ptr<Op>
flattenInplaceOpFactory(const OperatorIdentifier &_opid,
                        const Op::Settings &settings,
                        const Attributes &attr) {
  int64_t axis = attr.getAttribute<Attributes::Int>("axis", 1);
  return make_unique<FlattenInplaceOp>(_opid, axis, settings);
}

static OpCreator<FlattenOp> flattenOpCreator({Onnx::Operators::Flatten_1,
                                              Onnx::Operators::Flatten_9},
                                             flattenOpFactory,
                                             true);

static OpCreator<FlattenOp> flattenInplaceOpCreator(
    {Onnx::CustomOperators::FlattenInplace, Onnx::Operators::Flatten_9},
    flattenInplaceOpFactory,
    true);
} // namespace

} // namespace poponnx
