import esphome.codegen as cg
from esphome.components import binary_sensor, modbus, sensor, text_sensor
from esphome.components.const import CONF_BYTE_ORDER
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_APPARENT_POWER,
    CONF_CURRENT,
    CONF_EXPORT_ACTIVE_ENERGY,
    CONF_EXPORT_REACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_IMPORT_ACTIVE_ENERGY,
    CONF_IMPORT_REACTIVE_ENERGY,
    CONF_MODEL,
    CONF_PHASE_A,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@sourabhjaiswal"]

CONF_SERIAL_NUMBER = "serial_number"
CONF_DG_SENSING = "dg_sensing"


def AUTO_LOAD(config: ConfigType) -> list[str]:
    base = ["modbus"]
    if not config:
        return base + ["text_sensor", "binary_sensor"]
    extra = []
    if CONF_SERIAL_NUMBER in config:
        extra.append("text_sensor")
    if CONF_DG_SENSING in config:
        extra.append("binary_sensor")
    return base + extra


CONF_TOTAL_ACTIVE_ENERGY = "total_active_energy"
CONF_TOTAL_REACTIVE_ENERGY = "total_reactive_energy"
CONF_APPARENT_ENERGY = "apparent_energy"
CONF_MAXIMUM_DEMAND_ACTIVE_POWER = "maximum_demand_active_power"
CONF_MAXIMUM_DEMAND_REACTIVE_POWER = "maximum_demand_reactive_power"
CONF_MAXIMUM_DEMAND_APPARENT_POWER = "maximum_demand_apparent_power"

CONF_VOLTAGE_L12 = "voltage_l12"
CONF_VOLTAGE_L23 = "voltage_l23"
CONF_VOLTAGE_L31 = "voltage_l31"

CONF_AVERAGE_VOLTAGE_LL = "average_voltage_ll"
CONF_NET_ACTIVE_ENERGY_MAINS = "net_active_energy_mains"
CONF_NET_REACTIVE_ENERGY_MAINS = "net_reactive_energy_mains"
CONF_NET_APPARENT_ENERGY_MAINS = "net_apparent_energy_mains"
CONF_NET_ACTIVE_ENERGY_DG = "net_active_energy_dg"
CONF_NET_REACTIVE_ENERGY_DG = "net_reactive_energy_dg"
CONF_NET_APPARENT_ENERGY_DG = "net_apparent_energy_dg"

UNIT_KILOWATT_HOURS = "kWh"
UNIT_KILOVOLT_AMPS_HOURS = "kVAh"
UNIT_KILOVOLT_AMPS_REACTIVE_HOURS = "kVARh"

selec_meter_ns = cg.esphome_ns.namespace("selec_meter")
SelecMeter = selec_meter_ns.class_(
    "SelecMeter", cg.PollingComponent, modbus.ModbusClientDevice
)
SelecMeterModel = selec_meter_ns.enum("Model")

MODEL_EM2M = "em2m"
MODEL_EM4M = "em4m"
MODELS = {
    MODEL_EM2M: SelecMeterModel.EM2M,
    MODEL_EM4M: SelecMeterModel.EM4M,
}

# Naming matches the meter's own register 40070 (Endianness Selection):
# MSRF = Big Endian (straightforward ABCD word order), LSRF = Mid Little Endian (word-swapped CDAB order).
BYTE_ORDER_MSRF = "msrf"
BYTE_ORDER_LSRF = "lsrf"
# Maps the YAML option directly to the C++ word_swap bool -- no enum needed on the C++ side.
BYTE_ORDER_WORD_SWAP = {
    BYTE_ORDER_MSRF: False,
    BYTE_ORDER_LSRF: True,
}
# Default byte order assumed per model when `byte_order` isn't set explicitly.
# EM2M: preserves this component's original (word-swapped) decode behavior.
# EM4M: matches the meter's factory default (MSRF / Big Endian), confirmed against real hardware.
DEFAULT_WORD_SWAP = {
    MODEL_EM2M: True,
    MODEL_EM4M: False,
}

