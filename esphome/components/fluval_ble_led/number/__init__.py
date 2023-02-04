import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, fluval_ble_led
from .. import fluval_ble_led_ns

FluvalBleChannelNumber = fluval_ble_led_ns.class_(
    "FluvalBleChannelNumber", cg.Component, number.Number, fluval_ble_led.FluvalBleLed
)

from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_ID
)

CONF_CHANNEL = "channel"
CONF_ZERO_IF_OFF = "zero_if_off"

def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    if config[CONF_MIN_VALUE] < 0:
        raise cv.Invalid("max_value must be greater or equal to 0")
    if config[CONF_MAX_VALUE] > 1000:
        raise cv.Invalid("max_value must not be greater than 1000")
    return config

CONFIG_SCHEMA = cv.All(
    number.NUMBER_SCHEMA
    .extend(
        {
            cv.GenerateID(): cv.declare_id(FluvalBleChannelNumber),
            cv.Required(CONF_CHANNEL): int,
            cv.Optional(CONF_ZERO_IF_OFF, False): bool,
            cv.Optional(CONF_MAX_VALUE, default=1000.0): cv.positive_float,
            cv.Optional(CONF_MIN_VALUE, default=0.0): cv.positive_float,
            cv.Optional(CONF_STEP, default=100): cv.positive_float,            
        },        
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(fluval_ble_led.FLUVAL_CLIENT_SCHEMA),
    validate_min_max,
)

async def to_code(config):    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],                
    )
    cg.add(var.set_channel(config[CONF_CHANNEL]))    
    cg.add(var.set_zero_if_off(config[CONF_ZERO_IF_OFF]))
    await fluval_ble_led.register_fluval_led_client(var, config)        


    