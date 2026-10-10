import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_EC,
    CONF_ID,
    CONF_PH,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PH,
)
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]

AUTO_LOAD = ["modbus"]

kuntze_ns = cg.esphome_ns.namespace("kuntze")
Kuntze = kuntze_ns.class_("Kuntze", cg.PollingComponent, modbus.ModbusClientDevice)

CONF_DIS1 = "dis1"
CONF_DIS2 = "dis2"
CONF_REDOX = "redox"
CONF_OCI = "oci"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Kuntze),
            cv.Optional(CONF_PH): sensor.sensor_schema(
                unit_of_measurement=UNIT_PH,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_TEMPERATURE,
            ),
            cv.Optional(CONF_DIS1): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_DIS2): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_REDOX): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_EC): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_OCI): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_EMPTY,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("kuntze", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PH, var.set_ph_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_DIS1, var.set_dis1_sensor)
    await sensors(CONF_DIS2, var.set_dis2_sensor)
    await sensors(CONF_REDOX, var.set_redox_sensor)
    await sensors(CONF_EC, var.set_ec_sensor)
    await sensors(CONF_OCI, var.set_oci_sensor)
