import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, i2c
from esphome.const import CONF_ID, CONF_NAME, CONF_ASSUMED_STATE

DEPENDENCIES = ["i2c"]

M5RELAY4_ns = cg.esphome_ns.namespace("m5stack4relay")
M5Relay4Control = M5RELAY4_ns.class_("M5Relay4Control", cg.Component, i2c.I2CDevice)
M5Relay4Switch = M5RELAY4_ns.class_("M5Relay4Switch", switch.Switch, cg.Component)

CONF_RELAY1 = "relay1"
CONF_RELAY2 = "relay2"
CONF_RELAY3 = "relay3"
CONF_RELAY4 = "relay4"

CONF_LED1 = "led1"
CONF_LED2 = "led2"
CONF_LED3 = "led3"
CONF_LED4 = "led4"

CONF_SYNC_MODE = "sync_mode"

RelayBit_ = M5RELAY4_ns.enum("RelayBit", is_class=True)
ENUM_COMP_SWITCHES = {
    CONF_LED1: RelayBit_.LED1,
    CONF_LED2: RelayBit_.LED2,
    CONF_LED3: RelayBit_.LED3,
    CONF_LED4: RelayBit_.LED4,
    CONF_RELAY1: RelayBit_.RELAY1,
    CONF_RELAY2: RelayBit_.RELAY2,
    CONF_RELAY3: RelayBit_.RELAY3,
    CONF_RELAY4: RelayBit_.RELAY4,
}


def check_relayswitch():
    return cv.maybe_simple_value(
        switch.switch_schema(M5Relay4Switch).extend(
            {cv.Optional(CONF_ASSUMED_STATE): cv.boolean}
        ),
        key=CONF_NAME,
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Relay4Control),
            cv.Optional(CONF_SYNC_MODE): cv.boolean,
            cv.Optional(CONF_RELAY1): check_relayswitch(),
            cv.Optional(CONF_RELAY2): check_relayswitch(),
            cv.Optional(CONF_RELAY3): check_relayswitch(),
            cv.Optional(CONF_RELAY4): check_relayswitch(),
            cv.Optional(CONF_LED1): check_relayswitch(),
            cv.Optional(CONF_LED2): check_relayswitch(),
            cv.Optional(CONF_LED3): check_relayswitch(),
            cv.Optional(CONF_LED4): check_relayswitch(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x26))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    if CONF_SYNC_MODE in config:
        cg.add(var.set_sync_mode(config[CONF_SYNC_MODE]))

    for key, value in ENUM_COMP_SWITCHES.items():
        if key in config:
            conf = config[key]
            sens = yield switch.new_switch(conf, value)
            if CONF_ASSUMED_STATE in conf:
                cg.add(sens.set_assumed_state(conf[CONF_ASSUMED_STATE]))
            cg.add(sens.set_parent(var))
