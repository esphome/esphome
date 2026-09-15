import esphome.codegen as cg
from esphome.components import spi
from esphome.components.const import CONF_ACCELEROMETER_RANGE, CONF_GYROSCOPE_RANGE
from esphome.components.motion import motion_schema, new_motion_component
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import ICM20689Component, icm20689_ns

DEPENDENCIES = ["spi"]

#  Enum proxies (must match the C++ enum values exactly)
ICM20689AccelRange = icm20689_ns.enum("ICM20689AccelRange")
ACCEL_RANGE_OPTIONS = {
    "2G": ICM20689AccelRange.ICM20689_ACCEL_RANGE_2G,
    "4G": ICM20689AccelRange.ICM20689_ACCEL_RANGE_4G,
    "8G": ICM20689AccelRange.ICM20689_ACCEL_RANGE_8G,
    "16G": ICM20689AccelRange.ICM20689_ACCEL_RANGE_16G,
}

ICM20689GyroRange = icm20689_ns.enum("ICM20689GyroRange")
GYRO_RANGE_OPTIONS = {
    "250DPS": ICM20689GyroRange.ICM20689_GYRO_RANGE_250,
    "500DPS": ICM20689GyroRange.ICM20689_GYRO_RANGE_500,
    "1000DPS": ICM20689GyroRange.ICM20689_GYRO_RANGE_1000,
    "2000DPS": ICM20689GyroRange.ICM20689_GYRO_RANGE_2000,
}

#  Top-level CONFIG_SCHEMA
CONFIG_SCHEMA = (
    motion_schema(ICM20689Component, has_accel=True, has_gyro=True)
    .extend(
        {
            cv.Optional(CONF_ACCELEROMETER_RANGE, default="2G"): cv.enum(
                ACCEL_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_RANGE, default="250DPS"): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
        }
    )
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True, default_mode="MODE0", default_data_rate="1MHz"
        )
    )
)


#  Code generation
async def to_code(config: ConfigType) -> None:
    var = await new_motion_component(config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_accel_range(config[CONF_ACCELEROMETER_RANGE]))
    cg.add(var.set_gyro_range(config[CONF_GYROSCOPE_RANGE]))
