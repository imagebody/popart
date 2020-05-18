// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <memory>
#include <popart/op/pad.hpp>
#include <popart/op/slice.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/region.hpp>
#include <popart/tensor.hpp>

namespace popart {

Slice::Slice(int64_t start_, int64_t end_, int64_t axis_)
    : start(start_), end(end_), axis(axis_) {}

TensorInfo BaseSliceOp::createOutInfo() const {
  auto in_info      = inInfo(getInIndex());
  auto output_shape = in_info.shape();

  for (auto slice : getSlices()) {
    auto new_size            = slice.end - slice.start;
    output_shape[slice.axis] = new_size;
  }

  return {in_info.dataType(), output_shape};
}

std::vector<Slice> BaseSliceOp::getSlices() const {
  return getSlices(inInfo(getInIndex()).shape());
}

view::Region BaseSliceOp::createSlicedRegion(const Shape &toBeSliced) const {
  // if there was NO slicing, the Region bounds would be,
  view::LowBounds lbounds(toBeSliced.size(), 0);
  view::UppBounds ubounds = toBeSliced;

  // but this slice tightens these Region bounds along certain axes
  for (auto slice : getSlices(toBeSliced)) {
    lbounds[slice.axis] = slice.start;
    ubounds[slice.axis] = slice.end;
  }

  return {lbounds, ubounds};
}

view::Region BaseSliceOp::getFullInRegion() const {
  // the Region of the input tensor which is sliced
  return createSlicedRegion(inShape(getInIndex()));
}

view::RegMap BaseSliceOp::fwdRegMap(InIndex inIndex, OutIndex outIndex) const {
  if (inIndex != 0 || outIndex != 0) {
    throw internal_error(
        "[BaseSliceOp::fwdRegMap] Received input index {} but only 0 allowed, "
        "This for Op {}, ",
        inIndex,
        str());
  }
  auto fullInRegion = getFullInRegion();

  return [fullInRegion](const view::Region &r) {
    // (1) get intersection with maximal input region
    auto inRegion = r.intersect(fullInRegion);
    // (2) map to the output region by subtracting lower of maximal region
    view::LowBounds out_lb = inRegion.getLower();
    view::UppBounds out_ub = inRegion.getUpper();
    for (int i = 0; i < inRegion.rank(); ++i) {
      out_lb[i] -= fullInRegion.getLower()[i];
      out_ub[i] -= fullInRegion.getLower()[i];
    }
    return view::Regions(1, view::Region(out_lb, out_ub));
  };
}

view::Regions BaseSliceOp::uses(InIndex inIndex) const {
  if (inIndex != 0) {
    throw internal_error(
        "[BaseSliceOp::uses] "
        "BaseSliceOp has has input index {}, but only 0 permitted. "
        "This for op ",
        inIndex,
        str());
  }
  return {getFullInRegion()};
}

std::unique_ptr<Op>
SliceOp::getInplaceVariant(const OperatorIdentifier &operator_id) const {

  if (operator_id == Onnx::CustomOperators::SliceInplace) {
    return std::make_unique<SliceInplaceOp>(*this);
  }

  // catch remaining cases and throw an error
  return Op::getInplaceVariant(operator_id);
}

view::Region BaseSliceOp::getFullOutRegion() const {

  // sanity check : the region below is the same
  // as that computed using fwdRegMap:
  //  >>    fwdRegMap(0)(view::Region::getFull(inShape(getInIndex())));
  // This has been confirmed.

  return view::Region::getFull(outShape(getOutIndex()));
}

view::RegMap BaseSliceOp::bwdRegMap(InIndex inIndex, OutIndex outIndex) const {
  if (inIndex != 0 || outIndex != 0) {
    throw internal_error("[BaseSliceOp::bwdRegMap] "
                         "Received input index {} but only 0 allowed. "
                         "This for Op {}. ",
                         inIndex,
                         str());
  }

  auto fullOutRegion = getFullOutRegion();
  auto fullInRegion  = getFullInRegion();

  return [fullInRegion, fullOutRegion](const view::Region &r) {
    auto outRegion        = r.intersect(fullOutRegion);
    view::LowBounds in_lb = outRegion.getLower();
    view::UppBounds in_ub = outRegion.getUpper();
    for (int i = 0; i < outRegion.rank(); ++i) {
      in_lb[i] += fullInRegion.getLower()[i];
      in_ub[i] += fullInRegion.getLower()[i];
    }
    return view::Regions(1, view::Region(in_lb, in_ub));
  };
}

std::vector<Slice>
BaseSliceOp::getSlices(std::vector<int64_t> input_shape) const {

  std::vector<Slice> slices;
  slices.reserve(axes.size());

  for (int i = 0; i < axes.size(); i++) {
    auto axis = axes[i];

    if (axis >= input_shape.size()) {
      throw error("Invalid input shape in BaseSliceOp::getSlices. "
                  "The input shape has rank {}, but axis = {}. "
                  "axis must be less than the input shape's rank. "
                  "This error is for Op {}.",
                  input_shape.size(),
                  axis,
                  str());
    }

    auto dim_size = input_shape[axis];
    auto begin    = normalizeIndex(starts[i], dim_size);
    auto end      = normalizeIndex(ends[i], dim_size);

    if (begin > end) {
      throw error("BaseSliceOp::getSlices: begin = {} and end = {}. "
                  "The input was starts[{}] = {}, end [{}] = {}. "
                  "This error for Op {}",
                  begin,
                  end,
                  i,
                  starts[i],
                  i,
                  ends[i],
                  str());
    }

    slices.emplace_back(begin, end, axis);
  }

  return slices;
}

// In the ONNX Slice Implerator
// If `index > dim_size` it is treated as `index == dim_size`
// and negative indexing is also supported.
int64_t BaseSliceOp::normalizeIndex(int64_t index, int64_t dim_size) const {
  index = std::min(index, dim_size);

  if (index < 0) {
    if (dim_size + index < 0) {
      throw error("index {} is out of bounds for axis with size {}. "
                  "This error for Op {} in BaseSliceOp::normalizeIndex",
                  index,
                  dim_size,
                  str());
    }

    index = dim_size + index;
  }

  return index;
}

BaseSliceOp::BaseSliceOp(const OperatorIdentifier &_opid,
                         const std::vector<int64_t> &starts_,
                         const std::vector<int64_t> &ends_,
                         const std::vector<int64_t> &axes_,
                         const Op::Settings &settings_)

