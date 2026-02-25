from esphome import core
import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_ID, CONF_MAC_ADDRESS
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments, statement

CODEOWNERS = ["@jpeletier"]
DEPENDENCIES = ["esp32_ble_tracker"]

BLE_DEVICE_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA

bthome_ns = cg.esphome_ns.namespace("bthome")
DeviceListener = bthome_ns.class_(
    "DeviceListener", esp32_ble_tracker.ESPBTDeviceListener
)
DeviceBase = bthome_ns.class_("DeviceBase")
Device = bthome_ns.class_("Device", DeviceBase)
BTHomeSensorBase = bthome_ns.class_("BTHomeSensorBase", sensor.Sensor, cg.Component)
BTHomeSensor = bthome_ns.class_("BTHomeSensor", BTHomeSensorBase)


class DeferredStatement(cg.Statement):
    def __init__(self, func):
        self.func = func

    def __str__(self):
        statements = []
        self.func(statements)
        code = []
        for exp in statements:
            text = str(statement(exp))
            text = text.rstrip()
            code.append(text)
        return "\n".join(code)


class DeferredExpression(cg.Expression):
    def __init__(self, func, *args, **kwargs):
        self.func = func
        self.args = args
        self.kwargs = kwargs

    def __str__(self):
        return str(self.func(*self.args, **self.kwargs))


_REMOTE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Device),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_BINDKEY): cv.bind_key,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeviceListener),
        cv.Optional("remote_devices"): [_REMOTE_DEVICE_SCHEMA],
    }
).extend(BLE_DEVICE_SCHEMA)


BTHOME_KEY = "bthome_key"


def get_sensor_count(device_id):
    return CORE.data.get(BTHOME_KEY, {}).get(str(device_id), 0)


async def add_sensor(sensor_var, device_id):
    device_var = await cg.get_variable(device_id)
    counters = CORE.data.setdefault(BTHOME_KEY, {})
    index = counters.get(str(device_id), 0)
    counters[str(device_id)] = index + 1
    cg.add(device_var.set_sensor(index, sensor_var))


async def to_code(config):
    if "remote_devices" not in config:
        return

    listener = cg.new_Pvariable(
        core.ID(str(config[CONF_ID]), False, DeviceListener),
        TemplateArguments(len(config["remote_devices"])),
    )

    for i, device_config in enumerate(config["remote_devices"]):
        device_id = device_config[CONF_ID]
        device_var = cg.Pvariable(
            core.ID(str(device_id), False, DeviceBase),
            DeferredExpression(
                lambda device_id: device_id.type.template(
                    TemplateArguments(get_sensor_count(device_id))
                ).new(),
                device_id,
            ),
        )

        cg.add(listener.set_device(i, device_var))
        cg.add(device_var.set_address(device_config[CONF_MAC_ADDRESS].as_hex))

    await esp32_ble_tracker.register_ble_device(listener, config)
