import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from .. import usb_device_ns, CONF_USB_DEVICE

AUTO_LOAD = [CONF_USB_DEVICE]

CONF_DETACH = "detach"
CONF_WAKE_UP = "wake_up"

TYPES = [
    CONF_DETACH,
    CONF_WAKE_UP,
]


DetachSwitch = usb_device_ns.class_("DetachSwitch", switch.Switch, cg.Component)
WakeUpSwitch = usb_device_ns.class_("WakeUpSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DETACH): switch.switch_schema(DetachSwitch),
        cv.Optional(CONF_WAKE_UP): switch.switch_schema(WakeUpSwitch),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    for key in TYPES:
        if key in config:
            conf = config[key]
            var = await switch.new_switch(conf)
            await cg.register_component(var, config)
