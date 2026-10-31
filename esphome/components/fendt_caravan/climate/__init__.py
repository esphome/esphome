from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, sensor, switch
import esphome.config_validation as cv
from esphome.const import CONF_HEAT_ACTION, CONF_SENSOR, CONF_TYPE

from .. import CONF_PARENT_ID, FendtCaravanHubBase, fendt_caravan_ns

FendtClimate = fendt_caravan_ns.class_(
    "FendtClimate",
    climate.Climate,
    cg.Parented.template(FendtCaravanHubBase),
)

CONF_OFF_ACTION = "off_action"
CONF_HEATER_SWITCH = "heater_switch"

CONFIG_SCHEMA = cv.typed_schema(
    {
        "alde": climate.climate_schema(FendtClimate).extend(
            {
                cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
                cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
                cv.Required(CONF_HEATER_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_HEAT_ACTION): automation.validate_automation(
                    single=True
                ),
                cv.Optional(CONF_OFF_ACTION): automation.validate_automation(
                    single=True
                ),
            }
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await climate.new_climate(config)
    # set temperature sensor
    temp_sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_temperature_sensor(temp_sens))
    # set heater switch
    heater_switch = await cg.get_variable(config[CONF_HEATER_SWITCH])
    cg.add(var.set_heater_switch(heater_switch))
    if CONF_HEAT_ACTION in config:
        await automation.build_automation(
            var.get_heat_action_trigger(), [], config[CONF_HEAT_ACTION]
        )
    if CONF_OFF_ACTION in config:
        await automation.build_automation(
            var.get_off_action_trigger(), [], config[CONF_OFF_ACTION]
        )
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_climate")(var))
