import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM
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
    # Each transmitter may drive at most one unit A and one unit B
    climates = fv.full_config.get()["climate"]
    seen = set()

    for climate in climates:
        if CONF_PLATFORM in climate and climate[CONF_PLATFORM] == "haier_tundra":
            unit_tx_pair = (
                climate[CONF_UNIT],
                climate[remote_base.CONF_TRANSMITTER_ID],
            )

            if unit_tx_pair in seen and climate[CONF_ID] == config[CONF_ID]:
                raise cv.Invalid(
                    "Only one of each unit is allowed per transmitter", path=[CONF_UNIT]
                )
            seen.add(unit_tx_pair)


FINAL_VALIDATE_SCHEMA = _final_validate_unit


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("haier")
    await climate_ir.new_climate_ir(config, config[CONF_UNIT])
