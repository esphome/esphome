from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL
from esphome.types import ConfigType

from . import FILTER_SOURCE_FILES  # noqa: F401  pylint: disable=unused-import

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@kbx81"]

MODEL_RAS_25J2KVSG_ND = "RAS-25J2KVSG-ND"

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)
ToshibaRas25j2kvsgNdClimate = toshiba_ns.class_(
    "ToshibaRas25j2kvsgNdClimate", ToshibaClimate
)

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

_SCHEMA = climate_ir.climate_ir_with_receiver_schema(ToshibaClimate)
_RAS_25J2KVSG_ND_SCHEMA = climate_ir.climate_ir_with_receiver_schema(
    ToshibaRas25j2kvsgNdClimate
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        model: _RAS_25J2KVSG_ND_SCHEMA if model == MODEL_RAS_25J2KVSG_ND else _SCHEMA
        for model in MODELS
    },
    key=CONF_MODEL,
    default_type="GENERIC",
    enum=MODELS,
    upper=True,
)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("toshiba_ac")  # used from C++
    var = await climate_ir.new_climate_ir(config)
    if config[CONF_MODEL] == MODEL_RAS_25J2KVSG_ND:
        # The subclass sets its own model.
        cg.add_define("USE_TOSHIBA_RAS_25J2KVSG_ND")
    else:
        cg.add(var.set_model(config[CONF_MODEL]))


automation.register_apply_action(
    "toshiba.step_vane",
    automation.maybe_simple_id(
        {cv.Required(CONF_ID): cv.use_id(ToshibaRas25j2kvsgNdClimate)}
    ),
    automation.ApplyCall("step_vane()"),
)
