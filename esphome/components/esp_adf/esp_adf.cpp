#include "esp_adf.h"

#ifdef USE_ESP_IDF

#include <board.h>

#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_adf";

void ESPADF::setup() {
  ESP_LOGI(TAG, "Start codec chip");

  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
}

float ESPADF::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
