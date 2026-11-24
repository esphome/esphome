import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]

haier_tundra_ns = cg.esphome_ns.namespace("haier_tundra")
HaierTundra = haier_tundra_ns.class_("HaierTundra", climate_ir.ClimateIR)

CONF_UNIT = "unit"
Unit = haier_tundra_ns.enum("Unit")
UNITS = Unit.options("a", "b", prefix="HAIER_HEADER_UNIT_")

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HaierTundra).extend(
    {
        cv.Optional(CONF_UNIT, default="a"): cv.enum(UNITS, lower=True),
    }
)


# Each transmitter may drive at most one unit A and one unit B
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="climate",
    platform="haier_tundra",
    keys=(remote_base.CONF_TRANSMITTER_ID, CONF_UNIT),
    message="Only one of each unit is allowed per transmitter",
)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("haier")
    await climate_ir.new_climate_ir(config, config[CONF_UNIT])
