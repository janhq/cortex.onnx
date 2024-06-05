#pragma once
#include <memory.h>
#include <memory>
#include <string>
#include "cortex-common/enginei.h"
#include "json/value.h"
#include "ort_genai.h"
#include "ort_genai_c.h"

namespace cortex_onnx {
class OnnxEngine : public EngineI {
 public:
  OnnxEngine();
  void HandleChatCompletion(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;
  void HandleEmbedding(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;
  void LoadModel(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;
  void UnloadModel(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;
  void GetModelStatus(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;

  // API to get running models.
  void GetModels(
      std::shared_ptr<Json::Value> json_body,
      std::function<void(Json::Value&&, Json::Value&&)>&& callback) final;

 private:
  bool CheckModelLoaded(
      std::function<void(Json::Value&&, Json::Value&&)>& callback);

 private:
  std::unique_ptr<OgaHandle> handle_;
  std::unique_ptr<OgaModel> oga_model_ = nullptr;
  std::unique_ptr<OgaTokenizer> tokenizer_ = nullptr;
  std::unique_ptr<OgaTokenizerStream> tokenizer_stream_ = nullptr;
  std::atomic<bool> model_loaded_;
  std::string user_prompt_;
  std::string ai_prompt_;
  std::string system_prompt_;
  std::string pre_prompt_;
  std::string model_id_;
  uint64_t start_time_;
};
}  // namespace cortex_onnx