    : Op(_opid, settings_), starts(starts_), ends(ends_),
      axes(sanitizeAxes(starts_, axes_)) {}

SliceOp::SliceOp(const OperatorIdentifier &_opid,
                 const std::vector<int64_t> &starts_,
                 const std::vector<int64_t> &ends_,
                 const std::vector<int64_t> &axes_,
                 const Op::Settings &settings_)
    : BaseSliceOp(_opid, starts_, ends_, axes_, settings_) {}

SliceInplaceOp::SliceInplaceOp(const OperatorIdentifier &_opid,
                               const std::vector<int64_t> &starts_,
                               const std::vector<int64_t> &ends_,
                               const std::vector<int64_t> &axes_,
                               const Op::Settings &settings_)
    : BaseSliceOp(_opid, starts_, ends_, axes_, settings_) {}

std::vector<int64_t>
BaseSliceOp::sanitizeAxes(const std::vector<int64_t> &starts,
                          std::vector<int64_t> axes) {
  if (axes.size() == 0) {
    for (int i = 0; i < starts.size(); i++) {
      axes.push_back(i);
    }
  }
  return axes;
}

SliceInplaceOp::SliceInplaceOp(const SliceOp &op)
    : BaseSliceOp(Onnx::CustomOperators::SliceInplace,
                  op.getStarts(),
                  op.getEnds(),
                  op.getAxes(),
                  op.getSettings()) {
  unwindConcatDim = op.unwindConcatDim;
}

std::unique_ptr<Op> SliceOp::clone() const {
  return std::make_unique<SliceOp>(*this);
}

std::unique_ptr<Op> SliceInplaceOp::clone() const {
  return std::make_unique<SliceInplaceOp>(*this);
}

std::vector<std::tuple<OperatorIdentifier, float>>
SliceOp::inplacePriorityDefault() const {
  // see T6768: choosing default priorities
  return {{Onnx::CustomOperators::SliceInplace, 10.0f}};
}

std::vector<std::unique_ptr<Op>> SliceOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<SliceGradOp>(*this));
  return upops;
}

