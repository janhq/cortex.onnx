# cortex.onnx
cortex.onnx is a high-efficiency C++ inference engine for edge computing focusing on Windows platform using DirectML for GPU acceleration. 

It is a dynamic library that can be loaded by any server at runtime.

# Repo Structure
```
.
├── base -> Engine interface
├── examples -> Server example to integrate engine
├── onnxruntime-genai -> Upstream onnxruntime-genai
├── src -> Engine implementation
├── third-party -> Dependencies of the cortex.onnx project
```

## Build from source

This guide provides step-by-step instructions for building cortex.onnx from source on Windows systems.

## Clone the Repository

First, you need to clone the cortex.onnx repository:

```bash
git clone --recurse https://github.com/janhq/cortex.onnx.git
```

If you don't have git, you can download the source code as a file archive from [cortex.onnx GitHub](https://github.com/janhq/cortex.onnx). 

## Build library with server example
- **On Windows**
  Install CMake and MsBuild
  ```
  # Build dependencies
  ./build_cortex_onnx.bat

  # Build engine
  mkdir build
  cd build
  cmake ..
  cmake --build . --config Release -j4

  # Build server example (from root repository)
  mkdir -p examples/server/build
  cd examples/server/build
  cmake ..
  cmake --build . --config Release -j4
  ```

# Quickstart
**Step 1: Downloading a Model**

Clone a model from https://huggingface.co/cortexhub, checkout to dml branch

**Step 2: Start server**
- **On Windows**

  ```bash
  cd examples/server/build/Release
  mkdir -p engines\cortex.onnx\
  cp ..\..\..\..\build\Release\engine.dll engines\cortex.onnx\
  cp ..\..\..\..\onnxruntime-genai\build\Release\*.dll .\
  server.exe
  ```

**Step 3: Load model**
```bash title="Load model"
curl http://localhost:3928/loadmodel \
  -H 'Content-Type: application/json' \
  -d '{
    "model_path": "./model/llama3",
    "model_alias": "llama3",
    "system_prompt": "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n",
    "user_prompt": "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n",
    "ai_prompt": "<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n"
  }'
```
**Step 4: Making an Inference**

```bash title="cortex.onnx Inference"
curl http://localhost:3928/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful assistant."
      },
      {
        "role": "user",
        "content": "Who won the world series in 2020?"
      }
    ],
    "model": "llama3"
  }'
```

Table of parameters

| Parameter        | Type    | Description                                                  |
|------------------|---------|--------------------------------------------------------------|
| `model_path` | String  | The file path to the onnx model.                            |