# onnxruntime
set(COMMIT b04adc)

message("Copying onnx dependencies...")
file(COPY ${CMAKE_BINARY_DIR}/../../third-party/onnxruntime-${COMMIT}/lib/onnxruntime.rel.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
file(RENAME ${CMAKE_BINARY_DIR}/../ort/lib/onnxruntime.rel.dll ${CMAKE_BINARY_DIR}/../ort/lib/onnxruntime.dll)
file(COPY ${CMAKE_BINARY_DIR}/../../third-party/onnxruntime-${COMMIT}/lib/onnxruntime.rel.lib DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
file(RENAME ${CMAKE_BINARY_DIR}/../ort/lib/onnxruntime.rel.lib ${CMAKE_BINARY_DIR}/../ort/lib/onnxruntime.lib)
file(COPY ${CMAKE_BINARY_DIR}/../../third-party/onnxruntime-${COMMIT}/include DESTINATION ${CMAKE_BINARY_DIR}/../ort/)
file(COPY ${CMAKE_BINARY_DIR}/../../third-party/DirectML.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)
file(COPY ${CMAKE_BINARY_DIR}/../../third-party/D3D12Core.dll DESTINATION ${CMAKE_BINARY_DIR}/../ort/lib/)