"""Driver for the real ES7243 microphone ADC (not ES7243E).

ESPHome only ships a driver for ES7243E, which has a completely different
register map from the plain ES7243. These are genuinely different chips
sharing a naming convention, not a revision of the same part — Espressif's
own ESP-ADF maintains two separate driver implementations for them
(components/audio_hal/driver/es7243 vs .../es7243e), and their board.c files
probe the I2C bus at runtime to tell which one is populated.

Confirmed on an ESP32-LyraT-Mini V1.2: the board's own schematic labels the
part "U9 ES7243", and an I2C bus scan finds it at 0x13 (7-bit) — ES7243E's
default address is 0x10; 0x13 is ES7243's (ESP-ADF: ES7243_ADDR=0x26 in
8-bit form = 0x13 in 7-bit).

Register sequence and the MCLK pre-activation pulse train are ported
directly from Espressif's own public driver:
https://github.com/espressif/esp-adf/blob/master/components/audio_hal/driver/es7243/es7243.c
"""

import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_adc import AudioAdc
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MIC_GAIN
from esphome.types import ConfigType

CODEOWNERS = ["@Chorty"]
DEPENDENCIES = ["i2c"]

CONF_MCLK_PIN = "mclk_pin"

es7243_ns = cg.esphome_ns.namespace("es7243")
ES7243 = es7243_ns.class_("ES7243", AudioAdc, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7243),
            # Deliberately a bare GPIO number, not a tracked pin schema. On
            # boards where this line is also the I2S peripheral's MCLK pin
            # (needed to actually clock the codec once it's running),
            # ESPHome's pin-conflict validator has no way to express that
            # kind of intentional handoff: prime it here as a plain output
            # during this component's own early setup, then let the I2S
            # peripheral take over driving it as MCLK. Driven directly via
            # ESP-IDF's gpio_set_direction/gpio_set_level in the priming
            # routine for the same reason, mirroring ESP-ADF's own
            # es7243_mclk_active().
            cv.Required(CONF_MCLK_PIN): cv.int_range(min=0, max=39),
            cv.Optional(CONF_MIC_GAIN, default="27db"): cv.decibel,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x13))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_MCLK_PIN])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
