set(WhatToDoString "Try setting ONNX_INSTALL_DIR if not already done, \
something like -DONNX_INSTALL_DIR=/path/to/onnx/build/install/, \
otherwise additional search paths might be needed in FindONNX.cmake \
(add to HINTS and/or PATH_SUFFIXES)")

FIND_LIBRARY(ONNX_PROTO_LIB
  NAMES onnx_proto
  HINTS ${ONNX_INSTALL_DIR}/lib
  DOC "onnx library to link to (corresponding to proto generated header)")
IF(NOT ONNX_PROTO_LIB)
  MESSAGE(FATAL_ERROR "Could not set ONNX_PROTO_LIB, ${WhatToDoString}")
ENDIF()
MARK_AS_ADVANCED(ONNX_PROTO_LIB)
MESSAGE(STATUS "found onnx_proto, defining ONNX_PROTO_LIB: ${ONNX_PROTO_LIB}")


FIND_LIBRARY(ONNX_LIB
  NAMES onnx
  HINTS ${ONNX_INSTALL_DIR}/lib
  DOC "onnx library to link to (for model tests)")
IF(NOT ONNX_PROTO_LIB)
  MESSAGE(FATAL_ERROR "Could not set ONNX_LIB, ${WhatToDoString}")
ENDIF()
MARK_AS_ADVANCED(ONNX_LIB)
MESSAGE(STATUS "found onnx, defining ONNX_LIB: ${ONNX_LIB} (for model testing)")


FIND_PATH(ONNX_CHECKER_INCLUDE_DIR
  NAMES onnx/checker.h
  HINTS ${ONNX_INSTALL_DIR}/include
  PATH_SUFFIXES  # onnx onnx/include onnx/onnx
  DOC "directory containing the onnx model checker header, checker.h")
IF(NOT ONNX_CHECKER_INCLUDE_DIR)
  MESSAGE(FATAL_ERROR "Could not set ONNX_CHECKER_INCLUDE_DIR, ${WhatToDoString}")
ENDIF()
MARK_AS_ADVANCED(ONNX_CHECKER_INCLUDE_DIR)
MESSAGE(STATUS "found onnx/checker.h, defining \
ONNX_CHECKER_INCLUDE_DIR: ${ONNX_CHECKER_INCLUDE_DIR}")


FIND_PATH(ONNX_PB_INCLUDE_DIR
  NAMES onnx/onnx_pb.h
  HINTS ${ONNX_INSTALL_DIR}/include #/.setuptools-cmake-build
  PATH_SUFFIXES # onnx onnx/include
  DOC "directory containing the protobuf generated header, onnx_pb.h")
IF(NOT ONNX_PB_INCLUDE_DIR)
  MESSAGE(FATAL_ERROR "Could not set ONNX_PB_INCLUDE_DIR, ${WhatToDoString}")
ENDIF()
MARK_AS_ADVANCED(ONNX_PB_INCLUDE_DIR)
MESSAGE(STATUS "found onnx/onnx_pb.h, defining \
ONNX_PB_INCLUDE_DIR: ${ONNX_PB_INCLUDE_DIR}")


FIND_PATH(ONNX_SCHEMA_INCLUDE_DIR
  NAMES onnx/defs/schema.h
  HINTS ${ONNX_INSTALL_DIR}/include
  PATH_SUFFIXES # onnx onnx/include
  DOC "directory containing the header with functions \
  for checking opset:version mapping, schema.h")
  IF(NOT ONNX_SCHEMA_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Could not set ONNX_SCHEMA_INCLUDE_DIR, ${WhatToDoString}")
ENDIF()
MARK_AS_ADVANCED(ONNX_SCHEMA_INCLUDE_DIR)
MESSAGE(STATUS "\
found onnx/defs/schema.h, defining \
ONNX_SCHEMA_INCLUDE_DIR: ${ONNX_SCHEMA_INCLUDE_DIR}")
