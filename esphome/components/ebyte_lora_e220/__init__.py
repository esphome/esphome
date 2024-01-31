import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, uart
from esphome.const import *
ebyte_lora_e220_ns = cg.esphome_ns.namespace('ebyte_lora_e220')
EbyteLoraE220 = ebyte_lora_e220_ns.class_('EbyteLoraE220', cg.PollingComponent)

DEPENDENCIES = ['uart']
AUTO_LOAD = ['uart', 'sensor', 'text_sensor']

CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_LORA_STATUS = "lora_status"
CONF_LORA_MESSAGE = "lora_message"
CONF_LORA_RSSI = "lora_rssi"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EbyteLoraE220),
    cv.Required(CONF_PIN_AUX): pins.gpio_input_pin_schema,
    cv.Required(CONF_PIN_M0): pins.gpio_output_pin_schema,
    cv.Required(CONF_PIN_M1): pins.gpio_output_pin_schema,
    cv.Optional(CONF_LORA_MESSAGE): text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_NONE,),
    cv.Optional(CONF_LORA_STATUS): text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC,),
    cv.Optional(CONF_LORA_RSSI):
        sensor.sensor_schema(device_class=DEVICE_CLASS_SIGNAL_STRENGTH,unit_of_measurement=UNIT_DECIBEL_MILLIWATT,accuracy_decimals=0,state_class=STATE_CLASS_MEASUREMENT).extend(),

}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    
    p = await cg.gpio_pin_expression(config[CONF_PIN_AUX])
    cg.add(var.set_pin_aux(p))
  
    p = await cg.gpio_pin_expression(config[CONF_PIN_M0])
    cg.add(var.set_pin_m0(p))
    p = await cg.gpio_pin_expression(config[CONF_PIN_M1])
    cg.add(var.set_pin_m1(p))

    if CONF_LORA_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LORA_STATUS])
        cg.add(var.set_status_sensor(sens))

    if CONF_LORA_MESSAGE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LORA_MESSAGE])
        cg.add(var.set_message_sensor(sens))

    if CONF_LORA_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sens))


