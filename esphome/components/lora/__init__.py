import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, text_sensor, uart
from esphome.const import *

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart", "sensor", "text_sensor"]
MULTI_CONF = True

lora_ns = cg.esphome_ns.namespace("lora")
Lora = lora_ns.class_("Lora", cg.Component, uart.UARTDevice)
LoraGPIOPin = lora_ns.class_("LoraGPIOPin", cg.GPIOPin)
CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_LORA_MESSAGE = "lora_message"
CONF_LORA_RSSI = "lora_rssi"
CONF_LORA = "lora"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Lora),
            # for communication to let us know that we can receive data
            cv.Required(CONF_PIN_AUX): pins.gpio_input_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M0): pins.gpio_output_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M1): pins.gpio_output_pin_schema,
            # if you want to see the raw messages
            cv.Optional(CONF_LORA_MESSAGE): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_NONE,
            ),
            # if you want to see the rssi
            cv.Optional(CONF_LORA_RSSI): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    # check values every 20s
    .extend(cv.polling_component_schema("20s")).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    p = await cg.gpio_pin_expression(config[CONF_PIN_AUX])
    cg.add(var.set_pin_aux(p))

    p = await cg.gpio_pin_expression(config[CONF_PIN_M0])
    cg.add(var.set_pin_m0(p))
    p = await cg.gpio_pin_expression(config[CONF_PIN_M1])
    cg.add(var.set_pin_m1(p))

    if CONF_LORA_MESSAGE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LORA_MESSAGE])
        cg.add(var.set_message_sensor(sens))

    if CONF_LORA_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sens))


def validate_mode(value):
    if not (value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be output")
    return value


Lora_PIN_SCHEMA = pins.gpio_base_schema(
    LoraGPIOPin,
    cv.int_range(min=0, max=17),
    modes=[CONF_OUTPUT],
    mode_validator=validate_mode,
    invertable=True,
).extend(
    {
        cv.Required(CONF_LORA): cv.use_id(Lora),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_LORA, Lora_PIN_SCHEMA)
async def lora_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_LORA])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
