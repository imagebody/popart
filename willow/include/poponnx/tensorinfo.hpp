#ifndef GUARD_NEURALNET_TENSORINFO_HPP
#define GUARD_NEURALNET_TENSORINFO_HPP

#include <onnx/onnx_pb.h>

#include <sstream>
#include <vector>
#include <poponnx/names.hpp>

namespace willow {

class TensorInfo;

// Calculate the numpy broadcast shape as described in
// https://docs.scipy.org/doc/numpy/user/basics.broadcasting.html
std::vector<int64_t> npOut(const std::vector<int64_t> &s0,
                           const std::vector<int64_t> &s1);

template <typename T> std::vector<T> squeeze(const std::vector<T> &v) {
  std::vector<T> w;
  w.reserve(v.size());
  for (auto &x : v) {
    if (x != 1) {
      w.push_back(x);
    }
  }
  return w;
}

// Calculate the numpy broadcast shape as described in
// https://docs.scipy.org/doc/numpy/user/basics.broadcasting.html
TensorInfo npOut(const TensorInfo &i0, const TensorInfo &i1);

// FLOAT, FLOAT16, INT8 etc.
class DataTypeInfo {
public:
  DataTypeInfo(DataType type__,
               int nbytes__,
               std::string name__,
               std::string lcasename__);
  DataType type() const;
  // number of bytes of 1 element
  const int &nbytes() const;
  const std::string &name() const;
  const std::string &lcasename() const;

private:
  DataType type_;
  int nbytes_;
  std::string name_;
  std::string lcasename_;
};

const std::map<DataType, DataTypeInfo> &getDataTypeInfoMap();
std::map<DataType, DataTypeInfo> initDataTypeInfoMap();

const std::map<std::string, DataType> &getStrToDataTypeMap();
std::map<std::string, DataType> initStrToDataTypeMap();

const std::string &getAllONNXTypesString();
std::string initAllONNXTypesString();

class TensorInfo {
public:
  TensorInfo(DataType, const std::vector<int64_t> &);
  TensorInfo(std::string data_type, std::string shape);
  TensorInfo(std::string data_type, const std::vector<int64_t> &);
  TensorInfo(const onnx::TensorProto &);
  void set(const onnx::TensorProto &);
  TensorInfo() = default;
  void set(DataType, const std::vector<int64_t> &);
  const std::vector<int64_t> &shape() const;
  // A helper functions for back-ends which
  // prefer the size as (unsigned) size_t.
  std::vector<size_t> shape_szt() const;
  int rank() const;
  int64_t nelms() const;
  // total bytes of tensor
  int64_t nbytes() const;
  int64_t dim(int i) const;
  DataType dataType() const;
  const std::string &data_type() const;
  const std::string &data_type_lcase() const;
  void append(std::ostream &) const;
  bool isSet() const;
  bool operator==(const TensorInfo &) const;
  bool operator!=(const TensorInfo &) const;
  DataType dataTypeFromString(const std::string &s) const;
  std::vector<int64_t> shapeFromString(const std::string &s) const;
  onnx::TypeProto getOnnxTypeProto() const;

private:
  const DataTypeInfo *dataTypeInfo = nullptr;
  std::vector<int64_t> shape_v;
};

std::ostream &operator<<(std::ostream &stream, const TensorInfo &ti);

} // namespace willow

#endif
