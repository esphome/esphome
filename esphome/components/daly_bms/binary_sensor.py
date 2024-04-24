import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import DalyBmsComponent, CONF_BMS_DALY_ID

CONF_CHARGING_MOS_ENABLED = "charging_mos_enabled"
CONF_DISCHARGING_MOS_ENABLED = "discharging_mos_enabled"

TYPES = [
    CONF_CHARGING_MOS_ENABLED,
    CONF_DISCHARGING_MOS_ENABLED,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_DALY_ID): cv.use_id(DalyBmsComponent),
            cv.Optional(
                CONF_CHARGING_MOS_ENABLED
            ): binary_sensor.binary_sensor_schema(),
            cv.Optional(
                CONF_DISCHARGING_MOS_ENABLED
            ): binary_sensor.binary_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await binary_sensor.new_binary_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BMS_DALY_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
