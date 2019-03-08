#ifndef GUARD_NEURALNET_AUTO_VIRTUAL_GRAPH_HPP
#define GUARD_NEURALNET_AUTO_VIRTUAL_GRAPH_HPP

#include <poponnx/op.hpp>
#include <poponnx/transforms/transform.hpp>

namespace poponnx {

class Subgraph {
public:
  Subgraph(OpId op_id) : cost(0.f), candidates({op_id}), split_nodes({}) {}
  Subgraph(float c, OpId op_id)
      : cost(c), candidates({op_id}), split_nodes({}) {}

  float cost;
  std::set<OpId> candidates;
  std::map<float, OpId> split_nodes;

  std::set<OpId> final_splits;

  int64_t virtual_graph_id = 0;

  std::pair<bool, OpId> best_split(float split_cost);
};

class AutoVirtualGraph : public Transform {
public:
  static std::size_t id();

  AutoVirtualGraph() : Transform() {}
  virtual ~AutoVirtualGraph() override {}

  bool apply(Ir &ir) const final;

  virtual std::size_t getId() const override final { return id(); }

  virtual std::string getName() const override final {
    return "AutoVirtualGraph";
  }

  float costFn(Op *op, bool training) const;
};

} // namespace poponnx

#endif