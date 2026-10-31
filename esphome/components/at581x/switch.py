import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ICON_WIFI
from esphome.types import ConfigType

from .. import CONF_AT581X_ID, AT581XComponent

DEPENDENCIES = ["at581x"]


CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    device_class=DEVICE_CLASS_SWITCH,
    icon=ICON_WIFI,
).extend(
    cv.Schema(
        {
            cv.GenerateID(CONF_AT581X_ID): cv.use_id(AT581XComponent),
        }
    )
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AT581X_ID])
    await switch.sub_switches(config, parent=hub).create(
        config, hub.set_rf_power_switch, apply="set_rf_mode(state)", publish="before"
    )
