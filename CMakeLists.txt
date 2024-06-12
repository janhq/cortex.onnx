cmake_minimum_required(VERSION 3.5)
project(cortex.onnx)
SET(TARGET engine)

if(UNIX AND NOT APPLE)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC")
  add_compile_options(-fPIC)
endif()

set(THIRD_PARTY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build_deps/_install)
set(ORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build_deps/ort)

if(UNIX AND NOT APPLE)
  add_compile_options(-fPIC)
endif()

include(CheckIncludeFileCXX)
# CPP version
check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if(HAS_ANY
  AND HAS_STRING_VIEW
  AND HAS_COROUTINE)
  set(CMAKE_CXX_STANDARD 20)
elseif(HAS_ANY AND HAS_STRING_VIEW)
  set(CMAKE_CXX_STANDARD 17)
else()
  set(CMAKE_CXX_STANDARD 14)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(ORT_GENAI_LIB_DIR ${CMAKE_SOURCE_DIR}/onnxruntime-genai/build/Release)
MESSAGE("ORT_GENAI_LIB_DIR=" ${ORT_GENAI_LIB_DIR})
MESSAGE("CMAKE_SOURCE_DIR=" ${CMAKE_SOURCE_DIR})

set(RE2 ${CMAKE_SOURCE_DIR}/build_deps/ort)

if(WIN32)
  set(ONNXRUNTIME_GENAI_LIB "onnxruntime-genai.dll")
  set(ONNXRUNTIME_GENAI_DEPENDENCY "*.dll")
elseif(APPLE)
  set(ONNXRUNTIME_GENAI_LIB "libonnxruntime-genai.dylib")
  set(ONNXRUNTIME_GENAI_DEPENDENCY "*.dylib")
else()
  set(ONNXRUNTIME_GENAI_LIB "libonnxruntime-genai.so")
  set(ONNXRUNTIME_GENAI_DEPENDENCY "*.so")
endif()

add_library(${TARGET} SHARED 
    src/onnx_engine.cc
)

find_library(JSONCPP
    NAMES jsoncpp
    HINTS "${THIRD_PARTY_PATH}/lib"
)

find_library(TRANTOR
    NAMES trantor
    HINTS "${THIRD_PARTY_PATH}/lib"
)
target_link_directories(${TARGET} PRIVATE ${ORT_GENAI_LIB_DIR})
target_link_libraries(${TARGET} PRIVATE ${ONNXRUNTIME_GENAI_LIB} ${JSONCPP} ${TRANTOR} ${RE2}/lib/re2.lib 
                                              ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${TARGET} PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/base
            ${CMAKE_SOURCE_DIR}/onnxruntime-genai/src
            ${ORT_PATH}/include
            ${THIRD_PARTY_PATH}/include
            ${RE2}/include 
)