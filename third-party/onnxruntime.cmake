# onnxruntime
set(VERSION 1.19.0)
set(ONNX_VERSION v${VERSION})

MESSAGE("ONNX_VERSION=" ${ONNX_VERSION})

# Download library based on instructions 
if(UNIX) 
  MESSAGE("NOT SUPPORT")
else()
  set(LIBRARY_NAME Microsoft.ML.OnnxRuntime.DirectML.${VERSION}.zip)
endif()

# set(ONNX_URL https://github.com/microsoft/onnxruntime/releases/download/${ONNX_VERSION}/${LIBRARY_NAME})
# use nightly build
set(ONNX_URL https://se1vsblobprodcus349.vsblob.vsassets.io/b-bc038106a83b4dab9dd35a41bc58f34c/394DCC8ACF9F9E3D56F2311BDCDA5C07F0662C463E6A03EBA3B7638D5AFB651E00.blob?sv=2019-07-07&sr=b&si=1&sig=vWmFA1lP8T5zOIDdXlKUB3BvXxaKkuM5HlkaOyHeRz8%3D&spr=https&se=2024-06-25T06%3A40%3A35Z&rscl=x-e2eid-e893b2c0-06fc4f39-bd9baccc-7d9fb9bb-session-e893b2c0-06fc4f39-bd9baccc-7d9fb9bb&rscd=attachment%3B%20filename%3D%22Microsoft.ML.OnnxRuntime.DirectML.1.19.0-dev-20240621-0444-69d522f4e9.nupkg%22&P1=1719225635&P2=1&P3=2&P4=LRUqgcGvSbiUrJgeGSR6TV3kcGSYjxGjlKwis6IbYwM%3d)
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