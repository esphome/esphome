import esphome.codegen as cg
from esphome.components import binary_sensor, modbus, sensor, text_sensor
from esphome.components.const import (
    BYTE_ORDER_BIG,
    BYTE_ORDER_LITTLE,
    CONF_BYTE_ORDER,
    CONF_SERIAL_NUMBER,
)
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
    STATE_CLASS_TOTAL,
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

CODEOWNERS = ["@sourabhjaiswal"]

CONF_DG_SENSING = "dg_sensing"


def AUTO_LOAD(config: ConfigType) -> list[str]:
    return ["modbus"] + [
        component
        for key, component in (
            (CONF_SERIAL_NUMBER, "text_sensor"),
            (CONF_DG_SENSING, "binary_sensor"),
        )
        if not config or key in config
    ]


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

selec_meter_ns = cg.esphome_ns.namespace("selec_meter")
SelecMeter = selec_meter_ns.class_(
    "SelecMeter", cg.PollingComponent, modbus.ModbusClientDevice
)
SelecMeterModel = selec_meter_ns.enum("Model", is_class=True)

MODEL_EM2M = "em2m"
MODEL_EM4M = "em4m"
MODELS = {
    MODEL_EM2M: SelecMeterModel.MODEL_EM2M,
    MODEL_EM4M: SelecMeterModel.MODEL_EM4M,
}

# Register word order of the FP32 values: big_endian sends the high word first (ABCD),
# little_endian the low word first (CDAB). msrf/lsrf are aliases matching the meter's own
# register 40070 (Endianness Selection) menu names.
BYTE_ORDER_MSRF = "msrf"
BYTE_ORDER_LSRF = "lsrf"
# Maps the YAML option directly to the C++ word_swap bool -- no enum needed on the C++ side.
BYTE_ORDER_WORD_SWAP = {
    BYTE_ORDER_BIG: False,
    BYTE_ORDER_LITTLE: True,
    BYTE_ORDER_MSRF: False,
    BYTE_ORDER_LSRF: True,
}
validate_byte_order = cv.enum(BYTE_ORDER_WORD_SWAP, lower=True)

# EM2M-only: total combined energy figures with no non-DG equivalent on the EM4M.
EM2M_SENSORS = {
    CONF_TOTAL_ACTIVE_ENERGY: sensor.sensor_schema(
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
    CONF_APPARENT_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
}

# Sensors available on both EM2M and EM4M. On EM4M these read the aggregate
# (Total/Average) registers rather than a single-phase measurement.
COMMON_SENSORS = {
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


def _sensor_group(*keys: str, **schema_kwargs) -> dict[str, cv.Schema]:
    """One identical sensor_schema() per key."""
    return {key: sensor.sensor_schema(**schema_kwargs) for key in keys}


# EM4M-only, genuinely per-phase quantities, nested under phase_a/phase_b/phase_c like
# sdm_meter/atm90e32 do for 3-phase meters.
PHASE_SENSORS = {
    key: {**COMMON_SENSORS, **EM2M_SENSORS}[key]
    for key in (
        CONF_VOLTAGE,
        CONF_CURRENT,
        CONF_ACTIVE_POWER,
        CONF_REACTIVE_POWER,
        CONF_APPARENT_POWER,
        CONF_POWER_FACTOR,
        CONF_IMPORT_ACTIVE_ENERGY,
        CONF_EXPORT_ACTIVE_ENERGY,
        CONF_IMPORT_REACTIVE_ENERGY,
        CONF_EXPORT_REACTIVE_ENERGY,
        CONF_APPARENT_ENERGY,
    )
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
        state_class=STATE_CLASS_TOTAL,
    ),
    **_sensor_group(
        CONF_NET_REACTIVE_ENERGY_MAINS,
        CONF_NET_REACTIVE_ENERGY_DG,
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL,
    ),
    **_sensor_group(
        CONF_NET_APPARENT_ENERGY_MAINS,
        CONF_NET_APPARENT_ENERGY_DG,
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL,
    ),
}

PHASES = (CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C)


def _optional(sensors: dict[str, cv.Schema]) -> dict[cv.Optional, cv.Schema]:
    return {cv.Optional(key): schema for key, schema in sensors.items()}


_BASE_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(SelecMeter)}).extend(
    cv.polling_component_schema("10s"), modbus.modbus_device_schema(0x01)
)

