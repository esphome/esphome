from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import CONF_CURRENT, CONF_ENERGY, CONF_ID, CONF_POWER, CONF_VOLTAGE
from esphome.types import ConfigType

AUTO_LOAD = ["modbus"]

pzemdc_ns = cg.esphome_ns.namespace("pzemdc")
PZEMDC = pzemdc_ns.class_("PZEMDC", cg.PollingComponent, modbus.ModbusClientDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEMDC),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=3
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(kind=sensor.ENERGY_KWH),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)


automation.register_apply_action(
    "pzemdc.reset_energy",
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(PZEMDC),
        }
    ),
    automation.ApplyCall("reset_energy()"),
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("pzemdc", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await modbus.new_modbus_client_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
