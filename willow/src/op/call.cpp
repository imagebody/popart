#include <poponnx/graph.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op/call.hpp>
#include <poponnx/opserialiser.hpp>
#include <poponnx/scope.hpp>
#include <poponnx/tensorindex.hpp>

namespace poponnx {

CallOp::CallOp(Graph &parent_, Graph &callee_)
    : Op(Onnx::CustomOperators::Call, {parent_, ""}), callee(callee_) {
  settings.name = fmt::format("Call_{}", callee_.id);
}

void CallOp::setup() {}

std::unique_ptr<Op> CallOp::clone() const { return make_unique<CallOp>(*this); }

Graph &CallOp::getCalledGraph() { return callee.get(); }

void CallOp::appendAttributes(OpSerialiserBase &os) const {
  Op::appendAttributes(os);
  os.appendAttribute("callee", callee.get().id.str());
}

bool CallOp::isInputModified(InIndex index) {
  auto tensor_id = getCalledGraph().getInputId(index);
  auto tensor    = getCalledGraph().getTensors().get(tensor_id);

  for (auto consumer : tensor->consumers.getOps()) {
    for (auto i : consumer->input->indices(tensor)) {
      if (consumer->aliases(i).isEmpty() == false ||
          consumer->modifies(i).isEmpty() == false) {
        return true;
      }
    }
  }

  return false;
}

} // namespace poponnx