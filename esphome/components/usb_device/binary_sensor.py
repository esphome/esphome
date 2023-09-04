import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import CONF_USB_DEVICE_ID, UsbDevice, CONF_USB_DEVICE

AUTO_LOAD = [CONF_USB_DEVICE]

CONF_MOUNTED = "mounted"
CONF_READY = "ready"
CONF_SUSPENDED = "suspended"

TYPES = [
    CONF_MOUNTED,
    CONF_READY,
    CONF_SUSPENDED,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_USB_DEVICE_ID): cv.use_id(UsbDevice),
        cv.Optional(CONF_MOUNTED): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_READY): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_SUSPENDED): binary_sensor.binary_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_USB_DEVICE_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
