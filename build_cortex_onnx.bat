cmake -S ./third-party -B ./build_deps/third-party
cmake --build ./build_deps/third-party --config Release -j4

cmake -S .\onnxruntime-genai\ -B .\onnxruntime-genai\build -DUSE_DML=ON -DUSE_CUDA=OFF -DORT_HOME="./build_deps/ort" -DENABLE_PYTHON=OFF -DENABLE_TESTS=OFF -DENABLE_MODEL_BENCHMARK=OFF
cmake --build .\onnxruntime-genai\build --config Release -j4
