#pragma once

#ifdef USE_ESP32
#ifndef USE_I2S_LEGACY
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace snapclient {

static const int HTTP_TASK_PRIORITY = 9;  // 9
static const int HTTP_TASK_CORE_ID = tskNO_AFFINITY;

typedef struct audioDACdata_s {
  bool mute;
  int volume;
} audioDACdata_t;

void http_get_task(void *pvParameters);
void init_snapcast(QueueHandle_t audioQHdl, const char *name, const char *host, uint16_t port);
extern "C" void audio_set_mute(bool mute);
void audio_set_volume(int volume);

}  // namespace snapclient
}  // namespace esphome
#endif
#endif
