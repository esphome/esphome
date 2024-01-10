#include "dfu.h"
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>  // for Serial

extern "C" {

volatile bool goto_dfu = false;
void __wrap_tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void) rts;

  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (itf == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);

      if (coding.bit_rate == 1200) {
        goto_dfu = true;
      }
    }
  }
}
}

namespace esphome {
namespace dfu {

#define DFU_DBL_RESET_MEM 0x20007F7C
#define DFU_DBL_RESET_MAGIC 0x5A1AD5  // SALADS
uint32_t *dbl_reset_mem = ((uint32_t *) DFU_DBL_RESET_MEM);

void DeviceFirmwareUpdate::loop() {
  if (goto_dfu) {
    goto_dfu = false;
    (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
    pinMode(14, OUTPUT);
    pinMode(16, OUTPUT);
    digitalWrite(14, 1);
    digitalWrite(16, 1);

    delay(50);
    digitalWrite(14, 0);
    digitalWrite(16, 0);
  }
}

}  // namespace dfu
}  // namespace esphome
