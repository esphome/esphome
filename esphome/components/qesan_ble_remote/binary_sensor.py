import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_CODE

from . import QesanListener, qesan_ble_remote_ns

DEPENDENCIES = ["qesan_ble_remote"]

CONF_REMOTE_ID = "remote_id"

QesanBinarySensor = qesan_ble_remote_ns.class_(
    "QesanBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(QesanBinarySensor).extend(
    {
        cv.GenerateID(CONF_REMOTE_ID): cv.use_id(QesanListener),
        cv.Required(CONF_CODE): cv.hex_uint8_t,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    receiver = await cg.get_variable(config[CONF_REMOTE_ID])
    cg.add(var.set_button_code(config[CONF_CODE]))
    cg.add(receiver.add_binary_sensor(var))
