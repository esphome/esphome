import esphome.codegen as cg
from esphome.components import i2c, light, output, switch
from esphome.components.time import RealTimeClock
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c", "fram", "output", "time"]
CODEOWNERS = ["@p1ngb4ck"]
MULTI_CONF = False

fram_ns = cg.esphome_ns.namespace("fram")
FRAMComponent = fram_ns.class_("FRAM", cg.Component, i2c.I2CDevice)
dynamic_lamp_ns = cg.esphome_ns.namespace('dynamic_lamp')
DynamicLampComponent = dynamic_lamp_ns.class_('DynamicLampComponent', cg.Component)
CONF_DYNAMIC_LAMP_ID = "dynamic_lamp_id"

CONF_RTC = 'rtc'
CONF_FRAM = 'fram'
CONF_SAVE_MODE = 'save_mode'
CONF_AVAILABLE_OUTPUTS = 'available_outputs'
CONF_AVAILABLE_LAMPS = 'available_lamps'
CONF_AVAILABLE_RELAYS = 'available_relays'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(DynamicLampComponent),
    cv.Required(CONF_RTC): cv.use_id(RealTimeClock),
    cv.Required(CONF_FRAM): cv.use_id(FRAMComponent),
    cv.Required(CONF_AVAILABLE_OUTPUTS): [cv.use_id(output.FloatOutput)],
    cv.Required(CONF_AVAILABLE_LAMPS): [cv.use_id(light.LightState)],
    cv.Required(CONF_AVAILABLE_RELAYS): [cv.use_id(switch.Switch)],
    cv.Optional(CONF_SAVE_MODE, default=0): cv.int_range(0, 2),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        await cg.get_variable(config[CONF_RTC]),
        await cg.get_variable(config[CONF_FRAM]),
    )
    await cg.register_component(var, config)
    for outputPointer in config.get(CONF_AVAILABLE_OUTPUTS, []):
        idstr_ = str(outputPointer)
        output_ = await cg.get_variable(outputPointer)
        cg.add(var.add_available_output(output_, idstr_))
    for lampPointer in config.get(CONF_AVAILABLE_LAMPS, []):
        idstr_ = str(lampPointer)
        lamp_ = await cg.get_variable(lampPointer)
        cg.add(var.add_available_lamp(lamp_, idstr_))
    for relayPointer in config.get(CONF_AVAILABLE_RELAYS, []):
        idstr_ = str(relayPointer)
        relay_ = await cg.get_variable(relayPointer)
        cg.add(var.add_available_relay(relay_, idstr_))
    cg.add(var.set_save_mode(config[CONF_SAVE_MODE]))
