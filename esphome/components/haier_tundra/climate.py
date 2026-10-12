import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_PLATFORM
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]

haier_tundra_ns = cg.esphome_ns.namespace("haier_tundra")
HaierTundra = haier_tundra_ns.class_("HaierTundra", climate_ir.ClimateIR)

CONF_UNIT = "unit"
Unit = haier_tundra_ns.enum("Unit")
UNITS = {
    "a": Unit.HAIER_HEADER_UNIT_A,
    "b": Unit.HAIER_HEADER_UNIT_B,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HaierTundra).extend(
    {
        cv.Optional(CONF_UNIT, default="a"): cv.enum(UNITS, lower=True),
    }
)


def _final_validate_unit(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if "climate" in full_config:
        # Ensure at most one each of unit A and B
        has_unit_a = has_unit_b = False

        for climate in full_config["climate"]:
            if CONF_PLATFORM in climate and climate[CONF_PLATFORM] == "haier_tundra":
                if has_unit_a and climate[CONF_UNIT] == "a":
                    raise cv.Invalid("Only one unit A is allowed")
                has_unit_a = True

                if has_unit_b and climate[CONF_UNIT] == "b":
                    raise cv.Invalid("Only one unit B is allowed")
                has_unit_b = True


FINAL_VALIDATE_SCHEMA = _final_validate_unit


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("haier")
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_unit(config[CONF_UNIT]))
