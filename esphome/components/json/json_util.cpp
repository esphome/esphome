#include "json_util.h"
#include "esphome/core/log.h"

// ArduinoJson::Allocator is included via ArduinoJson.h in json_util.h

namespace esphome {
namespace json {

static const char *const TAG = "json";

#ifdef USE_PSRAM
// Build an allocator for the JSON Library using the RAMAllocator class
// This is only compiled when PSRAM is enabled
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return this->allocator_.allocate(size); }

  void deallocate(void *pointer) override {
    // ArduinoJson's Allocator interface doesn't provide the size parameter in deallocate.
    // RAMAllocator::deallocate() requires the size, which we don't have access to here.
    // RAMAllocator::deallocate implementation just calls free() regardless of whether
    // the memory was allocated with heap_caps_malloc or malloc.
    // This is safe because ESP-IDF's heap implementation internally tracks the memory region
    // and routes free() to the appropriate heap.
    free(pointer);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

  void *reallocate(void *ptr, size_t new_size) override {
    return this->allocator_.reallocate(static_cast<uint8_t *>(ptr), new_size);
  }

 protected:
  RAMAllocator<uint8_t> allocator_{RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE)};
};
#endif

std::string build_json(const json_build_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
#ifdef USE_PSRAM
  auto doc_allocator = SpiRamAllocator();
  JsonDocument json_document(&doc_allocator);
#else
  JsonDocument json_document;
#endif
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return "{}";
  }
  JsonObject root = json_document.to<JsonObject>();
  f(root);
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return "{}";
  }
  std::string output;
  serializeJson(json_document, output);
  return output;
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

bool parse_json(const std::string &data, const json_parse_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
#ifdef USE_PSRAM
  auto doc_allocator = SpiRamAllocator();
  JsonDocument json_document(&doc_allocator);
#else
  JsonDocument json_document;
#endif
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return false;
  }
  DeserializationError err = deserializeJson(json_document, data);

  JsonObject root = json_document.as<JsonObject>();

  if (err == DeserializationError::Ok) {
    return f(root);
  } else if (err == DeserializationError::NoMemory) {
    ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
    return false;
  }
  ESP_LOGE(TAG, "Parse error: %s", err.c_str());
  return false;
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

// JsonBuilder implementation
class JsonBuilder::Impl {
 public:
  Impl() {
#ifdef USE_PSRAM
    allocator_ = std::make_unique<SpiRamAllocator>();
    doc_ = std::make_unique<JsonDocument>(allocator_.get());
#else
    doc_ = std::make_unique<JsonDocument>();
#endif
  }

  JsonObject root() {
    if (!root_created_) {
      root_ = doc_->to<JsonObject>();
      root_created_ = true;
    }
    return root_;
  }

  bool overflowed() const { return doc_->overflowed(); }

  void serialize_to(std::string &output) { serializeJson(*doc_, output); }

 private:
#ifdef USE_PSRAM
  std::unique_ptr<SpiRamAllocator> allocator_;
#endif
  std::unique_ptr<JsonDocument> doc_;
  JsonObject root_;
  bool root_created_{false};
};

JsonBuilder::JsonBuilder() : impl_(std::make_unique<Impl>()) {}
JsonBuilder::~JsonBuilder() = default;

JsonObject JsonBuilder::root() { return impl_->root(); }

std::string JsonBuilder::serialize() {
  if (impl_->overflowed()) {
    ESP_LOGE(TAG, "JSON document overflow");
    return "{}";
  }
  std::string output;
  impl_->serialize_to(output);
  return output;
}

}  // namespace json
}  // namespace esphome
