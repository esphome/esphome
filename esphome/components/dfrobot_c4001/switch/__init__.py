import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODE,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_VIBRATE,
)
import esphome.final_validate as fv

from .. import CONF_DFROBOT_C4001_ID, HUB_CHILD_SCHEMA, dfrobot_c4001_ns

DEPENDENCIES = ["dfrobot_c4001"]


CONF_OUT_LED_ENABLE = "out_led_enable"
CONF_RUN_LED_ENABLE = "run_led_enable"
CONF_MICRO_MOTION_ENABLE = "micro_motion_enable"
ICON_LED = "mdi:led-off"
ICON_MICROSCOPE = "mdi:microscope"

OutLedSwitch = dfrobot_c4001_ns.class_("OutLedSwitch", switch.Switch)
RunLedSwitch = dfrobot_c4001_ns.class_("RunLedSwitch", switch.Switch)
MicroMotionSwitch = dfrobot_c4001_ns.class_("MicroMotionSwitch", switch.Switch)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_OUT_LED_ENABLE): switch.switch_schema(
                OutLedSwitch,
                # default_restore_mode="RESTORE_DEFAULT_ON",
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_LED,
            ),
            cv.Optional(CONF_RUN_LED_ENABLE): switch.switch_schema(
                RunLedSwitch,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_LED,
            ),
            cv.Optional(CONF_MICRO_MOTION_ENABLE): switch.switch_schema(
                MicroMotionSwitch,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_VIBRATE,
            ),
        }
    )
    .extend(HUB_CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate(config):
    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_DFROBOT_C4001_ID])[:-1]
    hub_conf = full_config.get_config_for_path(hub_path)
    mode = hub_conf.get(CONF_MODE)
    if mode == "PRESENCE" and CONF_MICRO_MOTION_ENABLE in config:
        raise cv.Invalid(
            f"When 'mode' is set to {mode}, {CONF_MICRO_MOTION_ENABLE} switch is not allowed."
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    sens0609_hub = await cg.get_variable(config[CONF_DFROBOT_C4001_ID])

    if out_led_enable := config.get(CONF_OUT_LED_ENABLE):
        s = await switch.new_switch(out_led_enable)
        await cg.register_parented(s, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_out_led_enable_switch(s))
    if run_led_enable := config.get(CONF_RUN_LED_ENABLE):
        s = await switch.new_switch(run_led_enable)
        await cg.register_parented(s, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_run_led_enable_switch(s))
    if micro_motion_enable := config.get(CONF_MICRO_MOTION_ENABLE):
        s = await switch.new_switch(micro_motion_enable)
        await cg.register_parented(s, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_micro_motion_enable_switch(s))
