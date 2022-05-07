import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir, sensor, switch
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
    CONF_SWITCHES,
)

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@andrewmv"]

insignia_ns = cg.esphome_ns.namespace("insignia")
InsigniaClimate = insignia_ns.class_(
    "InsigniaClimate", climate_ir.ClimateIR, cg.PollingComponent
)

CONF_FOLLOW_ME_SENSOR = "sensor"
CONF_FOLLOW_ME_SWITCH = "follow_me_switch"
CONF_LED_SWITCH = "led_switch"

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(InsigniaClimate),
        cv.Optional(CONF_FOLLOW_ME_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_FOLLOW_ME_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_LED_SWITCH): cv.use_id(switch.Switch),
    }
).extend(cv.polling_component_schema("60s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    if CONF_FOLLOW_ME_SENSOR in config:
        sens = await cg.get_variable(config[CONF_FOLLOW_ME_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_FOLLOW_ME_SWITCH in config:
        if CONF_FOLLOW_ME_SENSOR not in config:
            raise cv.Invalid(f"sensor must be specified to use follow_me_switch")
        else:
            cg.add(var.set_fm_configured(True))
            sw = await cg.get_variable(config[CONF_FOLLOW_ME_SWITCH])
            cg.add(var.set_fm_switch(sw))

    if CONF_LED_SWITCH in config:
        cg.add(var.set_led_configured(True))
        sw = await cg.get_variable(config[CONF_LED_SWITCH])
        cg.add(var.set_led_switch(sw));
