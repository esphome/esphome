import esphome.codegen as cg
from esphome.components import climate
from esphome.components.climate import validate_climate_swing_mode
import esphome.config_validation as cv
from esphome.const import CONF_SUPPORTED_SWING_MODES
from esphome.types import ConfigType

from . import (
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    MitsubishiCN105Component,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
)

AUTO_LOAD = ["climate"]

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    cg.Parented.template(MitsubishiCN105Component),
)


CONFIG_SCHEMA = (
    climate.climate_schema(MitsubishiCN105Climate)
    .extend(MITSUBISHI_CN105_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES, default="OFF"
            ): validate_climate_swing_mode
        }
    )
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await register_mitsubishi_cn105_device(var, config)
    cg.add(var.set_supported_swing_mode(config[CONF_SUPPORTED_SWING_MODES]))
