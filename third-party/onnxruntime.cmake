# onnxruntime
set(VERSION 1.18.0)
set(ONNX_VERSION v${VERSION})

MESSAGE("ONNX_VERSION=" ${ONNX_VERSION})

# Download library based on instructions 
if(UNIX) 
  MESSAGE("NOT SUPPORT")
else()
  set(LIBRARY_NAME Microsoft.ML.OnnxRuntime.DirectML.${VERSION}.zip)
endif()

set(ONNX_URL https://github.com/microsoft/onnxruntime/releases/download/${ONNX_VERSION}/${LIBRARY_NAME})
MESSAGE("ONNX_URL="${ONNX_URL})
MESSAGE("LIBARRY_NAME=" ${LIBRARY_NAME})

set(ONNX_PATH ${CMAKE_BINARY_DIR}/${LIBRARY_NAME})

MESSAGE("CMAKE_BINARY_DIR = " ${CMAKE_BINARY_DIR})

file(DOWNLOAD ${ONNX_URL} ${ONNX_PATH} STATUS ONNX_DOWNLOAD_STATUS)
list(GET ONNX_DOWNLOAD_STATUS 0 ONNX_DOWNLOAD_STATUS_NO)
# MESSAGE("file = " ${CMAKE_BINARY_DIR}/engines/${LIBRARY_NAME})

if(ONNX_DOWNLOAD_STATUS_NO)
    message(STATUS "Pre-built library not downloaded. (${ONNX_DOWNLOAD_STATUS})")
else()
    message(STATUS "Linking downloaded pre-built library.")
    file(ARCHIVE_EXTRACT INPUT ${ONNX_PATH} DESTINATION ${CMAKE_BINARY_DIR}/onnxruntime)
    file(COPY ${CMAKE_BINARY_DIR}/onnxruntime/runtimes/win-x64/native/onnxruntime.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
    file(COPY ${CMAKE_BINARY_DIR}/onnxruntime/runtimes/win-x64/native/onnxruntime.lib DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
    file(COPY ${CMAKE_BINARY_DIR}/../../third-party/DirectML.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
    file(COPY ${CMAKE_BINARY_DIR}/../../third-party/D3D12Core.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
    file(COPY ${CMAKE_BINARY_DIR}/onnxruntime/build/native/include DESTINATION ${CMAKE_BINARY_DIR}/../ort/)
endif()