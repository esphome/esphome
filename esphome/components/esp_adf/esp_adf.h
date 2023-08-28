#pragma once

#ifdef USE_ESP_IDF

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_SIZE = 1024;

enum class TaskEventType : uint8_t {
  STARTING = 0,
  STARTED,
  RUNNING,
  STOPPING,
  STOPPED,
  WARNING = 255,
};

struct TaskEvent {
  TaskEventType type;
  esp_err_t err;
};

struct CommandEvent {
  bool stop;
};

struct DataEvent {
  bool stop;
  size_t len;
  uint8_t data[BUFFER_SIZE];
};

class ESPADF;

class ESPADFPipeline : public Parented<ESPADF> {};

class ESPADF : public Component {
 public:
  void setup() override;

  float get_setup_priority() const override;

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

 protected:
  Mutex lock_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
