import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from . import DFROBOT_MMWAVE_RADAR_ID, DfrobotMmwaveRadarComponent

DEPENDENCIES = ["dfrobot_mmwave_radar"]

dfrobot_mmwave_radar_ns = cg.esphome_ns.namespace("dfrobot_mmwave_radar")
DfrobotMmwaveRadarSwitch = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarSwitch", switch.Switch, cg.Component
)

TYPE = "type"
TYPES = {
    "turn_on_sensor": "turn_on_sensor",
    "turn_on_led": "turn_on_led",
    "presence_via_uart": "presence_via_uart",
    "start_after_boot": "start_after_boot",
}

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DfrobotMmwaveRadarSwitch),
        cv.GenerateID(DFROBOT_MMWAVE_RADAR_ID): cv.use_id(DfrobotMmwaveRadarComponent),
        cv.Required(TYPE): cv.enum(TYPES),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    dfr_mmwave_component = await cg.get_variable(config[DFROBOT_MMWAVE_RADAR_ID])
    sw = await switch.new_switch(config)
    await cg.register_component(sw, config)
    cg.add(sw.set_component(dfr_mmwave_component))
    cg.add(sw.set_type(config[TYPE]))
    if config[TYPE] == "turn_on_sensor":
        cg.add(dfr_mmwave_component.set_active_switch(sw))
    if config[TYPE] == "turn_on_led":
        cg.add(dfr_mmwave_component.set_led_switch(sw))
    if config[TYPE] == "presence_via_uart":
        cg.add(dfr_mmwave_component.set_uart_switch(sw))
    if config[TYPE] == "start_after_boot":
        cg.add(dfr_mmwave_component.set_boot_start_switch(sw))
