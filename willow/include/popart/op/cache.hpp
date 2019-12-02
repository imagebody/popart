#ifndef GUARD_NEURALNET_CACHE_HPP
#define GUARD_NEURALNET_CACHE_HPP

#include <popart/op.hpp>
#include <popart/op/elementwise.hpp>

namespace popart {

class CacheStoreOp : public Op {
public:
  CacheStoreOp(const OperatorIdentifier &, const Op::Settings &);

  std::unique_ptr<Op> clone() const final;
  void setup() final {}

  static InIndex getRemoteBufferOffsetInIndex() { return 1; }
  static InIndex getCachedTensorInIndex() { return 0; }

  float getSubgraphValue() const final { return getHighSubgraphValue(); }

  void setRemoteBufferId(RemoteBufferId remotebuffer_id_) {
    remotebuffer_id = remotebuffer_id_;
  }
  RemoteBufferId getRemoteBufferId() const { return remotebuffer_id; }

private:
  RemoteBufferId remotebuffer_id;
};

class CacheAllocateOp : public Op {
public:
  CacheAllocateOp(const OperatorIdentifier &,
                  const TensorInfo &,
                  const Op::Settings &);

  std::unique_ptr<Op> clone() const final;
  void setup() final;

  static InIndex getCachedTensorOutIndex() { return 0; }

  TensorInfo getTensorInfo() { return tensor_info; }

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

  bool isOutlineable() const final { return false; }

private:
  TensorInfo tensor_info;
};

class CacheLoadOp : public Op {
public:
  CacheLoadOp(const OperatorIdentifier &,
              const TensorInfo &,
              const Op::Settings &);

  std::unique_ptr<Op> clone() const final;
  void setup() final;

  static InIndex getRemoteBufferOffsetInIndex() { return 1; }
  static OutIndex getCachedTensorInIndex() { return 0; }
  static OutIndex getCachedTensorOutIndex() { return 0; }

  view::Regions modifies(InIndex) const final;
  view::Regions aliases(InIndex, OutIndex) const final;

  view::RegMap fwdRegMap(InIndex, OutIndex) const final;
  view::RegMap bwdRegMap(InIndex, OutIndex) const final;

  TensorInfo getTensorInfo() { return tensor_info; }

  float getSubgraphValue() const final { return getHighSubgraphValue(); }

  void setRemoteBufferId(RemoteBufferId remotebuffer_id_) {
    remotebuffer_id = remotebuffer_id_;
  }
  RemoteBufferId getRemoteBufferId() const { return remotebuffer_id; }

private:
  RemoteBufferId remotebuffer_id;
  TensorInfo tensor_info;
};

} // namespace popart

#endif