from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32_rmt, remote_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_CARRIER_DUTY_PERCENT,
    CONF_CLOCK_DIVIDER,
    CONF_CLOCK_RESOLUTION,
    CONF_ID,
    CONF_INVERTED,
    CONF_PIN,
    CONF_RMT_CHANNEL,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
)
from esphome.core import CORE

AUTO_LOAD = ["remote_base"]

CONF_EOT_LEVEL = "eot_level"
CONF_ON_TRANSMIT = "on_transmit"
CONF_ON_COMPLETE = "on_complete"
CONF_ONE_WIRE = "one_wire"

remote_transmitter_ns = cg.esphome_ns.namespace("remote_transmitter")
RemoteTransmitterComponent = remote_transmitter_ns.class_(
    "RemoteTransmitterComponent", remote_base.RemoteTransmitterBase, cg.Component
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
            cv.only_with_esp_idf,
            esp32_rmt.validate_clock_resolution(),
        ),
        cv.Optional(CONF_CLOCK_DIVIDER): cv.All(
            cv.only_on_esp32, cv.only_with_arduino, cv.int_range(min=1, max=255)
        ),
        cv.Optional(CONF_EOT_LEVEL): cv.All(cv.only_with_esp_idf, cv.boolean),
        cv.Optional(CONF_ONE_WIRE): cv.All(cv.only_with_esp_idf, cv.boolean),
        cv.Optional(CONF_USE_DMA): cv.All(cv.only_with_esp_idf, cv.boolean),
        cv.SplitDefault(
            CONF_RMT_SYMBOLS,
            esp32_idf=64,
            esp32_s2_idf=64,
            esp32_s3_idf=48,
            esp32_c3_idf=48,
            esp32_c6_idf=48,
            esp32_h2_idf=48,
        ): cv.All(cv.only_with_esp_idf, cv.int_range(min=2)),
        cv.Optional(CONF_RMT_CHANNEL): cv.All(
            cv.only_with_arduino, esp32_rmt.validate_rmt_channel(tx=True)
        ),
        cv.Optional(CONF_ON_TRANSMIT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32:
        if esp32_rmt.use_new_rmt_driver():
            var = cg.new_Pvariable(config[CONF_ID], pin)
            cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
            if CONF_CLOCK_RESOLUTION in config:
                cg.add(var.set_clock_resolution(config[CONF_CLOCK_RESOLUTION]))
            if CONF_USE_DMA in config:
                cg.add(var.set_with_dma(config[CONF_USE_DMA]))
            if CONF_ONE_WIRE in config:
                cg.add(var.set_one_wire(config[CONF_ONE_WIRE]))
            if CONF_EOT_LEVEL in config:
                cg.add(var.set_eot_level(config[CONF_EOT_LEVEL]))
            elif CONF_ONE_WIRE in config and config[CONF_ONE_WIRE]:
                cg.add(var.set_eot_level(True))
            elif CONF_INVERTED in config[CONF_PIN] and config[CONF_PIN][CONF_INVERTED]:
                cg.add(var.set_eot_level(True))
        else:
            if (rmt_channel := config.get(CONF_RMT_CHANNEL, None)) is not None:
                var = cg.new_Pvariable(config[CONF_ID], pin, rmt_channel)
            else:
                var = cg.new_Pvariable(config[CONF_ID], pin)
            if CONF_CLOCK_DIVIDER in config:
                cg.add(var.set_clock_divider(config[CONF_CLOCK_DIVIDER]))

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
