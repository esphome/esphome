import esphome.codegen as cg
from esphome.components import light, number, select, sensor, switch, text_sensor

# import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIONS,
    CONF_OUTPUT_ID,
    CONF_STEP,
)

from .. import (
    CONF_FENDT_CARAVAN_ID,
    CONFIG_FENDT_SCHEMA,
    FendtNumber,
    FendtSelect,
    FendtSensor,
    FendtSwitch,
    FendtTextSensor,
)

# from .alde_sensor import ALDES, CONF_ALDE_DEVICE, CONFIG_ALDE_SCHEMA
# from .control_unit_sensor import (
#    CONF_CONTROL_UNIT_DEVICE,
#    CONFIG_CONTROL_UNIT_SCHEMA,
#    CONTROL_UNITS,
# )
# from .fridge_sensor import CONF_FRIDGE_DEVICE, CONFIG_FRIDGE_SCHEMA, FRIDGES
# from .lighting_sensor import CONF_LIGHTING_DEVICE, CONFIG_LIGHTING_SCHEMA, LIGHTINGS

CONFIG_SCHEMA = CONFIG_FENDT_SCHEMA.extend(
    {
        #       cv.Required(CONF_CONTROL_UNIT_DEVICE): CONFIG_CONTROL_UNIT_SCHEMA,
        #       cv.Optional(CONF_FRIDGE_DEVICE): CONFIG_FRIDGE_SCHEMA,
        #       cv.Optional(CONF_ALDE_DEVICE): CONFIG_ALDE_SCHEMA,
        #       cv.Optional(CONF_LIGHTING_DEVICE): CONFIG_LIGHTING_SCHEMA,
    }
)


async def device_to_code(parent, types, config):
    for conf in types:
        if conf in config:
            conf_item = config[conf]
            if conf_item[CONF_ID].type is FendtTextSensor:
                var = await text_sensor.new_text_sensor(conf_item)
                cg.add(getattr(parent, f"set_{conf}_text_sensor")(var))
            if conf_item[CONF_ID].type is FendtSwitch:
                var = await switch.new_switch(conf_item)
                cg.add(getattr(parent, f"set_{conf}_switch")(var))
            if conf_item[CONF_ID].type is FendtSelect:
                var = await select.new_select(
                    conf_item, options=conf_item[CONF_OPTIONS]
                )
                cg.add(getattr(parent, f"set_{conf}_select")(var))
            if conf_item[CONF_ID].type is FendtNumber:
                var = await number.new_number(
                    conf_item,
                    min_value=conf_item[CONF_MIN_VALUE],
                    max_value=conf_item[CONF_MAX_VALUE],
                    step=conf_item[CONF_STEP],
                )
                cg.add(getattr(parent, f"set_{conf}_number")(var))
            if conf_item[CONF_ID].type is FendtSensor:
                var = await sensor.new_sensor(conf_item)
                cg.add(getattr(parent, f"set_{conf}_sensor")(var))
            if conf_item[CONF_ID].type is light.LightState:
                var = cg.new_Pvariable(conf_item[CONF_OUTPUT_ID])
                await light.register_light(var, conf_item)
                cg.add(getattr(parent, f"set_{conf}_light_output")(var))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FENDT_CARAVAN_ID])
    print(parent)
    # conf_unit = config[CONF_CONTROL_UNIT_DEVICE]
    # unit = cg.new_Pvariable(conf_unit[CONF_ID])
    # await cg.register_component(unit, conf_unit)
    # cg.add(parent.set_control_unit(unit))
    # await device_to_code(unit, CONTROL_UNITS, conf_unit)
    # if conf_alde := config.get(CONF_ALDE_DEVICE):
    #    var = cg.new_Pvariable(conf_alde[CONF_ID])
    #    await cg.register_component(var, conf_alde)
    #    cg.add(parent.set_alde_device(var))
    #    await device_to_code(var, ALDES, conf_alde)
    # if conf_fridge := config.get(CONF_FRIDGE_DEVICE):
    #    var = cg.new_Pvariable(conf_fridge[CONF_ID])
    #    await cg.register_component(var, conf_fridge)
    #    cg.add(parent.set_fridge_device(var))
    #    await device_to_code(var, FRIDGES, conf_fridge)
    # if conf_lighting := config.get(CONF_LIGHTING_DEVICE):
    #    var = cg.new_Pvariable(conf_lighting[CONF_ID])
    #    await cg.register_component(var, conf_lighting)
    #    cg.add(parent.set_lighting_device(var))
    #    await device_to_code(var, LIGHTINGS, conf_lighting)
