import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_TX_PIN
from esphome.core import CORE, coroutine

DEPENDENCIES = ['uart']
MULTI_CONF = True

dmx512_ns = cg.esphome_ns.namespace('dmx512')
DMX512ESP32 = dmx512_ns.class_('DMX512ESP32', cg.Component)
DMX512ESP8266 = dmx512_ns.class_('DMX512ESP8266', cg.Component)

CONF_DMX512_ID = 'dmx512_id'
CONF_ENABLE_PIN = 'enable_pin'
CONF_UART_NUM = 'uart_num'
CONF_PERIODIC_UPDATE = 'periodic_update'
CONF_FORCE_FULL_FRAMES = 'force_full_frames'
CONF_CUSTOM_BREAK_LEN = 'custom_break_len'
CONF_CUSTOM_MAB_LEN = 'custom_mab_len'
CONF_UPDATE_INTERVAL = 'update_interval'

UART_MAX = 2

if CORE.is_esp32:
    UART_MAX = 2
elif CORE.is_esp8266:
    UART_MAX = 1

def _declare_type(value):
    if CORE.is_esp32:
        if CORE.using_arduino:
            return cv.declare_id(DMX512ESP32)(value)
    elif CORE.is_esp8266:
        return cv.declare_id(DMX512ESP8266)(value)
    raise NotImplementedError

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): _declare_type,
    cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_TX_PIN, default=5): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_UART_NUM, default=1): cv.int_range(min=0, max=UART_MAX),
    cv.Optional(CONF_PERIODIC_UPDATE, default=True): cv.boolean,
    cv.Optional(CONF_FORCE_FULL_FRAMES, default=False): cv.boolean,
    cv.Optional(CONF_CUSTOM_MAB_LEN, default=12): cv.int_range(min=12,max=1000),
    cv.Optional(CONF_CUSTOM_BREAK_LEN, default=92): cv.int_range(min=92, max=1000),
    cv.Optional(CONF_UPDATE_INTERVAL, default=500): cv.int_range(),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    cg.add_global(dmx512_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    if CONF_ENABLE_PIN in config:
        enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable))

    if CONF_TX_PIN in config:
        tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
        cg.add(var.set_uart_tx_pin(tx_pin))

    cg.add(var.set_uart_num(config[CONF_UART_NUM]))
    cg.add(var.set_periodic_update(config[CONF_PERIODIC_UPDATE]))
    cg.add(var.set_force_full_frames(config[CONF_FORCE_FULL_FRAMES]))
    cg.add(var.set_mab_len(config[CONF_CUSTOM_MAB_LEN]))
    cg.add(var.set_break_len(config[CONF_CUSTOM_BREAK_LEN]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
