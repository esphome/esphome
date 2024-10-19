import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_OUTPUT,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_TUNER,
    CONF_PILOT,
    CONF_ACOMP,
    CONF_LIMITER,
    CONF_ASQ,
    CONF_RDS,
    CONF_MUTE,
    CONF_MONO,
    CONF_ENABLE,
    CONF_OVERMOD,
    CONF_IALL,
    CONF_IALH,
    CONF_GPIO1,
    CONF_GPIO2,
    CONF_GPIO3,
    ICON_RADIO_TOWER,
    ICON_SINE_WAVE,
    ICON_VOLUME_MUTE,
    ICON_EAR_HEARING,
    ICON_FORMAT_TEXT,
    for_each_conf,
)

MuteSwitch = si4713_ns.class_("MuteSwitch", switch.Switch)
MonoSwitch = si4713_ns.class_("MonoSwitch", switch.Switch)
TunerEnableSwitch = si4713_ns.class_("TunerEnableSwitch", switch.Switch)
PilotEnableSwitch = si4713_ns.class_("PilotEnableSwitch", switch.Switch)
AcompEnableSwitch = si4713_ns.class_("AcompEnableSwitch", switch.Switch)
LimiterEnableSwitch = si4713_ns.class_("LimiterEnableSwitch", switch.Switch)
AsqIallSwitch = si4713_ns.class_("AsqIallSwitch", switch.Switch)
AsqIalhSwitch = si4713_ns.class_("AsqIalhSwitch", switch.Switch)
AsqOvermodSwitch = si4713_ns.class_("AsqOvermodSwitch", switch.Switch)
RdsEnable = si4713_ns.class_("RdsEnable", switch.Switch)
RDSEnableSwitch = si4713_ns.class_("RDSEnableSwitch", switch.Switch)
OutputGpioSwitch = si4713_ns.class_("OutputGpioSwitch", switch.Switch)

TUNER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            TunerEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RADIO_TOWER,
        ),
    }
)

PILOT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            PilotEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)


ACOMP_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            AcompEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

LIMITER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            LimiterEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

ASQ_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IALL): switch.switch_schema(
            AsqIallSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_IALH): switch.switch_schema(
            AsqIalhSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_OVERMOD): switch.switch_schema(
            AsqOvermodSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
    }
)

RDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            RDSEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FORMAT_TEXT,
        ),
    }
)

OUTPUT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GPIO1): switch.switch_schema(
            OutputGpioSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
        cv.Optional(CONF_GPIO2): switch.switch_schema(
            OutputGpioSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
        cv.Optional(CONF_GPIO3): switch.switch_schema(
            OutputGpioSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
    }
)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
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
        cv.Optional(CONF_TUNER): TUNER_SCHEMA,
        cv.Optional(CONF_PILOT): PILOT_SCHEMA,
        cv.Optional(CONF_ACOMP): ACOMP_SCHEMA,
        cv.Optional(CONF_LIMITER): LIMITER_SCHEMA,
        cv.Optional(CONF_ASQ): ASQ_SCHEMA,
        cv.Optional(CONF_RDS): RDS_SCHEMA,
        cv.Optional(CONF_OUTPUT): OUTPUT_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [CONF_MUTE, None],
        [CONF_MONO, None],
    ],
    CONF_TUNER: [
        [CONF_ENABLE, None],
    ],
    CONF_PILOT: [
        [CONF_ENABLE, None],
    ],
    CONF_ACOMP: [
        [CONF_ENABLE, None],
    ],
    CONF_LIMITER: [
        [CONF_ENABLE, None],
    ],
    CONF_ASQ: [
        [CONF_IALL, None],
        [CONF_IALH, None],
        [CONF_OVERMOD, None],
    ],
    CONF_RDS: [
        [CONF_ENABLE, None],
    ],
    CONF_OUTPUT: [
        [CONF_GPIO1, lambda sw: sw.set_pin(1)],
        [CONF_GPIO2, lambda sw: sw.set_pin(2)],
        [CONF_GPIO3, lambda sw: sw.set_pin(3)],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SI4713_ID])

    async def new_switch(c, args, setter):
        s = await switch.new_switch(c)
        await cg.register_parented(s, parent)
        sw = cg.add(getattr(parent, setter + "_switch")(s))
        if cb := s[1]:
            cb(sw)

    await for_each_conf(config, VARIABLES, new_switch)
