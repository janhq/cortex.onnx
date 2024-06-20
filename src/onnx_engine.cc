#include "onnx_engine.h"
#include <signal.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
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

inline std::string GetModelId(const Json::Value& json_body) {
  // First check if model exists in request
  if (!json_body["model"].isNull()) {
    return json_body["model"].asString();
  } else if (!json_body["model_alias"].isNull()) {
    return json_body["model_alias"].asString();
  }

  // We check llama_model_path for loadmodel request
  auto input = json_body["model_path"];
  if (!input.isNull()) {
    auto s = input.asString();
    std::replace(s.begin(), s.end(), '\\', '/');
    auto const pos = s.find_last_of('/');
    return s.substr(pos + 1);
  }
  return {};
}

}  // namespace

OnnxEngine::OnnxEngine() {
  handle_ = std::make_unique<OgaHandle>();
}

void OnnxEngine::LoadModel(
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  auto path = json_body->get("model_path", "").asString();
  user_prompt_ = json_body->get("user_prompt", "USER: ").asString();
  ai_prompt_ = json_body->get("ai_prompt", "ASSISTANT: ").asString();
  system_prompt_ =
      json_body->get("system_prompt", "ASSISTANT's RULE: ").asString();
  pre_prompt_ = json_body->get("pre_prompt", "").asString();
  max_history_chat_ = json_body->get("max_history_chat", 2).asInt();
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
    model_id_ = GetModelId(*json_body);
    LOG_INFO << "Model loaded successfully: " << path
             << ", model_id: " << model_id_;
    model_loaded_ = true;
    start_time_ = std::chrono::system_clock::now().time_since_epoch() /
                  std::chrono::milliseconds(1);
    if (q_ == nullptr) {
      q_ = std::make_unique<trantor::ConcurrentTaskQueue>(1, model_id_);
    }
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
    std::shared_ptr<Json::Value> json_body,
    std::function<void(Json::Value&&, Json::Value&&)>&& callback) {
  if (!CheckModelLoaded(callback))
    return;
  auto req = onnx::inferences::fromJson(json_body);
  auto is_stream = json_body->get("stream", false).asBool();

  std::string formatted_output = pre_prompt_;
  
  int history_max = max_history_chat_ * 2; // both user and assistant
  int index = 0;
  for (const auto& message : req.messages) {
    std::string input_role = message["role"].asString();
    std::string role;
    if (input_role == "user") {
      role = user_prompt_;
      std::string content = message["content"].asString();
      if (index > static_cast<int>(req.messages.size()) - history_max) {
        formatted_output += role + content;
      }
    } else if (input_role == "assistant") {
      role = ai_prompt_;
      std::string content = message["content"].asString();
      if (index > static_cast<int>(req.messages.size()) - history_max) {
        formatted_output += role + content;
      }
    } else if (input_role == "system") {
      role = system_prompt_;
      std::string content = message["content"].asString();
      formatted_output = role + content + formatted_output;
    } else {
      role = input_role;
      std::string content = message["content"].asString();
      formatted_output += role + content;
      LOG_WARN << "Should specify input_role";
    }
    index++;
  }
  formatted_output += ai_prompt_;

  // LOG_DEBUG << formatted_output;
  q_->runTaskInQueue([this, cb = std::move(callback),
                      fo = std::move(formatted_output), req] {
    try {
      if (req.stream) {
        auto sequences = OgaSequences::Create();
        tokenizer_->Encode(fo.c_str(), *sequences);

        auto params = OgaGeneratorParams::Create(*oga_model_);
        // TODO(sang)
        params->SetSearchOption("max_length", req.max_tokens);
        params->SetSearchOption("top_p", req.top_p);
        params->SetSearchOption("temperature", req.temperature);
        // params->SetSearchOption("repetition_penalty", 0.95);
        params->SetInputSequences(*sequences);

        auto generator = OgaGenerator::Create(*oga_model_, *params);
        auto start = std::chrono::system_clock::now();
        double generated_tokens = 0;
        while (!generator->IsDone() && model_loaded_) {
          generator->ComputeLogits();
          generator->GenerateNextToken();

          const int32_t num_tokens = generator->GetSequenceCount(0);
          int32_t new_token = generator->GetSequenceData(0)[num_tokens - 1];
          auto out_string = tokenizer_stream_->Decode(new_token);
          // std::cout << out_string;
          const std::string str =
              "data: " +
              CreateReturnJson(GenerateRandomString(20), "_", out_string) +
              "\n\n";
          Json::Value resp_data;
          resp_data["data"] = str;
          Json::Value status;
          status["is_done"] = false;
          status["has_error"] = false;
          status["is_stream"] = true;
          status["status_code"] = k200OK;
          cb(std::move(status), std::move(resp_data));
          generated_tokens++;
        }

        if (!model_loaded_) {
          LOG_WARN << "Model unloaded during inference";
          Json::Value respData;
          respData["data"] = std::string();
          Json::Value status;
          status["is_done"] = false;
          status["has_error"] = true;
          status["is_stream"] = true;
          status["status_code"] = k200OK;
          cb(std::move(status), std::move(respData));
          return;
        }
        auto end = std::chrono::system_clock::now();
        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        LOG_DEBUG << "Generated tokens per second: "
                  << generated_tokens / duration_ms * 1000;

        LOG_INFO << "End of result";
        Json::Value resp_data;
        const std::string str =
            "data: " +
            CreateReturnJson(GenerateRandomString(20), "_", "", "stop") +
            "\n\n" + "data: [DONE]" + "\n\n";
        resp_data["data"] = str;
        Json::Value status;
        status["is_done"] = true;
        status["has_error"] = false;
        status["is_stream"] = true;
        status["status_code"] = k200OK;
        cb(std::move(status), std::move(resp_data));

      } else {
        auto sequences = OgaSequences::Create();
        tokenizer_->Encode(fo.c_str(), *sequences);

        auto params = OgaGeneratorParams::Create(*oga_model_);
        params->SetSearchOption("max_length", req.max_tokens);
        params->SetSearchOption("top_p", req.top_p);
        params->SetSearchOption("temperature", req.temperature);
        // params->SetSearchOption("repetition_penalty", req.frequency_penalty);
        params->SetInputSequences(*sequences);

        auto start = std::chrono::system_clock::now();
        auto output_sequences = oga_model_->Generate(*params);
        const auto output_sequence_length =
            output_sequences->SequenceCount(0) - sequences->SequenceCount(0);
        const auto* output_sequence_data =
            output_sequences->SequenceData(0) + sequences->SequenceCount(0);
        auto out_string =
            tokenizer_->Decode(output_sequence_data, output_sequence_length);

        // std::cout << "Output: " << std::endl << out_string << std::endl;
        auto end = std::chrono::system_clock::now();
        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        LOG_DEBUG << "Generated tokens per second: "
                  << static_cast<double>(output_sequence_length) / duration_ms *
                         1000;

        std::string to_send = out_string.p_;
        auto resp_data = CreateFullReturnJson(GenerateRandomString(20), "_",
                                              to_send, "_", 0, 0);
        Json::Value status;
        status["is_done"] = true;
        status["has_error"] = false;
        status["is_stream"] = false;
        status["status_code"] = k200OK;
        cb(std::move(status), std::move(resp_data));
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
      cb(std::move(status), std::move(json_resp));
    }
  });
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
  Json::Value model_array = Json::arrayValue;

  if (model_loaded_) {
    Json::Value val;
    val["id"] = model_id_;
    val["engine"] = "cortex.onnx";
    val["start_time"] = start_time_;
    val["vram"] = "-";
    val["ram"] = "-";
    val["object"] = "model";
    model_array.append(val);
  }

  json_resp["object"] = "list";
  json_resp["data"] = model_array;

  Json::Value status;
  status["is_done"] = true;
  status["has_error"] = false;
  status["is_stream"] = false;
  status["status_code"] = k200OK;
  callback(std::move(status), std::move(json_resp));
  LOG_INFO << "Running models responded";
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