# Sensors available on both EM2M and EM4M. On EM4M these read the aggregate
# (Total/Average) registers rather than a single-phase measurement.
SENSORS = {
    CONF_TOTAL_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_IMPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_TOTAL_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_IMPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_APPARENT_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_REACTIVE_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_APPARENT_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_POWER_FACTOR: sensor.sensor_schema(
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_REACTIVE_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_APPARENT_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

# EM2M-only: total combined energy figures with no non-DG equivalent on the EM4M.
EM2M_ONLY_SENSORS = (
    CONF_TOTAL_ACTIVE_ENERGY,
    CONF_TOTAL_REACTIVE_ENERGY,
    CONF_APPARENT_ENERGY,
)


def _sensor_group(*keys: str, **schema_kwargs) -> dict[str, cv.Schema]:
    """Build a dict of otherwise-identical sensor.sensor_schema() entries, one per key.

    EM4M's line-to-line and mains/DG sensors repeat the same unit/accuracy/device_class/state_class
    across several config keys -- this collapses each such group to a single call instead of a
    separate sensor.sensor_schema() block per key.
    """
    return {key: sensor.sensor_schema(**schema_kwargs) for key in keys}


# EM4M-only, genuinely per-phase quantities. Exposed under phase_a/phase_b/phase_c (see
# PHASE_SCHEMA below), matching the nesting sdm_meter/atm90e32 use for 3-phase meters, rather
# than as flat _l1/_l2/_l3 keys.
PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_REACTIVE_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_APPARENT_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_POWER_FACTOR: sensor.sensor_schema(
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_IMPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_IMPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_APPARENT_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
}

PHASE_SCHEMA = cv.Schema(
    {cv.Optional(key): schema for key, schema in PHASE_SENSORS.items()}
)

# EM4M-only, line-to-line and mains/DG net totals -- these don't map to a single phase, so they
# stay flat rather than living under phase_a/phase_b/phase_c.
EM4M_SENSORS = {
    **_sensor_group(
        CONF_VOLTAGE_L12,
        CONF_VOLTAGE_L23,
        CONF_VOLTAGE_L31,
        CONF_AVERAGE_VOLTAGE_LL,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # Net (import - export) energy totals, split by source. Only meaningful when the meter's
    # "Dual Source" setting is Yes; DG registers read 0 on single-source installs.
    **_sensor_group(
        CONF_NET_ACTIVE_ENERGY_MAINS,
        CONF_NET_ACTIVE_ENERGY_DG,
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    **_sensor_group(
        CONF_NET_REACTIVE_ENERGY_MAINS,
        CONF_NET_REACTIVE_ENERGY_DG,
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    **_sensor_group(
        CONF_NET_APPARENT_ENERGY_MAINS,
        CONF_NET_APPARENT_ENERGY_DG,
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
}

ALL_SENSORS = {**SENSORS, **EM4M_SENSORS}

PHASES = (CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C)

# EM4M-only, but not plain sensor.Sensor entries (text_sensor / binary_sensor), so they
# live outside EM4M_SENSORS / ALL_SENSORS and are wired up separately in to_code().
EM4M_ONLY_EXTRA_KEYS = (CONF_SERIAL_NUMBER, CONF_DG_SENSING)


def _validate_model_sensors(config: ConfigType) -> ConfigType:
    model = config[CONF_MODEL]
    if model == MODEL_EM2M:
        for key in (*EM4M_SENSORS, *EM4M_ONLY_EXTRA_KEYS, *PHASES):
            if key in config:
                raise cv.Invalid(f"'{key}' requires 'model: {MODEL_EM4M}'", [key])
    elif model == MODEL_EM4M:
        for key in EM2M_ONLY_SENSORS:
            if key in config:
                raise cv.Invalid(
                    f"'{key}' has no non-DG equivalent register on the EM4M "
                    f"and is not yet supported with 'model: {MODEL_EM4M}'",
                    [key],
                )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(SelecMeter)})
    .extend(
        {
            cv.Optional(sensor_name): schema
            for sensor_name, schema in ALL_SENSORS.items()
        }
    )
    .extend(
        {
            cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
        }
    )
    .extend({cv.Optional(CONF_MODEL, default=MODEL_EM2M): cv.enum(MODELS, lower=True)})
    .extend({cv.Optional(CONF_BYTE_ORDER): cv.enum(BYTE_ORDER_WORD_SWAP, lower=True)})
    .extend(
        {
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_DG_SENSING): binary_sensor.binary_sensor_schema(),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01)),
    _validate_model_sensors,
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("selec_meter", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if (word_swap := config.get(CONF_BYTE_ORDER)) is None:
        word_swap = DEFAULT_WORD_SWAP[config[CONF_MODEL]]
    cg.add(var.set_word_swap(word_swap))
    for name in ALL_SENSORS:
        if name in config:
            sens = await sensor.new_sensor(config[name])
            cg.add(getattr(var, f"set_{name}_sensor")(sens))
    for phase_index, phase in enumerate(PHASES):
        if (phase_config := config.get(phase)) is None:
            continue
        for name in PHASE_SENSORS:
            if name in phase_config:
                sens = await sensor.new_sensor(phase_config[name])
                cg.add(getattr(var, f"set_{name}_sensor")(phase_index, sens))
    if (serial_number_config := config.get(CONF_SERIAL_NUMBER)) is not None:
        serial_number_sens = await text_sensor.new_text_sensor(serial_number_config)
        cg.add(var.set_serial_number_sensor(serial_number_sens))
    if (dg_sensing_config := config.get(CONF_DG_SENSING)) is not None:
        dg_sensing_sens = await binary_sensor.new_binary_sensor(dg_sensing_config)
        cg.add(var.set_dg_sensing_sensor(dg_sensing_sens))
