import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_OPERATING_MODE
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import Alpha3, alpha3_ns
from ..const import CONF_ALPHA3_ID, CONF_CONTROL_MODE

DEPENDENCIES = ["alpha3"]

Alpha3Select = alpha3_ns.class_("Alpha3Select", select.Select)
Alpha3SelectType = alpha3_ns.enum("Alpha3SelectType", is_class=True)

OPERATION_OPTIONS = ["Normal", "Stop", "Min", "Max"]
CONTROL_OPTIONS = [
    "Constant pressure",
    "Proportional pressure",
    "Constant speed",
    "AUTOADAPT radiator",
    "AUTOADAPT underfloor",
    "AUTOADAPT radiator and underfloor",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_OPERATING_MODE): select.select_schema(
            Alpha3Select, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_CONTROL_MODE): select.select_schema(
            Alpha3Select, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ALPHA3_ID])
    if (conf := config.get(CONF_OPERATING_MODE)) is not None:
        entity = await select.new_select(
            conf,
            hub,
            Alpha3SelectType.ALPHA3_SELECT_TYPE_OPERATION_MODE,
            options=OPERATION_OPTIONS,
        )
        cg.add(hub.set_operation_mode_select(entity))
    if (conf := config.get(CONF_CONTROL_MODE)) is not None:
        entity = await select.new_select(
            conf,
            hub,
            Alpha3SelectType.ALPHA3_SELECT_TYPE_CONTROL_MODE,
            options=CONTROL_OPTIONS,
        )
        cg.add(hub.set_control_mode_select(entity))
