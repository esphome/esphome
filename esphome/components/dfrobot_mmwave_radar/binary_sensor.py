import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_MOTION
from . import DFROBOT_MMWAVE_RADAR_ID, DfrobotMmwaveRadarComponent

DEPENDENCIES = ["dfrobot_mmwave_radar"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_MOTION
).extend(
    {
        cv.GenerateID(DFROBOT_MMWAVE_RADAR_ID): cv.use_id(DfrobotMmwaveRadarComponent),
    }
)


async def to_code(config):
    dfr_mmwave_component = await cg.get_variable(config[DFROBOT_MMWAVE_RADAR_ID])
    binary_sens = await binary_sensor.new_binary_sensor(config)

    cg.add(dfr_mmwave_component.set_detected_binary_sensor(binary_sens))
