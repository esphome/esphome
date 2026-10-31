from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_VOLTAGE,
    ICON_CURRENT_AC,
)
from esphome.types import ConfigType

AUTO_LOAD = ["modbus"]

pzemac_ns = cg.esphome_ns.namespace("pzemac")
PZEMAC = pzemac_ns.class_("PZEMAC", cg.PollingComponent, modbus.ModbusClientDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEMAC),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=3
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=2
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                kind=sensor.ENERGY_WH, accuracy_decimals=0
            ),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                kind=sensor.FREQUENCY, icon=ICON_CURRENT_AC, accuracy_decimals=1
            ),
            cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
                kind=sensor.POWER_FACTOR, accuracy_decimals=2
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)


automation.register_apply_action(
    "pzemac.reset_energy",
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PZEMAC),
        }
    ),
    automation.ApplyCall("reset_energy()"),
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("pzemac", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await modbus.new_modbus_client_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
    await sensors(CONF_FREQUENCY, var.set_frequency_sensor)
    await sensors(CONF_POWER_FACTOR, var.set_power_factor_sensor)
