#include <spdlog/fmt/fmt.h>
#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/op.hpp>
#include <poponnx/patterns/pattern.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/util.hpp>

namespace poponnx {

int Pattern::tensor_counter = 0;

bool Pattern::touchesAnchored(Op *op) const {
  for (auto &tensor : touches(op)) {
    if (op->pir->isAnchored(tensor->id)) {
      return true;
    }
  }
  return false;
};

TensorId Pattern::createTemporaryTensorId(TensorId base_id) {
  auto temp_id = fmt::format("t{}__{}", tensor_counter++, base_id);
  logging::ir::trace("Generating tensor id {}", temp_id);
  return temp_id;
}

} // namespace poponnx
