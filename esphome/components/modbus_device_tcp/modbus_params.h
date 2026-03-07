#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace esphome {
namespace modbus_device_tcp {
#endif

/*=====================================================================================
 * Parameter structures for Modbus (coils, discrete inputs, input/holding registers).
 * Sizes are driven by MODBUS_NUM_OBJECTS (set in YAML as num_objects, default 5).
 *====================================================================================*/
#ifndef DEVICE_PARAMS
#define DEVICE_PARAMS

#ifndef MODBUS_NUM_OBJECTS
#define MODBUS_NUM_OBJECTS 5  // NOLINT(cppcoreguidelines-macro-usage) - needed for C array sizes
#endif

#define MODBUS_COIL_BYTES ((MODBUS_NUM_OBJECTS + 7) / 8)

#pragma pack(push, 1)
struct discrete_reg_params_t {
  uint8_t discrete_data[MODBUS_COIL_BYTES];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct coil_reg_params_t {
  uint8_t coil_data[MODBUS_COIL_BYTES];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct input_reg_params_t {
  uint16_t input_regs[MODBUS_NUM_OBJECTS];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct holding_reg_params_t {
  uint16_t holding_regs[MODBUS_NUM_OBJECTS];
};
#pragma pack(pop)

#ifdef __cplusplus
}  // namespace modbus_device_tcp
}  // namespace esphome
#endif

#endif /* DEVICE_PARAMS */
