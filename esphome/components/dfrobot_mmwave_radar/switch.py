import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from . import DFROBOT_MMWAVE_RADAR_ID, DfrobotMmwaveRadarComponent

DEPENDENCIES = ["dfrobot_mmwave_radar"]

dfrobot_mmwave_radar_ns = cg.esphome_ns.namespace("dfrobot_mmwave_radar")
DfrobotMmwaveRadarSwitch = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DfrobotMmwaveRadarSwitch),
        cv.GenerateID(DFROBOT_MMWAVE_RADAR_ID): cv.use_id(DfrobotMmwaveRadarComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    dfr_mmwave_component = await cg.get_variable(config[DFROBOT_MMWAVE_RADAR_ID])
    sw = await switch.new_switch(config)
    await cg.register_component(sw, config)
    cg.add(dfr_mmwave_component.set_active_switch(sw))
    cg.add(sw.set_component(dfr_mmwave_component))
