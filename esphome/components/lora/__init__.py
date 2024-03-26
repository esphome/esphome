import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, text_sensor, uart, pcf8574

from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    UNIT_DECIBEL_MILLIWATT,
    STATE_CLASS_MEASUREMENT,
    CONF_ID,
    ENTITY_CATEGORY_NONE,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart", "sensor", "text_sensor"]

lora_ns = cg.esphome_ns.namespace("lora")
LoraComponent = lora_ns.class_("Lora", cg.PollingComponent, uart.UARTDevice)
CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_PCF8574 = "pcf8574"
CONF_LORA_MESSAGE = "lora_message"
CONF_LORA_RSSI = "lora_rssi"
CONF_LORA = "lora"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LoraComponent),
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
            cv.Optional(CONF_PCF8574): cv.use_id(pcf8574.PCF8574Component),
            # if you want to see the rssi
            cv.Optional(CONF_LORA_RSSI): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    pin_aux = await cg.gpio_pin_expression(config[CONF_PIN_AUX])
    cg.add(var.set_pin_aux(pin_aux))

    pin_m0 = await cg.gpio_pin_expression(config[CONF_PIN_M0])
    cg.add(var.set_pin_m0(pin_m0))
    pin_m1 = await cg.gpio_pin_expression(config[CONF_PIN_M1])
    cg.add(var.set_pin_m1(pin_m1))

    if CONF_LORA_MESSAGE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LORA_MESSAGE])
        cg.add(var.set_message_sensor(sens))

    if CONF_LORA_MESSAGE in config:
        comp = await cg.get_variable(config[CONF_PCF8574])
        cg.add(var.set_pcf8574(comp))

    if CONF_LORA_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sens))
