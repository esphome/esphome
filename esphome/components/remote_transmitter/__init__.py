import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, remote_base
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_CARRIER_DUTY_PERCENT,
    CONF_CLOCK_RESOLUTION,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_OPEN_DRAIN,
    CONF_PIN,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
    CONF_VALUE,
    PlatformFramework,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["remote_base"]

CONF_EOT_LEVEL = "eot_level"
CONF_NON_BLOCKING = "non_blocking"
CONF_ON_TRANSMIT = "on_transmit"
CONF_ON_COMPLETE = "on_complete"
CONF_TRANSMITTER_ID = remote_base.CONF_TRANSMITTER_ID

remote_transmitter_ns = cg.esphome_ns.namespace("remote_transmitter")
RemoteTransmitterComponent = remote_transmitter_ns.class_(
    "RemoteTransmitterComponent", remote_base.RemoteTransmitterBase, cg.Component
)
DigitalWriteAction = remote_transmitter_ns.class_(
    "DigitalWriteAction",
    automation.Action,
    cg.Parented.template(RemoteTransmitterComponent),
)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RemoteTransmitterComponent),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CARRIER_DUTY_PERCENT): cv.All(
            cv.percentage_int, cv.Range(min=1, max=100)
        ),
        cv.Optional(CONF_CLOCK_RESOLUTION): cv.All(
            cv.only_on_esp32,
            esp32_rmt.validate_clock_resolution(),
        ),
        cv.Optional(CONF_EOT_LEVEL): cv.All(cv.only_on_esp32, cv.boolean),
        cv.Optional(CONF_USE_DMA): cv.All(
            esp32.only_on_variant(
                supported=[esp32.VARIANT_ESP32P4, esp32.VARIANT_ESP32S3]
            ),
            cv.boolean,
        ),
        cv.SplitDefault(
            CONF_RMT_SYMBOLS,
            esp32=64,
            esp32_c3=48,
            esp32_c5=48,
            esp32_c6=48,
            esp32_h2=48,
            esp32_p4=48,
            esp32_s2=64,
            esp32_s3=48,
        ): cv.All(cv.only_on_esp32, cv.int_range(min=2)),
        cv.Optional(CONF_NON_BLOCKING): cv.All(cv.only_on_esp32, cv.boolean),
        cv.Optional(CONF_ON_TRANSMIT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_non_blocking(config):
    if CORE.is_esp32 and CONF_NON_BLOCKING not in config:
        _LOGGER.warning(
            "'non_blocking' is not set for 'remote_transmitter' and will default to 'true'.\n"
            "The default behavior changed in 2025.11.0; previously blocking mode was used.\n"
            "To silence this warning, explicitly set 'non_blocking: true' (or 'false')."
        )
        config[CONF_NON_BLOCKING] = True


FINAL_VALIDATE_SCHEMA = _validate_non_blocking

DIGITAL_WRITE_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterComponent),
        cv.Required(CONF_VALUE): cv.templatable(cv.boolean),
    },
    key=CONF_VALUE,
)


@automation.register_action(
    "remote_transmitter.digital_write", DigitalWriteAction, DIGITAL_WRITE_ACTION_SCHEMA
)
async def digital_write_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_TRANSMITTER_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, bool)
    cg.add(var.set_value(template_))
    return var


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32:
        var = cg.new_Pvariable(config[CONF_ID], pin)
        cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
        cg.add(var.set_non_blocking(config[CONF_NON_BLOCKING]))
        if CONF_CLOCK_RESOLUTION in config:
            cg.add(var.set_clock_resolution(config[CONF_CLOCK_RESOLUTION]))
        if CONF_USE_DMA in config:
            cg.add(var.set_with_dma(config[CONF_USE_DMA]))
        if CONF_EOT_LEVEL in config:
            cg.add(var.set_eot_level(config[CONF_EOT_LEVEL]))
        else:
            cg.add(
                var.set_eot_level(
                    config[CONF_PIN][CONF_MODE][CONF_OPEN_DRAIN]
                    or config[CONF_PIN][CONF_INVERTED]
                )
            )
    else:
        var = cg.new_Pvariable(config[CONF_ID], pin)
    await cg.register_component(var, config)

    cg.add(var.set_carrier_duty_percent(config[CONF_CARRIER_DUTY_PERCENT]))

    if on_transmit_config := config.get(CONF_ON_TRANSMIT):
        await automation.build_automation(
            var.get_transmit_trigger(), [], on_transmit_config
        )

    if on_complete_config := config.get(CONF_ON_COMPLETE):
        await automation.build_automation(
            var.get_complete_trigger(), [], on_complete_config
        )


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "remote_transmitter_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "remote_transmitter.cpp": {
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
        },
    }
)
