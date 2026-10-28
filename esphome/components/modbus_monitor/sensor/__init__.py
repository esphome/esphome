import esphome.codegen as cg
from esphome.components import modbus, sensor
from esphome.components.const import CONF_VALUE_TYPE
from esphome.components.modbus.helpers import SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID

from .. import MonitorItem, modbus_monitor_ns, register_decoder

DEPENDENCIES = ["modbus"]

CONF_REGISTER = "register"

ModbusMonitorSensor = modbus_monitor_ns.class_(
    "ModbusMonitorSensor", sensor.Sensor, MonitorItem
)

# The value types modbus_controller sensors take, decoded by the same helper. RAW is left out: it publishes
# nothing a sensor can show.
VALUE_TYPES = {k: v for k, v in SENSOR_VALUE_TYPE.items() if k != "RAW"}

CONFIG_SCHEMA = sensor.sensor_schema(ModbusMonitorSensor).extend(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
        cv.Required(CONF_ADDRESS): cv.hex_uint8_t,
        cv.Required(CONF_REGISTER): cv.uint16_t,
        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(
            VALUE_TYPES, upper=True
        ),
    }
)


async def to_code(config):
    decoder = await register_decoder(config[modbus.CONF_MODBUS_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ADDRESS],
        config[CONF_REGISTER],
        config[CONF_VALUE_TYPE],
    )
    await sensor.register_sensor(var, config)
    cg.add(decoder.add_item(var))
