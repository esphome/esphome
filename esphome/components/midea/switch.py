import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner, MideaSwitch

CODEOWNERS = ["@dudanov"]

CONF_BREEZE_AWAY = "breeze_away"
CONF_BREEZELESS = "breezeless"
CONF_FAHRENHEIT = "fahrenheit"
CONF_IECO = "ieco"
CONF_IECO_NUMBER = "ieco_number"
CONF_JET_COOL = "jet_cool"
CONF_OUT_SILENT = "out_silent"
CONF_PURIFIER = "purifier"
CONF_SOUND = "sound"


def _switch_schema(icon, entity_category=cv.UNDEFINED):
    # The appliance reports the state; nothing is restored
    return switch.switch_schema(
        switch.ApplySwitch,
        icon=icon,
        entity_category=entity_category,
        default_restore_mode="DISABLED",
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # 0x40 set status
        cv.Optional(CONF_PURIFIER): _switch_schema("mdi:air-purifier"),
        cv.Optional(CONF_FAHRENHEIT): _switch_schema(
            "mdi:temperature-fahrenheit", ENTITY_CATEGORY_CONFIG
        ),
        # 0xB0 set properties
        cv.Optional(CONF_JET_COOL): _switch_schema("mdi:snowflake-alert"),
        cv.Optional(CONF_BREEZE_AWAY): _switch_schema("mdi:weather-windy"),
        cv.Optional(CONF_BREEZELESS): _switch_schema("mdi:weather-windy-variant"),
        cv.Optional(CONF_OUT_SILENT): _switch_schema("mdi:volume-off"),
        cv.Optional(CONF_IECO): _switch_schema("mdi:leaf").extend(
            {
                # Value the iECO request carries; the unit reports it in its 0xB5 capabilities
                cv.Optional(CONF_IECO_NUMBER, default=1): cv.uint8_t,
            }
        ),
        cv.Optional(CONF_SOUND): _switch_schema(
            "mdi:volume-high", ENTITY_CATEGORY_CONFIG
        ),
    }
)


def _apply(setting):
    return f"control_switch({setting}, state)"


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    if (ieco := config.get(CONF_IECO)) is not None:
        cg.add(hub.set_ieco_number(ieco[CONF_IECO_NUMBER]))
    switches = switch.sub_switches(config, parent=hub)
    s = MideaSwitch
    await switches(
        CONF_PURIFIER, hub.set_purifier_switch, apply=_apply(s.MIDEA_SWITCH_PURIFIER)
    )
    await switches(
        CONF_FAHRENHEIT,
        hub.set_fahrenheit_switch,
        apply=_apply(s.MIDEA_SWITCH_FAHRENHEIT),
    )
    await switches(
        CONF_JET_COOL, hub.set_jet_cool_switch, apply=_apply(s.MIDEA_SWITCH_JET_COOL)
    )
    await switches(
        CONF_BREEZE_AWAY,
        hub.set_breeze_away_switch,
        apply=_apply(s.MIDEA_SWITCH_BREEZE_AWAY),
    )
    await switches(
        CONF_BREEZELESS,
        hub.set_breezeless_switch,
        apply=_apply(s.MIDEA_SWITCH_BREEZELESS),
    )
    await switches(
        CONF_OUT_SILENT,
        hub.set_out_silent_switch,
        apply=_apply(s.MIDEA_SWITCH_OUT_SILENT),
    )
    await switches(CONF_IECO, hub.set_ieco_switch, apply=_apply(s.MIDEA_SWITCH_IECO))
    await switches(CONF_SOUND, hub.set_sound_switch, apply=_apply(s.MIDEA_SWITCH_SOUND))
