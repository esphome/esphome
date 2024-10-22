import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_KT0803_ID,
    KT0803Component,
    kt0803_ns,
    CONF_REF_CLK,
    CONF_XTAL,
    CONF_ALC,
    CONF_SILENCE,
    CONF_MUTE,
    CONF_MONO,
    CONF_ENABLE,
    CONF_AUTO_PA_DOWN,
    CONF_PA_DOWN,
    CONF_STANDBY_ENABLE,
    CONF_PA_BIAS,
    CONF_DETECTION,
    CONF_AU_ENHANCE,
    ICON_VOLUME_MUTE,
    ICON_EAR_HEARING,
    ICON_SINE_WAVE,
    ICON_SLEEP,
    for_each_conf,
)

MuteSwitch = kt0803_ns.class_("MuteSwitch", switch.Switch)
MonoSwitch = kt0803_ns.class_("MonoSwitch", switch.Switch)
AutoPaDownSwitch = kt0803_ns.class_("AutoPaDownSwitch", switch.Switch)
PaDownSwitch = kt0803_ns.class_("PaDownSwitch", switch.Switch)
StandbyEnableSwitch = kt0803_ns.class_("StandbyEnableSwitch", switch.Switch)
PaBiasSwitch = kt0803_ns.class_("PaBiasSwitch", switch.Switch)
AuEnhanceSwitch = kt0803_ns.class_("AuEnhanceSwitch", switch.Switch)
AuEnhanceSwitch = kt0803_ns.class_("AuEnhanceSwitch", switch.Switch)
RefClkEnableSwitch = kt0803_ns.class_("RefClkEnableSwitch", switch.Switch)
XtalEnableSwitch = kt0803_ns.class_("XtalEnableSwitch", switch.Switch)
AlcEnableSwitch = kt0803_ns.class_("AlcEnableSwitch", switch.Switch)
SilenceDetectionSwitch = kt0803_ns.class_("SilenceDetectionSwitch", switch.Switch)

REF_CLK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            RefClkEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
    }
)

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            XtalEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
    }
)

ALC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            AlcEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

SILENCE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DETECTION): switch.switch_schema(
            SilenceDetectionSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KT0803_ID): cv.use_id(KT0803Component),
        cv.Optional(CONF_MUTE): switch.switch_schema(
            MuteSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_VOLUME_MUTE,
        ),
        cv.Optional(CONF_MONO): switch.switch_schema(
            MonoSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_EAR_HEARING,
        ),
        cv.Optional(CONF_AUTO_PA_DOWN): switch.switch_schema(
            AutoPaDownSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_PA_DOWN): switch.switch_schema(
            PaDownSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_STANDBY_ENABLE): switch.switch_schema(
            StandbyEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_PA_BIAS): switch.switch_schema(
            PaBiasSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_AU_ENHANCE): switch.switch_schema(
            AuEnhanceSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_REF_CLK): REF_CLK_SCHEMA,
        cv.Optional(CONF_XTAL): XTAL_SCHEMA,
        cv.Optional(CONF_ALC): ALC_SCHEMA,
        cv.Optional(CONF_SILENCE): SILENCE_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [CONF_MUTE],
        [CONF_MONO],
        [CONF_AUTO_PA_DOWN],
        [CONF_PA_DOWN],
        [CONF_STANDBY_ENABLE],
        [CONF_PA_BIAS],
        [CONF_AU_ENHANCE],
    ],
    CONF_REF_CLK: [
        [CONF_ENABLE],
    ],
    CONF_XTAL: [
        [CONF_ENABLE],
    ],
    CONF_ALC: [
        [CONF_ENABLE],
    ],
    CONF_SILENCE: [
        [CONF_DETECTION],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_KT0803_ID])

    async def new_switch(c, args, setter):
        s = await switch.new_switch(c)
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_switch")(s))

    await for_each_conf(config, VARIABLES, new_switch)
