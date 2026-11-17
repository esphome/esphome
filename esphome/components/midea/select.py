import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner, MideaSelect

CODEOWNERS = ["@dudanov"]

CONF_AUX_HEAT_MODE = "aux_heat_mode"
CONF_BREEZE_AWAY_DIRECTION = "breeze_away_direction"
CONF_BREEZE_MODE = "breeze_mode"
CONF_CASCADE = "cascade"
CONF_FRESH_AIR = "fresh_air"
CONF_HORIZONTAL_SWING_ANGLE = "horizontal_swing_angle"
CONF_LEVELS = "levels"
CONF_RATE_SELECT = "rate_select"
CONF_VERTICAL_SWING_ANGLE = "vertical_swing_angle"

# Options in the order of the value tables in air_conditioner.cpp
AUX_HEAT_MODE_OPTIONS = ["Off", "Aux Heat", "Aux Only"]
SWING_ANGLE_OPTIONS = [
    "Off",
    "Position 1",
    "Position 2",
    "Position 3",
    "Position 4",
    "Position 5",
]
CASCADE_OPTIONS = ["Off", "Up", "Down"]
FRESH_AIR_OPTIONS = ["Off", "Low", "Medium", "High", "Boost"]
RATE_SELECT_OPTIONS = {
    2: ["Off", "75%", "50%"],
    5: ["Off", "Level 5", "Level 4", "Level 3", "Level 2", "Level 1"],
}
BREEZE_MODE_OPTIONS = ["Off", "Breeze Away", "Breeze Mild", "Breezeless"]
BREEZE_AWAY_DIRECTION_OPTIONS = ["Off", "Up", "Down"]


def _select_schema(icon, entity_category=cv.UNDEFINED):
    return select.select_schema(
        select.ApplySelect, icon=icon, entity_category=entity_category
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # 0x40 set status
        cv.Optional(CONF_AUX_HEAT_MODE): _select_schema("mdi:heating-coil"),
        # 0xB0 set properties
        cv.Optional(CONF_VERTICAL_SWING_ANGLE): _select_schema(
            "mdi:arrow-up-down", ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_HORIZONTAL_SWING_ANGLE): _select_schema(
            "mdi:arrow-left-right", ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_CASCADE): _select_schema("mdi:weather-windy"),
        cv.Optional(CONF_FRESH_AIR): _select_schema("mdi:air-filter"),
        cv.Optional(CONF_RATE_SELECT): _select_schema("mdi:speedometer").extend(
            {
                # The unit reports 2 or 5 levels in its 0xB5 capabilities
                cv.Optional(CONF_LEVELS, default=5): cv.one_of(2, 5, int=True),
            }
        ),
        cv.Optional(CONF_BREEZE_MODE): _select_schema("mdi:weather-windy-variant"),
        cv.Optional(CONF_BREEZE_AWAY_DIRECTION): _select_schema("mdi:arrow-decision"),
    }
)


def _apply(setting):
    return f"control_select({setting}, index)"


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    selects = select.sub_selects(config, parent=hub)
    s = MideaSelect
    await selects(
        CONF_AUX_HEAT_MODE,
        hub.set_aux_heat_mode_select,
        options=AUX_HEAT_MODE_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_AUX_HEAT_MODE),
    )
    await selects(
        CONF_VERTICAL_SWING_ANGLE,
        hub.set_vertical_swing_angle_select,
        options=SWING_ANGLE_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_VERTICAL_SWING_ANGLE),
    )
    await selects(
        CONF_HORIZONTAL_SWING_ANGLE,
        hub.set_horizontal_swing_angle_select,
        options=SWING_ANGLE_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_HORIZONTAL_SWING_ANGLE),
    )
    await selects(
        CONF_CASCADE,
        hub.set_cascade_select,
        options=CASCADE_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_CASCADE),
    )
    await selects(
        CONF_FRESH_AIR,
        hub.set_fresh_air_select,
        options=FRESH_AIR_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_FRESH_AIR),
    )
    if (rate := config.get(CONF_RATE_SELECT)) is not None:
        cg.add(hub.set_rate_select_levels(rate[CONF_LEVELS]))
        await selects(
            CONF_RATE_SELECT,
            hub.set_rate_select_select,
            options=RATE_SELECT_OPTIONS[rate[CONF_LEVELS]],
            apply=_apply(s.MIDEA_SELECT_RATE_SELECT),
        )
    await selects(
        CONF_BREEZE_MODE,
        hub.set_breeze_mode_select,
        options=BREEZE_MODE_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_BREEZE_MODE),
    )
    await selects(
        CONF_BREEZE_AWAY_DIRECTION,
        hub.set_breeze_away_direction_select,
        options=BREEZE_AWAY_DIRECTION_OPTIONS,
        apply=_apply(s.MIDEA_SELECT_BREEZE_AWAY_DIRECTION),
    )
