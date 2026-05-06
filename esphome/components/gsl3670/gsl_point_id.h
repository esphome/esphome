#pragma once

#include <cstdint>
namespace esphome {
namespace gsl3670 {

struct gsl_touch_info {
  int x[10];
  int y[10];
  int id[10];
  int finger_num;
};

uint32_t gsl_mask_tiaoping();
uint32_t gsl_version_id();
void gsl_alg_id_main(gsl_touch_info *cinfo);
void gsl_DataInit(const uint32_t *conf_in);

}  // namespace gsl3670
}  // namespace esphome
