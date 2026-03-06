#include <stdint.h>

/*=====================================================================================
 * Parameter structures for Modbus slave (coils, discrete inputs, input/holding registers).
 * Sizes are driven by MODBUS_NUM_OBJECTS (set in YAML as num_objects, default 5).
 *====================================================================================*/
#ifndef _DEVICE_PARAMS
#define _DEVICE_PARAMS

#ifndef MODBUS_NUM_OBJECTS
#define MODBUS_NUM_OBJECTS 5
#endif

#define MODBUS_COIL_BYTES ((MODBUS_NUM_OBJECTS + 7) / 8)

#pragma pack(push, 1)
typedef struct {
    uint8_t discrete_data[MODBUS_COIL_BYTES];
} discrete_reg_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t coil_data[MODBUS_COIL_BYTES];
} coil_reg_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t input_regs[MODBUS_NUM_OBJECTS];
} input_reg_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t holding_regs[MODBUS_NUM_OBJECTS];
} holding_reg_params_t;
#pragma pack(pop)

#define _MODBUS_PARAMS_LIST \
  _MODBUS_PARAM(discrete_reg_params_t, discrete_reg_params) \
  _MODBUS_PARAM(coil_reg_params_t, coil_reg_params) \
  _MODBUS_PARAM(input_reg_params_t, input_reg_params) \
  _MODBUS_PARAM(holding_reg_params_t, holding_reg_params)

#ifdef __cplusplus
extern "C" {
#define _MODBUS_PARAM(type, name) extern type name; inline type name = {};
#else
#define _MODBUS_PARAM(type, name) extern type name;
#endif
_MODBUS_PARAMS_LIST
#undef _MODBUS_PARAM
#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_PARAMS */
