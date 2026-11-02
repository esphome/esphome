import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_LIGHTBULB,
    ICON_MOTION_SENSOR,
)
from esphome.types import ConfigType

from . import (
    CONF_DFROBOT_SEN0609_ID,
    MODE_SPEED_AND_DISTANCE,
    DFRobotSEN0609Component,
    dfrobot_sen0609_ns,
    final_validate_mode,
)

SwitchSetting = dfrobot_sen0609_ns.enum("SwitchSetting")

CONF_MICRO_MOTION = "micro_motion"
CONF_OUT_LED = "out_led"
CONF_RUN_LED = "run_led"


def _led() -> cv.Schema:
    # The module forgets the LED state at power off, so it is restored from here
    return switch.switch_schema(
        switch.ApplySwitch,
        default_restore_mode="RESTORE_DEFAULT_ON",
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_LIGHTBULB,
    )


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_MICRO_MOTION): switch.switch_schema(
        switch.ApplySwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Optional(CONF_RUN_LED): _led(),
    cv.Optional(CONF_OUT_LED): _led(),
}

FINAL_VALIDATE_SCHEMA = final_validate_mode(
    {CONF_MICRO_MOTION: MODE_SPEED_AND_DISTANCE}
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    switches = switch.sub_switches(config, parent=hub)
    for key, setting in (
        (CONF_MICRO_MOTION, SwitchSetting.SWITCH_MICRO_MOTION),
        (CONF_RUN_LED, SwitchSetting.SWITCH_RUN_LED),
        (CONF_OUT_LED, SwitchSetting.SWITCH_OUT_LED),
    ):
        await switches(
            key,
            lambda s, setting=setting: hub.set_switch(setting, s),
            apply=f"control_switch({setting}, state)",
        )
