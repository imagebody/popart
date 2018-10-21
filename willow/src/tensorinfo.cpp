#include <numeric>
#include <willow/error.hpp>
#include <willow/tensorinfo.hpp>
#include <willow/util.hpp>

namespace willow {

TensorInfo::TensorInfo(DataType t, const std::vector<int64_t> &s)
    : dataTypeInfo(&getDataTypeInfoMap().at(t)), shape_v(s) {}

TensorInfo::TensorInfo(std::string s_type, const std::vector<int64_t> &s)
    : TensorInfo(dataTypeFromString(s_type), s) {}

TensorInfo::TensorInfo(const onnx::TensorProto &t) { set(t); }

TensorInfo::TensorInfo(std::string s_type, std::string s_shape)
    : TensorInfo(dataTypeFromString(s_type), shapeFromString(s_shape)) {}

void TensorInfo::set(const onnx::TensorProto &t) {
  dataTypeInfo = &getDataTypeInfoMap().at(t.data_type());
  shape_v.resize(0);
  shape_v.reserve(t.dims_size());
  for (auto &v : t.dims()) {
    shape_v.push_back(v);
  }
}

std::vector<size_t> TensorInfo::shape_szt() const {
  std::vector<size_t> szts;
  szts.reserve(rank());
  for (auto &x : shape()) {
    szts.push_back(static_cast<size_t>(x));
  }
  return szts;
}

// numpy output shape of:
std::vector<int64_t> npOut(const std::vector<int64_t> &s0,
                           const std::vector<int64_t> &s1) {

  if (s0 != s1) {
    throw error("np broadcasting not implemented");
  }

  return s0;
}

TensorInfo npOut(const TensorInfo &i0, const TensorInfo &i1) {
  if (i0 != i1) {
    throw error("np broadcasting not supported, failed TensorInfo comparison");
  }

  return i1;
}

void TensorInfo::append(std::stringstream &ss) const {
  ss << padded(dataTypeInfo->name(), 8);
  appendSequence(ss, shape_v);
}

bool TensorInfo::isSet() const { return dataTypeInfo != nullptr; }

const std::string &TensorInfo::data_type() const {
  return dataTypeInfo->name();
}

const std::vector<int64_t> &TensorInfo::shape() const { return shape_v; }

int TensorInfo::rank() const { return static_cast<int>(shape_v.size()); }

int64_t TensorInfo::nelms() const {
  return std::accumulate(
      shape_v.begin(), shape_v.end(), 1, std::multiplies<int64_t>());
}

int64_t TensorInfo::nbytes() const {
  return nelms() * static_cast<int64_t>(dataTypeInfo->nbytes());
}

int64_t TensorInfo::dim(int i) const { return shape_v[i]; }

DataType TensorInfo::dataType() const { return dataTypeInfo->type(); }

void TensorInfo::set(DataType t, const std::vector<int64_t> &s) {
  dataTypeInfo = &getDataTypeInfoMap().at(t);
  shape_v      = s;
}

const std::map<DataType, DataTypeInfo> &getDataTypeInfoMap() {
  static std::map<DataType, DataTypeInfo> dataTypeInfoMap =
      initDataTypeInfoMap();
  return dataTypeInfoMap;
}

bool TensorInfo::operator==(const TensorInfo &i1) const {
  return (shape_v == i1.shape_v && dataTypeInfo == i1.dataTypeInfo);
}

bool TensorInfo::operator!=(const TensorInfo &i1) const {
  return !(operator==(i1));
}

std::map<DataType, DataTypeInfo> initDataTypeInfoMap() {

  return {{TP::UNDEFINED, {TP::UNDEFINED, -1, "UNDEFINED"}},
          {TP::FLOAT, {TP::FLOAT, 4, "FLOAT"}},
          {TP::UINT8, {TP::UINT8, 1, "UINT8"}},
          {TP::INT8, {TP::INT8, 1, "INT8"}},
          {TP::UINT16, {TP::UINT16, 2, "UINT16"}},
          {TP::INT16, {TP::INT16, 2, "INT16"}},
          {TP::INT32, {TP::INT32, 4, "INT32"}},
          {TP::INT64, {TP::INT64, 8, "INT64"}},
          {TP::STRING, {TP::STRING, -1, "STRING"}},
          {TP::BOOL, {TP::BOOL, 1, "BOOL"}},
          {TP::FLOAT16, {TP::FLOAT16, 2, "FLOAT16"}},
          {TP::DOUBLE, {TP::DOUBLE, 8, "DOUBLE"}},
          {TP::UINT32, {TP::UINT32, 4, "UINT32"}},
          {TP::UINT64, {TP::UINT64, 8, "UINT64"}},
          {TP::COMPLEX64, {TP::COMPLEX64, 8, "COMPLEX64"}},
          {TP::COMPLEX128, {TP::COMPLEX128, 16, "COMPLEX128"}}};
}

std::map<std::string, DataType> initStrToDataTypeMap() {
  std::map<std::string, DataType> invMap;
  for (auto &type_inf : getDataTypeInfoMap()) {
    auto dtInf           = type_inf.second;
    invMap[dtInf.name()] = dtInf.type();
  }
  return invMap;
}

const std::string &getAllONNXTypesString() {
  const static std::string allTypes = initAllONNXTypesString();
  return allTypes;
}

std::string initAllONNXTypesString() {
  std::stringstream allTypes;
  allTypes << '[';
  bool firstType = true;
  for (auto &name_type : getStrToDataTypeMap()) {
    if (firstType) {
      firstType = false;
    } else {
      allTypes << ',' << ' ';
    }
    allTypes << name_type.first;
  }
  allTypes << ']';
  return allTypes.str();
}

DataType TensorInfo::dataTypeFromString(const std::string &s) const {
  auto found = getStrToDataTypeMap().find(s);
  if (found == getStrToDataTypeMap().end()) {
    throw error("no ONNX type " + s + ", they're " + getAllONNXTypesString() +
                ".");
  }
  return found->second;
}

// expects shape to be "(1 2 400 3)" or "(5)", so no spaces allowed.
std::vector<int64_t> TensorInfo::shapeFromString(const std::string &s) const {
  if (s.size() < 2 || s[0] != '(' || s[s.size() - 1] != ')') {
    throw error("invalid string for shape");
  }
  if (s.find(' ') != std::string::npos) {
    throw error("s contains a space : not valid shape string");
  }

  std::vector<int64_t> shape;

  // https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
  std::string token;
  std::istringstream tokenStream(s.substr(1, s.size() - 2));
  while (std::getline(tokenStream, token, ',')) {
    shape.push_back(std::stoi(token));
  }

  std::stringstream ss;
  return shape;
}

const std::map<std::string, DataType> &getStrToDataTypeMap() {
  static std::map<std::string, DataType> m = initStrToDataTypeMap();
  return m;
}

DataTypeInfo::DataTypeInfo(DataType type__, int nbytes__, std::string name__)
    : type_(type__), nbytes_(nbytes__), name_(name__) {}

const int &DataTypeInfo::nbytes() const { return nbytes_; }

const std::string &DataTypeInfo::name() const { return name_; }

DataType DataTypeInfo::type() const { return type_; }

} // namespace willow
