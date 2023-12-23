import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, uart
from esphome.const import (
    CONF_CHANNELS,
    CONF_GAMMA_CORRECT,
    CONF_OUTPUT_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_SENSING_PIN,
)


CODEOWNERS = ["@michau-krakow"]
DEPENDENCIES = ["uart", "light"]

oxt_dimmer_ns = cg.esphome_ns.namespace("oxt_dimmer")
OxtController = oxt_dimmer_ns.class_("OxtController", cg.Component, uart.UARTDevice)
OxtDimmerChannel = oxt_dimmer_ns.class_(
    "OxtDimmerChannel", cg.Component, light.LightOutput
)

CHANNEL_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(OxtDimmerChannel),
        cv.Optional(CONF_MIN_VALUE, default=50): cv.int_range(min=0, max=255),
        cv.Optional(CONF_MAX_VALUE, default=255): cv.int_range(min=0, max=255),
        cv.Optional(CONF_SENSING_PIN): pins.gpio_input_pin_schema,
        # override defaults
        cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
    }
)

CONFIG_SCHEMA = (
    # allow at least 15ms for UART frame of 10-12 bytes @9600bps to be fully transmitted!
    cv.polling_component_schema("50 ms")
    .extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(OxtController),
            cv.Required(CONF_CHANNELS): cv.All(
                cv.ensure_list(CHANNEL_SCHEMA), cv.Length(min=1, max=2)
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "oxt_dimmer", baud_rate=9600, require_tx=True, require_rx=False
)


async def to_code(config):
    ctrl = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(ctrl, config)
    await uart.register_uart_device(ctrl, config)

    for index, channel_cfg in enumerate(config[CONF_CHANNELS]):
        channel = cg.new_Pvariable(channel_cfg[CONF_OUTPUT_ID])
        await cg.register_component(channel, channel_cfg)
        await light.register_light(channel, channel_cfg)
        cg.add(channel.set_min_value(channel_cfg[CONF_MIN_VALUE]))
        cg.add(channel.set_max_value(channel_cfg[CONF_MAX_VALUE]))
        if CONF_SENSING_PIN in channel_cfg:
            sensing_pin = await cg.gpio_pin_expression(channel_cfg[CONF_SENSING_PIN])
            cg.add(channel.set_sensing_pin(sensing_pin))
        cg.add(ctrl.add_channel(index, channel))
