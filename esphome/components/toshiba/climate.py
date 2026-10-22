from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@kbx81"]

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)

Model = toshiba_ns.enum("Model")
MODELS = Model.options(
    "GENERIC",
    "RAC-PT1411HWRU-C",
    "RAC-PT1411HWRU-F",
    "RAS-2819T",
    "RAS-B10N3KV2",
    "RAS-25J2KVSG-ND",
    "SEIYA",
    prefix="MODEL_",
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(ToshibaClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="generic"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("toshiba_ac")  # used from C++
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if config[CONF_MODEL] == "RAS-25J2KVSG-ND":
        cg.add_define("USE_TOSHIBA_RAS_25J2KVSG_ND")


automation.register_apply_action(
    "toshiba.fix",
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(ToshibaClimate)}),
    automation.ApplyCall("fix()"),
)
