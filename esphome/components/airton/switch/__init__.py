import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..climate import CONF_AIRTON_ID, CONF_SLEEP_MODE, AirtonClimate, airton_ns

CODEOWNERS = ["@Procsiab"]

AirtonSetting = airton_ns.enum("AirtonSetting", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_ID): cv.use_id(AirtonClimate),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            switch.ApplySwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_SLEEP_MODE): switch.switch_schema(
            switch.ApplySwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AIRTON_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_DISPLAY,
        hub.set_display_switch,
        apply=f"set_setting({AirtonSetting.AIRTON_SETTING_DISPLAY}, state)",
    )
    await switches(
        CONF_SLEEP_MODE,
        hub.set_sleep_mode_switch,
        apply=f"set_setting({AirtonSetting.AIRTON_SETTING_SLEEP_MODE}, state)",
    )
