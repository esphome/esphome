import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_USE_FAHRENHEIT
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir", "coolix"]
CODEOWNERS = ["@dudanov"]

midea_ir_ns = cg.esphome_ns.namespace("midea_ir")
MideaIR = midea_ir_ns.class_("MideaIR", climate_ir.ClimateIR)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MideaIR).extend(
    {
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
    }
)


async def to_code(config: ConfigType) -> None:
    # midea_ir uses MideaProtocol from C++ and auto-loads coolix, whose coolix.cpp uses
    # CoolixProtocol even when no coolix climate is configured
    remote_base.request_protocol("midea")
    remote_base.request_protocol("coolix")
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))