void BaseSliceOp::connectInTensor(InIndex inIndex, TensorId tenId) {
  if (inIndex == getInIndex()) {
    Op::connectInTensor(inIndex, tenId);
  }

  if (opid.version >= 10) {
    if (inIndex == getStartsInIndex()) {
      try {
        getInTensorData(tenId, starts, {DataType::INT32, DataType::INT64});
      } catch (popart::error &err) {
        throw error("Need the value of the {} input 'starts' to detemine the "
                    "output shape, but was unable because {}",
                    opid,
                    err.what());
      }
      axes = sanitizeAxes(starts, {});
    } else if (inIndex == getEndsInIndex()) {
      try {
        getInTensorData(tenId, ends, {DataType::INT32, DataType::INT64});
      } catch (popart::error &err) {
        throw error("Need the value of the {} input 'ends' to detemine the "
                    "output shape, but was unable because {}",
                    opid,
                    err.what());
      }
    } else if (inIndex == getAxesInIndex()) {
      try {
        std::vector<int64_t> _axes;
        getInTensorData(tenId, _axes, {DataType::INT32, DataType::INT64});
        axes = sanitizeAxes(starts, _axes);
      } catch (popart::error &err) {
        throw error("Need the value of the {} input 'axes' to detemine the "
                    "output shape, but was unable because {}",
                    opid,
                    err.what());
      }
    }
  }
}

void BaseSliceOp::setup() {
  outInfo(getOutIndex()) = createOutInfo();
  // TODO : check that shapes agree T9582
}

std::vector<std::unique_ptr<Op>> SliceInplaceOp::getGradOps() {
  throw internal_error(
      "[SliceInplaceOp::getGradOps] "
      "All gradients should be generated before any inplacing is performed. "
      "This for Op {}",
      str());
}

view::Regions SliceInplaceOp::aliases(InIndex in, OutIndex out) const {
  if (in != 0) {
    throw internal_error("[SliceInplaceOp::aliases] "
                         "BaseSliceOp has no input index {}, only 0 permitted. "
                         "This for Op {}",
                         in,
                         str());
  }
  return bwdRegMap(in, out)(view::Region::getFull(outShape(out)));
}

void BaseSliceOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);

  if (opid.version < 10) {
    os.appendAttribute("starts", starts);
    os.appendAttribute("ends", ends);
    os.appendAttribute("axes", axes);
  } else {
    os.appendAttribute("_starts", starts);
    os.appendAttribute("_ends", ends);
    os.appendAttribute("_axes", axes);
  }
}

SliceGradOp::SliceGradOp(const SliceOp &op_)
    : Op(Onnx::GradOperators::SliceGrad, op_.getSettings()),
      slices(op_.getSlices()),
      preSlicedInInfo(op_.inInfo(SliceOp::getInIndex())) {
  setPadding(op_);
}

std::unique_ptr<Op> SliceGradOp::clone() const {
  return std::make_unique<SliceGradOp>(*this);
}

void SliceGradOp::setup() { outInfo(getOutIndex()) = preSlicedInInfo; }

