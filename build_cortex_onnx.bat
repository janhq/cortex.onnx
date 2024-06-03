cmake -S ./third-party -B ./build_deps/third-party
cmake --build ./build_deps/third-party --config Release -j4

cmake -S .\onnxruntime-genai\ -B .\onnxruntime-genai\build -DUSE_DML=ON -DUSE_CUDA=OFF -DENABLE_PYTHON=OFF -DORT_HOME=./build_deps/ort
cmake --build .\onnxruntime-genai\build --config Release -j4