# EM2M keeps this component's original word-swapped decode; EM4M uses the meter's factory
# default (MSRF / Big Endian), confirmed against real hardware.
CONFIG_SCHEMA = cv.typed_schema(
    {
        MODEL_EM2M: _BASE_SCHEMA.extend(
            {
                cv.Optional(
                    CONF_BYTE_ORDER, default=BYTE_ORDER_LITTLE
                ): validate_byte_order,
                **_optional(COMMON_SENSORS),
                **_optional(EM2M_SENSORS),
            }
        ),
        MODEL_EM4M: _BASE_SCHEMA.extend(
            {
                cv.Optional(
                    CONF_BYTE_ORDER, default=BYTE_ORDER_BIG
                ): validate_byte_order,
                **_optional(COMMON_SENSORS),
                **_optional(EM4M_SENSORS),
                **{cv.Optional(phase): PHASE_SCHEMA for phase in PHASES},
                cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
                cv.Optional(CONF_DG_SENSING): binary_sensor.binary_sensor_schema(),
            }
        ),
    },
    key=CONF_MODEL,
    default_type=MODEL_EM2M,
    enum=MODELS,
    lower=True,
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("selec_meter", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_word_swap(config[CONF_BYTE_ORDER]))
    if config[CONF_MODEL] == MODEL_EM4M:
        cg.add_define("USE_SELEC_METER_EM4M")
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TOTAL_ACTIVE_ENERGY, var.set_total_active_energy_sensor)
    await sensors(CONF_IMPORT_ACTIVE_ENERGY, var.set_import_active_energy_sensor)
    await sensors(CONF_EXPORT_ACTIVE_ENERGY, var.set_export_active_energy_sensor)
    await sensors(CONF_TOTAL_REACTIVE_ENERGY, var.set_total_reactive_energy_sensor)
    await sensors(CONF_IMPORT_REACTIVE_ENERGY, var.set_import_reactive_energy_sensor)
    await sensors(CONF_EXPORT_REACTIVE_ENERGY, var.set_export_reactive_energy_sensor)
    await sensors(CONF_APPARENT_ENERGY, var.set_apparent_energy_sensor)
    await sensors(CONF_ACTIVE_POWER, var.set_active_power_sensor)
    await sensors(CONF_REACTIVE_POWER, var.set_reactive_power_sensor)
    await sensors(CONF_APPARENT_POWER, var.set_apparent_power_sensor)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER_FACTOR, var.set_power_factor_sensor)
    await sensors(CONF_FREQUENCY, var.set_frequency_sensor)
    await sensors(
        CONF_MAXIMUM_DEMAND_ACTIVE_POWER, var.set_maximum_demand_active_power_sensor
    )
    await sensors(
        CONF_MAXIMUM_DEMAND_REACTIVE_POWER, var.set_maximum_demand_reactive_power_sensor
    )
    await sensors(
        CONF_MAXIMUM_DEMAND_APPARENT_POWER, var.set_maximum_demand_apparent_power_sensor
    )
    await sensors(CONF_VOLTAGE_L12, var.set_voltage_l12_sensor)
    await sensors(CONF_VOLTAGE_L23, var.set_voltage_l23_sensor)
    await sensors(CONF_VOLTAGE_L31, var.set_voltage_l31_sensor)
    await sensors(CONF_AVERAGE_VOLTAGE_LL, var.set_average_voltage_ll_sensor)
    await sensors(CONF_NET_ACTIVE_ENERGY_MAINS, var.set_net_active_energy_mains_sensor)
    await sensors(
        CONF_NET_REACTIVE_ENERGY_MAINS, var.set_net_reactive_energy_mains_sensor
    )
    await sensors(
        CONF_NET_APPARENT_ENERGY_MAINS, var.set_net_apparent_energy_mains_sensor
    )
    await sensors(CONF_NET_ACTIVE_ENERGY_DG, var.set_net_active_energy_dg_sensor)
    await sensors(CONF_NET_REACTIVE_ENERGY_DG, var.set_net_reactive_energy_dg_sensor)
    await sensors(CONF_NET_APPARENT_ENERGY_DG, var.set_net_apparent_energy_dg_sensor)
    for phase_index, phase in enumerate(PHASES):
        if (phase_config := config.get(phase)) is None:
            continue
        for name in PHASE_SENSORS:
            if name in phase_config:
                sens = await sensor.new_sensor(phase_config[name])
                cg.add(getattr(var, f"set_{name}_sensor")(phase_index, sens))
    await text_sensor.sub_text_sensors(config)(
        CONF_SERIAL_NUMBER, var.set_serial_number_sensor
    )
    await binary_sensor.sub_binary_sensors(config)(
        CONF_DG_SENSING, var.set_dg_sensing_sensor
    )
