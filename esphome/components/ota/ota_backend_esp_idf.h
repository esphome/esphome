#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF

#include "ota_component.h"
#include "ota_backend.h"
#include <esp_ota_ops.h>

namespace esphome {
namespace ota {

class IDFOTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;

 private:
  esp_ota_handle_t update_handle_{0};
};

}  // namespace ota
}  // namespace esphome
#endif
