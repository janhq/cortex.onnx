#include "onnx_engine.h"
#include <signal.h>
#include <random>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include "chat_completion_request.h"
#include "json/writer.h"
#include "trantor/utils/Logger.h"

namespace cortex_onnx {
namespace {
constexpr const int k200OK = 200;
constexpr const int k400BadRequest = 400;
constexpr const int k409Conflict = 409;
constexpr const int k500InternalServerError = 500;

Json::Value CreateFullReturnJson(const std::string& id,
                                 const std::string& model,
                                 const std::string& content,
                                 const std::string& system_fingerprint,
                                 int prompt_tokens, int completion_tokens,
                                 Json::Value finish_reason = Json::Value()) {
  Json::Value root;

  root["id"] = id;
  root["model"] = model;
  root["created"] = static_cast<int>(std::time(nullptr));
  root["object"] = "chat.completion";
  root["system_fingerprint"] = system_fingerprint;

  Json::Value choicesArray(Json::arrayValue);
  Json::Value choice;

  choice["index"] = 0;
  Json::Value message;
  message["role"] = "assistant";
  message["content"] = content;
  choice["message"] = message;
  choice["finish_reason"] = finish_reason;

  choicesArray.append(choice);
  root["choices"] = choicesArray;

  Json::Value usage;
  usage["prompt_tokens"] = prompt_tokens;
  usage["completion_tokens"] = completion_tokens;
  usage["total_tokens"] = prompt_tokens + completion_tokens;
  root["usage"] = usage;

  return root;
}

std::string CreateReturnJson(const std::string& id, const std::string& model,
                             const std::string& content,
                             Json::Value finish_reason = Json::Value()) {
  Json::Value root;

  root["id"] = id;
  root["model"] = model;
  root["created"] = static_cast<int>(std::time(nullptr));
  root["object"] = "chat.completion.chunk";

  Json::Value choicesArray(Json::arrayValue);
  Json::Value choice;

  choice["index"] = 0;
  Json::Value delta;
  delta["content"] = content;
  choice["delta"] = delta;
  choice["finish_reason"] = finish_reason;

  choicesArray.append(choice);
  root["choices"] = choicesArray;

  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";  // This sets the indentation to an empty string,
                               // producing compact output.
  return Json::writeString(writer, root);
}

std::string GenerateRandomString(std::size_t length) {
  const std::string characters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  std::random_device rd;
  std::mt19937 generator(rd());

  std::uniform_int_distribution<> distribution(
      0, static_cast<int>(characters.size()) - 1);

  std::string random_string(length, '\0');
  std::generate_n(random_string.begin(), length,
                  [&]() { return characters[distribution(generator)]; });

  return random_string;
}

}  // namespace

OnnxEngine::OnnxEngine() {
  handle_ = std::make_unique<OgaHandle>();
}

void OnnxEngine::LoadModel(
    std::shared_ptr<Json::Value> jsonBody,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  auto path = jsonBody->get("model_path", "").asString();
  user_prompt_ = jsonBody->get("user_prompt", "USER: ").asString();
  ai_prompt_ = jsonBody->get("ai_prompt", "ASSISTANT: ").asString();
  system_prompt_ =
      jsonBody->get("system_prompt", "ASSISTANT's RULE: ").asString();
  pre_prompt_ = jsonBody->get("pre_prompt", "").asString();
  try {
    std::cout << "Creating model..." << std::endl;
    oga_model_ = OgaModel::Create(path.c_str());
    std::cout << "Creating tokenizer..." << std::endl;
    tokenizer_ = OgaTokenizer::Create(*oga_model_);
    tokenizer_stream_ = OgaTokenizerStream::Create(*tokenizer_);
    Json::Value json_resp;
    json_resp["message"] = "Model loaded successfully";
    Json::Value status;
    status["is_done"] = true;
    status["has_error"] = false;
    status["is_stream"] = false;
    status["status_code"] = k200OK;
    callback(std::move(status), std::move(json_resp));
    LOG_INFO << "Model loaded successfully: " << path;
    model_loaded_ = true;
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
    oga_model_.reset();
    tokenizer_.reset();
    tokenizer_stream_.reset();
    Json::Value json_resp;
    json_resp["message"] = "Failed to load model";
    Json::Value status;
    status["is_done"] = false;
    status["has_error"] = true;
    status["is_stream"] = false;
    status["status_code"] = k500InternalServerError;
    callback(std::move(status), std::move(json_resp));
  }
}

void OnnxEngine::HandleChatCompletion(
    std::shared_ptr<Json::Value> jsonBody,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  if (!CheckModelLoaded(callback))
    return;
  auto req = onnx::inferences::fromJson(jsonBody);
  auto is_stream = jsonBody->get("stream", false).asBool();

  std::string formatted_output;
  for (const auto& message : req.messages) {
    std::string input_role = message["role"].asString();
    std::string role;
    if (input_role == "user") {
      role = user_prompt_;
      std::string content = message["content"].asString();
      formatted_output += role + content;
    } else if (input_role == "assistant") {
      role = ai_prompt_;
      std::string content = message["content"].asString();
      formatted_output += role + content;
    } else if (input_role == "system") {
      role = system_prompt_;
      std::string content = message["content"].asString();
      formatted_output = role + content + formatted_output;
    } else {
      role = input_role;
      std::string content = message["content"].asString();
      formatted_output += role + content;
    }
  }
  formatted_output += ai_prompt_;

  LOG_DEBUG << formatted_output;

  try {
    if (req.stream) {
      auto sequences = OgaSequences::Create();
      tokenizer_->Encode(formatted_output.c_str(), *sequences);

      auto params = OgaGeneratorParams::Create(*oga_model_);
      // TODO(sang)
      params->SetSearchOption("max_length", req.max_tokens);
      params->SetSearchOption("top_p", req.top_p);
      params->SetSearchOption("temperature", req.temperature);
      // params->SetSearchOption("repetition_penalty", 0.95);
      params->SetInputSequences(*sequences);

      auto generator = OgaGenerator::Create(*oga_model_, *params);
      while (!generator->IsDone()) {
        generator->ComputeLogits();
        generator->GenerateNextToken();

        const int32_t num_tokens = generator->GetSequenceCount(0);
        int32_t new_token = generator->GetSequenceData(0)[num_tokens - 1];
        auto out_string = tokenizer_stream_->Decode(new_token);
        std::cout << out_string;
        const std::string str =
            "data: " + CreateReturnJson(GenerateRandomString(20), "_", out_string) + "\n\n";
        Json::Value resp_data;
        resp_data["data"] = str;
        Json::Value status;
        status["is_done"] = false;
        status["has_error"] = false;
        status["is_stream"] = true;
        status["status_code"] = k200OK;
        callback(std::move(status), std::move(resp_data));
      }

      LOG_INFO << "End of result";
      Json::Value resp_data;
      const std::string str =
          "data: " + CreateReturnJson("gdsf", "_", "", "stop") + "\n\n" +
          "data: [DONE]" + "\n\n";
      resp_data["data"] = str;
      Json::Value status;
      status["is_done"] = true;
      status["has_error"] = false;
      status["is_stream"] = true;
      status["status_code"] = k200OK;
      callback(std::move(status), std::move(resp_data));
    } else {
      auto sequences = OgaSequences::Create();
      tokenizer_->Encode(formatted_output.c_str(), *sequences);

      auto params = OgaGeneratorParams::Create(*oga_model_);
      params->SetSearchOption("max_length", req.max_tokens);
      params->SetSearchOption("top_p", req.top_p);
      params->SetSearchOption("temperature", req.temperature);
      params->SetSearchOption("repetition_penalty", req.frequency_penalty);
      params->SetInputSequences(*sequences);

      auto output_sequences = oga_model_->Generate(*params);
      const auto output_sequence_length = output_sequences->SequenceCount(0);
      const auto* output_sequence_data = output_sequences->SequenceData(0);
      auto out_string =
          tokenizer_->Decode(output_sequence_data, output_sequence_length);

      std::cout << "Output: " << std::endl << out_string << std::endl;

      // TODO(sang)
      std::string to_send = out_string.p_;
      auto resp_data = CreateFullReturnJson(GenerateRandomString(20), "_", to_send, "_", 0, 0);
      Json::Value status;
      status["is_done"] = true;
      status["has_error"] = false;
      status["is_stream"] = false;
      status["status_code"] = k200OK;
      callback(std::move(status), std::move(resp_data));
    }
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
    Json::Value json_resp;
    json_resp["message"] = "Error during inference";
    Json::Value status;
    status["is_done"] = false;
    status["has_error"] = true;
    status["is_stream"] = false;
    status["status_code"] = k500InternalServerError;
    callback(std::move(status), std::move(json_resp));
  }
}

void OnnxEngine::HandleEmbedding(
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  LOG_WARN << "Engine does not support embedding yet";
}

void OnnxEngine::UnloadModel(
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  if (!CheckModelLoaded(callback))
    return;
  oga_model_.reset();
  tokenizer_.reset();
  tokenizer_stream_.reset();
  model_loaded_ = false;

  Json::Value json_resp;
  json_resp["message"] = "Model unloaded successfully";
  Json::Value status;
  status["is_done"] = true;
  status["has_error"] = false;
  status["is_stream"] = false;
  status["status_code"] = k200OK;
  callback(std::move(status), std::move(json_resp));
  LOG_INFO << "Model unloaded sucessfully";
}
void OnnxEngine::GetModelStatus(
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  Json::Value json_resp;
  json_resp["message"] = "Engine does not support get model status method yet";
  Json::Value status;
  status["is_done"] = false;
  status["has_error"] = true;
  status["is_stream"] = false;
  status["status_code"] = k409Conflict;
  callback(std::move(status), std::move(json_resp));
  LOG_WARN << "Engine does not support get model status method yet";
}

// API to get running models.
void OnnxEngine::GetModels(
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  Json::Value json_resp;
  json_resp["message"] = "Engine does not support get models method yet";
  Json::Value status;
  status["is_done"] = false;
  status["has_error"] = true;
  status["is_stream"] = false;
  status["status_code"] = k409Conflict;
  callback(std::move(status), std::move(json_resp));
  LOG_WARN << "Engine does not support get models method yet";
}

bool OnnxEngine::CheckModelLoaded(
    std::function<void(Json::Value&&, Json::Value&&)>& callback) {
  if (!model_loaded_) {
    LOG_WARN << "Error: model is not loaded yet";
    Json::Value json_resp;
    json_resp["message"] =
        "Model has not been loaded, please load model into cortex.onnx";
    Json::Value status;
    status["is_done"] = false;
    status["has_error"] = true;
    status["is_stream"] = false;
    status["status_code"] = k409Conflict;
    callback(std::move(status), std::move(json_resp));
    return false;
  }
  return true;
}

}  // namespace cortex_onnx

extern "C" {
EngineI* get_engine() {
  return new cortex_onnx::OnnxEngine();
}
}