# Makefile for Cortex onnx engine - Build, Lint, Test, and Clean

CMAKE_EXTRA_FLAGS ?= ""
RUN_TESTS ?= true
MODEL_PATH ?= ./directml/directml-int4-awq-block-128/

# Default target, does nothing
all:
	@echo "Specify a target to run"

# Build the Cortex onnx engine
install-dependencies:
ifeq ($(OS),Windows_NT) # Windows
	@powershell -Command "cmake -S ./third-party -B ./build_deps/third-party;"
	@powershell -Command "cmake --build ./build_deps/third-party --config Release -j4;"
else  # Unix-like systems (Linux and MacOS)
	@echo "Skipping install dependencies"
	@exit 0
endif

build-onnxruntime:
ifeq ($(OS),Windows_NT) # Windows
	@powershell -Command "cmake -S .\onnxruntime-genai\ -B .\onnxruntime-genai\build -DUSE_DML=ON -DUSE_CUDA=OFF -DENABLE_PYTHON=OFF -DORT_HOME=\".\build_deps\ort\";"
	@powershell -Command "cmake --build .\onnxruntime-genai\build --config Release -j4;"
else  # Unix-like systems (Linux and MacOS)
	@echo "Skipping install dependencies"
	@exit 0
endif

build-engine:
ifeq ($(OS),Windows_NT)
	@powershell -Command "mkdir -p build; cd build; cmake .. $(CMAKE_EXTRA_FLAGS); cmake --build . --config Release;"
else
	@echo "Skipping build engine"
	@exit 0
endif

build-example-server:
ifeq ($(OS),Windows_NT)
	@powershell -Command "mkdir -p .\examples\server\build\Release\engines\cortex.onnx; cd .\examples\server\build; cmake .. $(CMAKE_EXTRA_FLAGS); cmake --build . --config Release;"
	@powershell -Command "cp .\build_deps\ort\lib\D3D12Core.dll .\examples\server\build\Release\;"
	@powershell -Command "cp .\build_deps\ort\lib\DirectML.dll .\examples\server\build\Release\;"
	@powershell -Command "cp .\build_deps\ort\lib\onnxruntime.dll .\examples\server\build\Release\onnxruntime.rel.dll;"
	@powershell -Command "cp .\onnxruntime-genai\build\Release\onnxruntime-genai.dll .\examples\server\build\Release\;"
	@powershell -Command "cp .\build\Release\engine.dll .\examples\server\build\Release\engines\cortex.onnx\;"
else 
	@echo "Skipping build example server"
	@exit 0
endif

package:
ifeq ($(OS),Windows_NT)
	@powershell -Command "mkdir -p cortex.onnx; cp build\Release\engine.dll cortex.onnx\; cp .\examples\server\build\Release\*.dll cortex.onnx\; 7z a -ttar temp.tar cortex.onnx\*; 7z a -tgzip cortex.onnx.tar.gz temp.tar;"
else
	@echo "Skipping package"
	@exit 0
endif

run-e2e-test:
ifeq ($(RUN_TESTS),false)
	@echo "Skipping tests"
else
ifeq ($(OS),Windows_NT)
	@powershell -Command "python -m pip install --upgrade pip;"
	@powershell -Command "python -m pip install requests;"
	@powershell -Command "python -m pip install huggingface-hub[cli];"
	@powershell -Command "huggingface-cli download microsoft/Phi-3-mini-4k-instruct-onnx --include directml/* --local-dir . ;"
	@powershell -Command "cd examples\server\build\Release; python ..\..\..\..\.github\scripts\e2e-test-server.py server ..\..\..\..\$(MODEL_PATH);"
else
	@echo "Skipping run e2e test"
	@exit 0
endif
endif

clean:
ifeq ($(OS),Windows_NT)
	cmd /C "rmdir /S /Q build examples\\server\\build cortex.onnx cortex.onnx.tar.gz cortex.onnx.zip"
else
	@echo "Skipping clean"
	@exit 0
endif