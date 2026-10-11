from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_FREQUENCY,
    CONF_ID,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_ENERGY,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_KILOVOLT_AMPS_HOURS,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@nuttytree"]

pzem6l24_ns = cg.esphome_ns.namespace("pzem6l24")
PZEM6L24 = pzem6l24_ns.class_(
    "PZEM6L24", cg.PollingComponent, modbus.ModbusClientDevice
)

ResetPhase = pzem6l24_ns.enum("ResetPhase")
RESET_PHASE_OPTIONS = {
    "all": ResetPhase.RESET_PHASE_ALL,
    "a": ResetPhase.RESET_PHASE_A,
    "b": ResetPhase.RESET_PHASE_B,
    "c": ResetPhase.RESET_PHASE_C,
    "combined": ResetPhase.RESET_PHASE_COMBINED,
}

# Combined config keys
CONF_TOTAL_ACTIVE_POWER = "total_active_power"
CONF_TOTAL_REACTIVE_POWER = "total_reactive_power"
CONF_TOTAL_APPARENT_POWER = "total_apparent_power"
CONF_TOTAL_POWER_FACTOR = "total_power_factor"
CONF_TOTAL_ACTIVE_ENERGY = "total_active_energy"
CONF_TOTAL_REACTIVE_ENERGY = "total_reactive_energy"
CONF_TOTAL_APPARENT_ENERGY = "total_apparent_energy"
CONF_PHASE = "phase"


_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)
_ACTIVE_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)
_REACTIVE_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_REACTIVE_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)
_APPARENT_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT_AMPS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_APPARENT_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)
_POWER_FACTOR_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_POWER_FACTOR,
    state_class=STATE_CLASS_MEASUREMENT,
)
_ACTIVE_ENERGY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)
_REACTIVE_ENERGY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_REACTIVE_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)
_APPARENT_ENERGY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
    accuracy_decimals=1,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)
_FREQUENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    icon=ICON_CURRENT_AC,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
)

# Quantities measured once per phase; the config key is "<name>_<phase>".
_PHASE_QUANTITIES = {
    "voltage": _VOLTAGE_SCHEMA,
    "current": _CURRENT_SCHEMA,
    "active_power": _ACTIVE_POWER_SCHEMA,
    "reactive_power": _REACTIVE_POWER_SCHEMA,
    "apparent_power": _APPARENT_POWER_SCHEMA,
    "power_factor": _POWER_FACTOR_SCHEMA,
    "active_energy": _ACTIVE_ENERGY_SCHEMA,
    "reactive_energy": _REACTIVE_ENERGY_SCHEMA,
    "apparent_energy": _APPARENT_ENERGY_SCHEMA,
}

# Quantities the meter reports once, keyed by config key.
_COMBINED_QUANTITIES = {
    CONF_FREQUENCY: _FREQUENCY_SCHEMA,
    CONF_TOTAL_ACTIVE_POWER: _ACTIVE_POWER_SCHEMA,
    CONF_TOTAL_REACTIVE_POWER: _REACTIVE_POWER_SCHEMA,
    CONF_TOTAL_APPARENT_POWER: _APPARENT_POWER_SCHEMA,
    CONF_TOTAL_POWER_FACTOR: _POWER_FACTOR_SCHEMA,
    CONF_TOTAL_ACTIVE_ENERGY: _ACTIVE_ENERGY_SCHEMA,
    CONF_TOTAL_REACTIVE_ENERGY: _REACTIVE_ENERGY_SCHEMA,
    CONF_TOTAL_APPARENT_ENERGY: _APPARENT_ENERGY_SCHEMA,
}

# Every sensor by config key; the C++ setter is "set_<key>_sensor".
_SENSORS = {
    f"{name}_{phase}": schema
    for name, schema in _PHASE_QUANTITIES.items()
    for phase in "abc"
} | _COMBINED_QUANTITIES

# The meter answers unit addresses 1 to 247; 0 is the Modbus broadcast address and 248 to 255 are reserved.
_ADDRESS_SCHEMA = cv.All(
    cv.hex_uint8_t,
    cv.Range(min=1, max=247, msg="The PZEM-6L24 answers unit addresses 1 to 247 only"),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEM6L24),
            **{cv.Optional(key): schema for key, schema in _SENSORS.items()},
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
    .extend({cv.Optional(CONF_ADDRESS, default=0x01): _ADDRESS_SCHEMA})
)


automation.register_apply_action(
    "pzem6l24.reset_energy",
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PZEM6L24),
            cv.Optional(CONF_PHASE, default="all"): cv.enum(
                RESET_PHASE_OPTIONS, lower=True
            ),
        }
    ),
    automation.ApplyField(CONF_PHASE, "reset_energy", ResetPhase),
)


FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device("pzem6l24", role="client")


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    for key in _SENSORS:
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))