void SliceGradOp::setPadding(const SliceOp &slice_op) {
  auto t_rank   = slice_op.inInfo(SliceOp::getInIndex()).rank();
  auto in_shape = slice_op.inInfo(SliceOp::getInIndex()).shape();
  std::vector<int64_t> pads(t_rank * 2, 0);

  for (auto slice : slice_op.getSlices()) {
    pads[slice.axis]          = slice.start;
    pads[slice.axis + t_rank] = in_shape[slice.axis] - slice.end;
  }

  for (int i = 0; i < t_rank; i++) {
    lower_padding.push_back(pads[i]);
    upper_padding.push_back(pads[t_rank + i]);
  }
}

const std::vector<GradInOutMapper> &SliceGradOp::gradInputInfo() const {
  static const std::vector<GradInOutMapper> inInfo = {
      {getInIndex(), SliceOp::getOutIndex(), GradOpInType::GradOut}};
  return inInfo;
}

const std::map<int, int> &SliceGradOp::gradOutToNonGradIn() const {
  static const std::map<int, int> outInfo = {
      {getOutIndex(), SliceOp::getInIndex()}};
  return outInfo;
}

void SliceGradOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);

  std::vector<int64_t> starts(slices.size());
  std::vector<int64_t> ends(slices.size());
  std::vector<int64_t> axes(slices.size());

  for (size_t i = 0; i < slices.size(); ++i) {
    starts[i] = slices[i].start;
    ends[i]   = slices[i].end;
    axes[i]   = slices[i].axis;
  }

  os.appendAttribute("_starts", starts);
  os.appendAttribute("_ends", ends);
  os.appendAttribute("_axes", axes);
  os.appendAttribute("_lower_padding", lower_padding);
  os.appendAttribute("_upper_padding", upper_padding);
}

namespace {

static OpDefinition::DataTypes T    = {DataType::UINT8,
                                    DataType::UINT16,
                                    DataType::UINT32,
                                    DataType::UINT64,
                                    DataType::INT8,
                                    DataType::INT16,
                                    DataType::INT32,
                                    DataType::INT64,
                                    DataType::FLOAT16,
                                    DataType::FLOAT,
                                    DataType::BOOL};
static OpDefinition::DataTypes Tind = {DataType::INT32, DataType::INT64};

static OpDefinition
    sliceOpV1Def({OpDefinition::Inputs({{"data", T}}),
                  OpDefinition::Outputs({{"output", T}}),
                  OpDefinition::Attributes(
                      {{"axes", {"*"}}, {"ends", {"*"}}, {"starts", {"*"}}})});

static OpDefinition sliceOpDef({OpDefinition::Inputs({
                                    {"data", T},
                                    {"starts", Tind, true},
                                    {"ends", Tind, true},
                                    {"axes", Tind, true},
                                    // Not supported
                                    //{"steps", Tind true} },
                                }),
                                OpDefinition::Outputs({{"output", T}}),
                                OpDefinition::Attributes({})});

static OpCreator<SliceOp> sliceOpCreator(
    OpDefinitions({{Onnx::Operators::Slice_1, sliceOpV1Def},
                   {Onnx::Operators::Slice_10, sliceOpDef},
                   {Onnx::Operators::Slice_11, sliceOpDef}}),
    [](const OperatorIdentifier &_opid,
       const Op::Settings &settings,
       const Attributes &attr) -> std::unique_ptr<Op> {
      if (_opid.version < 10) {
        // Before version 10 the slice parameters were attributes
        std::vector<int64_t> starts =
            attr.getAttribute<Attributes::Ints>("starts", {});
        std::vector<int64_t> ends =
            attr.getAttribute<Attributes::Ints>("ends", {});
        std::vector<int64_t> axes =
            attr.getAttribute<Attributes::Ints>("axes", {});

        return std::unique_ptr<Op>(
            new SliceOp(_opid, starts, ends, axes, settings));
      } else {
        // Slice parameters are now inputs
        return std::unique_ptr<Op>(new SliceOp(_opid, {}, {}, {}, settings));
      }
    },
    true);
} // namespace

} // namespace popart
