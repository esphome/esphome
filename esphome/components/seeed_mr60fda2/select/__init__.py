import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_SENSITIVITY, ENTITY_CATEGORY_CONFIG, ICON_ACCELERATION_Z
from esphome.types import ConfigType

from .. import CONF_MR60FDA2_ID, MR60FDA2Component, mr60fda2_ns

DEPENDENCIES = ["seeed_mr60fda2"]

InstallHeightSelect = mr60fda2_ns.class_("InstallHeightSelect", select.Select)
HeightThresholdSelect = mr60fda2_ns.class_("HeightThresholdSelect", select.Select)
SensitivitySelect = mr60fda2_ns.class_("SensitivitySelect", select.Select)

CONF_INSTALL_HEIGHT = "install_height"
CONF_HEIGHT_THRESHOLD = "height_threshold"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_INSTALL_HEIGHT): select.select_schema(
        InstallHeightSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_ACCELERATION_Z,
    ),
    cv.Optional(CONF_HEIGHT_THRESHOLD): select.select_schema(
        HeightThresholdSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_ACCELERATION_Z,
    ),
    cv.Optional(CONF_SENSITIVITY): select.select_schema(
        SensitivitySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR60FDA2_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_INSTALL_HEIGHT,
        hub.set_install_height_select,
        options=["2.4m", "2.5m", "2.6m", "2.7m", "2.8m", "2.9m", "3.0m"],
    )
    await selects(
        CONF_HEIGHT_THRESHOLD,
        hub.set_height_threshold_select,
        options=["0.0m", "0.1m", "0.2m", "0.3m", "0.4m", "0.5m", "0.6m"],
    )
    await selects(CONF_SENSITIVITY, hub.set_sensitivity_select, options=["1", "2", "3"])
