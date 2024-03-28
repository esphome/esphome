import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, uart

from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    CONF_ID,
    UNIT_PERCENT,
)

CODEOWNERS = ["@danielkoek"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart", "sensor"]
MULTI_CONF = True

ebyte_lora_ns = cg.esphome_ns.namespace("ebyte_lora")
EbyteLoraComponent = ebyte_lora_ns.class_(
    "EbyteLoraComponent", cg.PollingComponent, uart.UARTDevice
)
CONF_EBYTE_LORA = "ebyte_lora"
CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_LORA_RSSI = "lora_rssi"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EbyteLoraComponent),
            # for communication to let us know that we can receive data
            cv.Required(CONF_PIN_AUX): pins.gpio_input_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M0): pins.gpio_output_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M1): pins.gpio_output_pin_schema,
            # if you want to see the rssi
            cv.Optional(CONF_LORA_RSSI): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
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
    if CONF_LORA_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sens))
