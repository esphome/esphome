import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, i2c
from esphome.const import CONF_ID, CONF_NAME, CONF_ASSUMED_STATE

DEPENDENCIES = ["i2c"]

CODEOWNERS = ["@nielsnl"]

M5RELAY4_ns = cg.esphome_ns.namespace("m5stack_4relay")
M5Relay4Control = M5RELAY4_ns.class_("M5Relay4Control", cg.Component, i2c.I2CDevice)
M5Relay4Switch = M5RELAY4_ns.class_("M5Relay4Switch", switch.Switch, cg.Component)

CONF_RELAY0 = "relay1"
CONF_RELAY1 = "relay2"
CONF_RELAY2 = "relay3"
CONF_RELAY3 = "relay4"

CONF_LED0 = "led1"
CONF_LED1 = "led2"
CONF_LED2 = "led3"
CONF_LED3 = "led4"

CONF_SYNC_MODE = "sync_mode"

ENUM_COMP_SWITCHES = {
    CONF_LED0: 7,
    CONF_LED1: 6,
    CONF_LED2: 5,
    CONF_LED3: 4,
    CONF_RELAY0: 3,
    CONF_RELAY1: 2,
    CONF_RELAY2: 1,
    CONF_RELAY3: 0,
}


def check_relayswitch():
    return cv.maybe_simple_value(
        switch.switch_schema(M5Relay4Switch),
        key=CONF_NAME,
    ).extend({cv.Optional(CONF_ASSUMED_STATE): cv.boolean})


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5Relay4Control),
        cv.Optional(CONF_SYNC_MODE): cv.boolean(),
        cv.Optional(CONF_RELAY0): check_relayswitch(),
        cv.Optional(CONF_RELAY1): check_relayswitch(),
        cv.Optional(CONF_RELAY2): check_relayswitch(),
        cv.Optional(CONF_RELAY3): check_relayswitch(),
        cv.Optional(CONF_LED0): check_relayswitch(),
        cv.Optional(CONF_LED1): check_relayswitch(),
        cv.Optional(CONF_LED2): check_relayswitch(),
        cv.Optional(CONF_LED3): check_relayswitch(),
    }
).extend(cv.component().extend(i2c.i2c_device_schema(0x26)))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    if CONF_SYNC_MODE in config:
        cg.add(var.set_sync_mode(config[CONF_SYNC_MODE]))

    for key, value in ENUM_COMP_SWITCHES:
        if key in config:
            conf = config[key]
            sens = yield switch.new_switch(conf, value)
            if CONF_ASSUMED_STATE in conf:
                cg.add(sens.set_assumed_state(conf[CONF_ASSUMED_STATE]))
            cg.add(sens.set_parent(var))
