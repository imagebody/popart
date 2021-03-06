// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <popart/error.hpp>
#include <popart/graph.hpp>
#include <popart/ir.hpp>
#include <popart/names.hpp>
#include <popart/op.hpp>
#include <popart/op/cache.hpp>
#include <popart/op/call.hpp>
#include <popart/op/ipucopy.hpp>
#include <popart/tensor.hpp>
#include <popart/tensors.hpp>
#include <popart/topocons.hpp>
#include <popart/transforms/cachesetup.hpp>

namespace popart {

std::size_t CacheSetup::id() { return typeid(CacheSetup).hash_code(); }

bool CacheSetup::apply(Graph &graph) const {
  logging::debug("[CacheSetup] Started.");

  Ir &ir                 = graph.getIr();
  int64_t remoteBufferId = 0;

  // Create remote buffer info for CacheLoad/CacheStore ops with set buffer ID
  for (Op *op : ir.getAllOps()) {
    if (CacheLoadOp *loadOp = dynamic_cast<CacheLoadOp *>(op)) {
      auto allRemoteBufferIds = ir.getAllRemoteBufferInfos();
      RemoteBufferId id       = loadOp->getRemoteBufferId();
      if (id > -1 && allRemoteBufferIds.find(id) == allRemoteBufferIds.end()) {
        auto info = RemoteBufferInfo(
            loadOp->output->tensor(CacheLoadOp::getCachedTensorOutIndex())
                ->info,
            1);
        ir.setRemoteBufferInfo(id, info);
        remoteBufferId = std::max(remoteBufferId, id + 1);
      }
    }
    if (CacheStoreOp *storeOp = dynamic_cast<CacheStoreOp *>(op)) {
      auto allRemoteBufferIds = ir.getAllRemoteBufferInfos();
      RemoteBufferId id       = storeOp->getRemoteBufferId();
      if (id > -1 && allRemoteBufferIds.find(id) == allRemoteBufferIds.end()) {
        auto info = RemoteBufferInfo(
            storeOp->input->tensor(CacheStoreOp::getCachedTensorInIndex())
                ->info,
            1);
        ir.setRemoteBufferInfo(id, info);
        remoteBufferId = std::max(remoteBufferId, id + 1);
      }
    }
  }

  // Mapping from each CacheArg to it's final consumers
  std::map<TensorId, std::set<Op *>> argOpMap;
  std::map<Op *, std::set<TensorId>> opArgMap;
  std::map<TensorId, std::pair<RemoteBufferId, RemoteBufferIndex>> argBufferMap;

  // TODO: Support for Loop and ID ranges per arg tensor ID

  for (TensorId &tensor_id : graph.getTensors().getAllTensorIds()) {
    Tensor *tensor = graph.getTensors().get(tensor_id);
    if (tensor->isCacheArgTensor()) {
      logging::transform::trace("[CacheSetup] Resolving CacheArg tensor {}",
                                tensor_id);
      std::vector<Tensor *> traceFront;
      traceFront.push_back(tensor);

      while (traceFront.size() > 0) {
        Tensor *front = traceFront.back();
        traceFront.pop_back();
        for (Op *consumer : front->consumers.getOps()) {
          // Only certain ops can be on the path between CacheArg and
          // CacheLoad/Store.
          if (consumer->opid == Onnx::CustomOperators::CacheLoad ||
              consumer->opid == Onnx::CustomOperators::CacheStore) {
            argOpMap[tensor_id].insert(consumer);
            opArgMap[consumer].insert(tensor_id);
          } else if (consumer->opid == Onnx::CustomOperators::Call_1) {
            CallOp *call = dynamic_cast<CallOp *>(consumer);

            auto indices = consumer->input->indices(front);
            for (auto index : indices) {
              auto t_id = call->getCalledGraph().getInputId(index);
              auto t    = call->getCalledGraph().getTensors().get(t_id);
              traceFront.push_back(t);
            }
          } else {
            logging::warn("[CacheSetup] Unsupported Op {} in path from"
                          "CacheArg tensor {}.",
                          consumer->debugName(),
                          tensor->id);
          }
        }
      }
    }
  }

  std::map<int64_t, std::set<VGraphId>> remoteBufferVGIDs;

  for (auto &argOp : argOpMap) {
    if (argBufferMap.find(argOp.first) == argBufferMap.end()) {
      int64_t remoteBufferIndex = 0;

      // All cacheArg tensors in a group will refer to the same RemoteBufferId
      std::set<TensorId> group;
      TensorInfo tensorInfo;
      group.insert(argOp.first);

      std::vector<Op *> front;
      front.insert(front.end(), argOp.second.begin(), argOp.second.end());
      while (front.size() > 0) {
        Op *op = front.back();
        front.pop_back();
        for (TensorId tensor_id : opArgMap.at(op)) {
          if (group.find(tensor_id) == group.end()) {
            group.insert(tensor_id);
            front.insert(front.end(),
                         argOpMap.at(tensor_id).begin(),
                         argOpMap.at(tensor_id).end());
          }
        }
      }

      for (TensorId tensor_id : group) {
        argBufferMap[tensor_id] = {remoteBufferId, remoteBufferIndex};
        auto cacheArgTensor     = graph.getTensors().get(tensor_id);
        *static_cast<int *>(cacheArgTensor->tensorData()->data()) =
            static_cast<int>(remoteBufferIndex);
        logging::trace("CacheArg {} buffer: {} index: {}",
                       tensor_id,
                       remoteBufferId,
                       remoteBufferIndex);
        for (Op *op : argOpMap[tensor_id]) {
          if (CacheStoreOp *cs = dynamic_cast<CacheStoreOp *>(op)) {
            cs->setRemoteBufferId(remoteBufferId);
            tensorInfo = cs->input->tensor(cs->getCachedTensorInIndex())->info;
            remoteBufferVGIDs[remoteBufferId].insert(
                cs->hasVirtualGraphId() ? cs->getVirtualGraphId() : -1);
            logging::transform::trace(
                "[CacheSetup] Op {} connected to remote buffer {}. "
                "Tensor info {}.",
                cs->debugName(),
                remoteBufferId,
                cs->inInfo(CacheStoreOp::getCachedTensorInIndex()));
          }
          if (CacheLoadOp *cl = dynamic_cast<CacheLoadOp *>(op)) {
            cl->setRemoteBufferId(remoteBufferId);
            tensorInfo =
                cl->output->tensor(cl->getCachedTensorOutIndex())->info;
            remoteBufferVGIDs[remoteBufferId].insert(
                cl->hasVirtualGraphId() ? cl->getVirtualGraphId() : -1);
            logging::transform::trace(
                "[CacheSetup] Op {} connected to remote buffer {}. "
                "Tensor info {}.",
                cl->debugName(),
                remoteBufferId,
                cl->outInfo(CacheLoadOp::getCachedTensorOutIndex()));
          }
        }
        remoteBufferIndex++;
      }

      auto info = RemoteBufferInfo(tensorInfo, remoteBufferIndex);
      ir.setRemoteBufferInfo(remoteBufferId, info);

      remoteBufferId++;
    }
  }

  for (auto &mapping : remoteBufferVGIDs) {
    if (mapping.second.size() > 1) {
      if (logging::transform::isEnabled(logging::Level::Trace)) {
        logging::transform::trace("[CacheSetup] Remote buffer ID {} maps to "
                                  "multiple virtual graphs {} with:",
                                  mapping.first,
                                  mapping.second);
        for (auto &argBuffer : argBufferMap) {
          auto &tensor_id         = argBuffer.first;
          auto &remoteBufferId    = argBuffer.second.first;
          auto &remoteBufferIndex = argBuffer.second.second;
          if (remoteBufferId != mapping.first)
            continue;
          logging::transform::trace("[CacheSetup]   Tensor arg {} with:",
                                    tensor_id);
          for (Op *op : argOpMap[tensor_id]) {
            logging::transform::trace(
                "[CacheSetup]     Op {} phase {} vgid {} {}.",
                op->opid,
                op->getPingPongPhase(),
                op->getVirtualGraphId(),
                op->debugName());
          }
        }
      }
      throw error("[CacheSetup] Remote buffer ID {} maps to multiple virtual "
                  "graphs {}.",
                  mapping.first,
                  mapping.second);
    }
  }

  // Cached (weight) tensors
  for (TensorId &tensor_id : graph.getTensors().getAllTensorIds()) {
    Tensor *tensor = graph.getTensors().get(tensor_id);
    if (tensor->cacheInfo.isCached()) {
      auto arg_tensor_id = getCacheArgTensorId(tensor_id);
      tensor->cacheInfo.setRemoteBufferInfo(argBufferMap[arg_tensor_id].first,
                                            argBufferMap[arg_tensor_id].second);
    }
  }

  logging::debug("[CacheSetup] Done.");
  return true;
}

namespace {
// CacheSetup
bool init = Transform::registerTransform(new CacheSetup());
} // namespace

} // namespace popart
