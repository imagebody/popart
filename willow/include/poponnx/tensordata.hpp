#ifndef GUARD_NEURALNET_STEPIO_HPP
#define GUARD_NEURALNET_STEPIO_HPP

#include <poponnx/names.hpp>
#include <poponnx/tensorinfo.hpp>

namespace willow {

// TODO: This API should change when T5207 is done

// A class to hold data, used
// within the willow::Tensor class.
class TensorData {
public:
  // create by copying from src to data_,
  // the size of the copy determined by TensorInfo
  TensorData(const TensorInfo &, const void *src);

  // create by copying to data_ from onnx::TensorProto
  TensorData(const onnx::TensorProto &);
  void *data();

private:
  std::vector<char> data_;
};

// A class to point to constant data
class ConstVoidData {
public:
  const void *data;
  // This is used to confirm that data is as expected
  TensorInfo info;
};

// A class to point to non-const data
class MutableVoidData {
public:
  void *data;
  // This is used to confirm that data is as expected
  TensorInfo info;
};

// A virtual class for accessing pointers to
// the data required to perform a training step
class StepIO {
public:
  virtual ~StepIO() = default;
  // constant input data,
  virtual ConstVoidData in(TensorId) const = 0;
  // non-const anchor data,
  // which will be modified inplace.
  virtual MutableVoidData out(TensorId) const = 0;
};

} // namespace willow

#endif