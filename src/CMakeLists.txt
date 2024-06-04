cmake_minimum_required(VERSION 3.18.1)

project(cortex.onnx)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a")

option(USE_CXX "Invoke the C++ example" ON)

if(USE_CXX)
  add_compile_definitions(USE_CXX)
endif()

set(ORT_GENAI_LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

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

add_executable(phi3 ${CMAKE_SOURCE_DIR}/onnx_engine.cc)


target_link_directories(phi3 PRIVATE ${ORT_GENAI_LIB_DIR})
target_link_libraries(phi3 PRIVATE ${ONNXRUNTIME_GENAI_LIB})
target_include_directories(phi3 PRIVATE ${CMAKE_SOURCE_DIR}/../onnxruntime-genai/src/)

target_link_libraries(
        phi3
        PUBLIC
        onnxruntime-genai)

file(GLOB ort_genai_libs "${CMAKE_SOURCE_DIR}/lib/${ONNXRUNTIME_GENAI_DEPENDENCY}")

foreach(DLL_FILE ${ort_genai_libs})
  add_custom_command(
    TARGET phi3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_FILE} $<TARGET_FILE_DIR:phi3>
  )
endforeach()