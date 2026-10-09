import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PHASE_A,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

CONF_ENERGY_PRODUCTION_DAY = "energy_production_day"
CONF_TOTAL_ENERGY_PRODUCTION = "total_energy_production"
CONF_TOTAL_GENERATION_TIME = "total_generation_time"
CONF_TODAY_GENERATION_TIME = "today_generation_time"
CONF_PV1 = "pv1"
CONF_PV2 = "pv2"
UNIT_HOURS = "h"
UNIT_KOHM = "kΩ"
UNIT_MILLIAMPERE = "mA"

CONF_INVERTER_STATUS = "inverter_status"
CONF_PV_ACTIVE_POWER = "pv_active_power"
CONF_INVERTER_MODULE_TEMP = "inverter_module_temp"
CONF_PROTOCOL_VERSION = "protocol_version"

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@leeuwte"]

growatt_solar_ns = cg.esphome_ns.namespace("growatt_solar")
GrowattSolar = growatt_solar_ns.class_(
    "GrowattSolar", cg.PollingComponent, modbus.ModbusClientDevice
)

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}
PV_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PHASE_SCHEMA = cv.Schema(
    {cv.Optional(sensor): schema for sensor, schema in PHASE_SENSORS.items()}
)
PV_SCHEMA = cv.Schema(
    {cv.Optional(sensor): schema for sensor, schema in PV_SENSORS.items()}
)

GrowattProtocolVersion = growatt_solar_ns.enum("GrowattProtocolVersion")
PROTOCOL_VERSIONS = {
    "RTU": GrowattProtocolVersion.RTU,
    "RTU2": GrowattProtocolVersion.RTU2,
}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GrowattSolar),
            cv.Optional(CONF_PROTOCOL_VERSION, default="RTU"): cv.enum(
                PROTOCOL_VERSIONS, upper=True
            ),
            cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
            cv.Optional(CONF_PV1): PV_SCHEMA,
            cv.Optional(CONF_PV2): PV_SCHEMA,
            cv.Optional(CONF_INVERTER_STATUS): sensor.sensor_schema(),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACTIVE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PV_ACTIVE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY_PRODUCTION_DAY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TOTAL_ENERGY_PRODUCTION): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_INVERTER_MODULE_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("growatt_solar", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    cg.add(var.set_protocol_version(config[CONF_PROTOCOL_VERSION]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_INVERTER_STATUS, var.set_inverter_status_sensor)
    await sensors(CONF_FREQUENCY, var.set_grid_frequency_sensor)
    await sensors(CONF_ACTIVE_POWER, var.set_grid_active_power_sensor)
    await sensors(CONF_PV_ACTIVE_POWER, var.set_pv_active_power_sensor)
    await sensors(CONF_ENERGY_PRODUCTION_DAY, var.set_today_production_sensor)
    await sensors(CONF_TOTAL_ENERGY_PRODUCTION, var.set_total_energy_production_sensor)
    await sensors(CONF_INVERTER_MODULE_TEMP, var.set_inverter_module_temp_sensor)

    for i, phase in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase not in config:
            continue

        phase_config = config[phase]
        for sensor_type in PHASE_SENSORS:
            if sensor_type in phase_config:
                sens = await sensor.new_sensor(phase_config[sensor_type])
                cg.add(getattr(var, f"set_{sensor_type}_sensor")(i, sens))

    for i, pv in enumerate([CONF_PV1, CONF_PV2]):
        if pv not in config:
            continue

        pv_config = config[pv]
        for sensor_type in pv_config:
            if sensor_type in pv_config:
                sens = await sensor.new_sensor(pv_config[sensor_type])
                cg.add(getattr(var, f"set_{sensor_type}_sensor_pv")(i, sens))
