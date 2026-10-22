from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
from esphome.config import iter_ids
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@kbx81"]

MODEL_RAS_25J2KVSG_ND = "RAS-25J2KVSG-ND"

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)

Model = toshiba_ns.enum("Model")
MODELS = Model.options(
    "GENERIC",
    "RAC-PT1411HWRU-C",
    "RAC-PT1411HWRU-F",
    "RAS-2819T",
    "RAS-B10N3KV2",
    MODEL_RAS_25J2KVSG_ND,
    "SEIYA",
    prefix="MODEL_",
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(ToshibaClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="generic"): cv.enum(MODELS, upper=True),
    }
)


def _validate_fix_model(config: ConfigType) -> ConfigType:
    if config[CONF_MODEL] != MODEL_RAS_25J2KVSG_ND:
        for id_, path in iter_ids(fv.full_config.get()):
            if id_ == config[CONF_ID] and path[-2:] == ["toshiba.fix", CONF_ID]:
                with cv.prepend_path([cv.ROOT_CONFIG_PATH, *path]):
                    raise cv.Invalid(
                        f"toshiba.fix requires model {MODEL_RAS_25J2KVSG_ND}"
                    )
    return config


FINAL_VALIDATE_SCHEMA = _validate_fix_model


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("toshiba_ac")  # used from C++
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if config[CONF_MODEL] == MODEL_RAS_25J2KVSG_ND:
        cg.add_define("USE_TOSHIBA_RAS_25J2KVSG_ND")


automation.register_apply_action(
    "toshiba.fix",
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(ToshibaClimate)}),
    automation.ApplyCall("fix()"),
)
