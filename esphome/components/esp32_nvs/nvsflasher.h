#pragma once

#include "esphome/core/component.h"

#ifdef ARDUINO_ARCH_ESP32

#include "nvs.h"
#include "nvs_flash.h"

//debug to dump nvs to serial port
extern "C" void nvs_dump(const char *partName);

namespace esphome {
namespace esp32_nvs {

  static const char *TAG1 = "nvsflasher";

  enum NvsResultBfrType { NONE=0, READ_BFR=1, WRITE_BFR=2 };

// ref: docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html

// all of the esp32_nvs templates can share the actual nvs interface functions
// rather than duplicate them in the Esp32NvsComponent class
class NvsFlasher {
public:

  // if overwrite==true, create or update the flash key/value with the wr_bfr
  // if overwrite==false:
  //   if the key is already in nvs, read the value into rd_bfr
  //   if the key is not in nvs, write the key/value to flash from wr_bfr
  static NvsResultBfrType reconcile_flash(const char* nvs_namespace, const char* id,
      unsigned char *rd_bfr, uint32_t rd_len, unsigned char *wr_bfr, uint32_t wr_len, bool overwrite) {
    NvsResultBfrType result = NONE; //default to 'failed'
    nvs_handle handle;
    //ESP_LOGI(TAG1, "reconcile_flash: starting");
    esp_err_t err = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if(err) {
      ESP_LOGE(TAG1, "reconcile_flash: nvs_open(%s) failed: %s", nvs_namespace, esp_err_to_name(err));
    }
    else {
      // opened OK
      bool do_overwrite = false;
      err = nvs_get_blob(handle, id, rd_bfr, &rd_len);
      if(err != ESP_OK)
        do_overwrite = true;  //it's not already flashed
      else {
        // there is already a value in flash
        //ESP_LOGI(TAG1, "reconcile_flash: nvs_get_blob(%s/%s) len %d already exists", nvs_namespace, id, rd_len);
        //ESP_LOG_BUFFER_HEXDUMP(TAG1, rd_bfr, rd_len, ESP_LOG_ERROR); //must be level ERROR or it won't print
        if(!overwrite)
          result = READ_BFR; //we are done, rd_bfr to be copied to this->value_
        else {
          // overwrite is true
          // copy wr_bfr to flash unless it is already there
          if (rd_len != wr_len)
            do_overwrite = true;
          else if(memcmp(rd_bfr, wr_bfr, wr_len) != 0)
            do_overwrite = true;
        }
      }
      if(do_overwrite) {
        // new or overwrite namespace/key
        err = nvs_set_blob(handle, id, wr_bfr, wr_len);
        if(err) {
          ESP_LOGE(TAG1, "reconcile_flash: nvs_set_blob(%s/%s): %s", nvs_namespace, id, esp_err_to_name(err));
        }
        else {
          // wrote OK
          //ESP_LOGI(TAG1, "reconcile_flash: nvs_set_blob(%s/%s) success", nvs_namespace_.c_str(), id_.c_str());
          err = nvs_commit(handle);
          if(err) {
            ESP_LOGE(TAG1, "reconcile_flash: nvs_commit(%s/%s): %s", nvs_namespace, id, esp_err_to_name(err));
          }
          else {
            //ESP_LOGI(TAG1, "reconcile_flash: nvs_commit(%s/%s) success", nvs_namespace, id);
            //nvs_dump("nvs");
            //ESP_LOG_BUFFER_HEXDUMP(TAG1, wr_bfr, wr_len, ESP_LOG_ERROR);
            result = WRITE_BFR; //wr_bfr gets written to this->value_
          }
        }
      }
      nvs_close(handle);
    }
    return result;
  }

  // returns the size of nvs_namespace/id if it already resides in flash
  static uint32_t get_required_size(const char* nvs_namespace, const char* id) {
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    nvs_handle handle;
    esp_err_t err = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if(err) {
      ESP_LOGE(TAG1, "get_required_size: nvs_open(%s) failed: %s", nvs_namespace, esp_err_to_name(err));
    }
    else {
      // Read the size of memory space required for blob
      err = nvs_get_blob(handle, id, NULL, &required_size);
      if(err)
        required_size = 0;
      nvs_close(handle);
    }
    //ESP_LOGI(TAG1, "get_required_size: returning %d", required_size);
    return required_size;
  }

  // erases the entire namespace
  static void erase_namespace(const char* nvs_namespace) {
    nvs_handle handle;
    esp_err_t err = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if(err) {
      ESP_LOGE(TAG1, "erase_namespace: nvs_open(%s) failed: %s", nvs_namespace, esp_err_to_name(err));
    }
    else {
      err = nvs_erase_all(handle);
      if(err) {
        ESP_LOGE(TAG1, "erase_namespace: nvs_erase_all(%s) failed: %s", nvs_namespace, esp_err_to_name(err));
      }
      else {
        nvs_commit(handle);
        nvs_close(handle);
        // nvs_dump("nvs");
      }
    }
  }

};

}   //esp32_nvs
}   //esphome

#endif //ARDUINO_ARCH_ESP32
