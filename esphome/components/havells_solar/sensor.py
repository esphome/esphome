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
    CONF_REACTIVE_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_MINUTE,
    UNIT_VOLT,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)

CONF_ENERGY_PRODUCTION_DAY = "energy_production_day"
CONF_TOTAL_ENERGY_PRODUCTION = "total_energy_production"
CONF_TOTAL_GENERATION_TIME = "total_generation_time"
CONF_TODAY_GENERATION_TIME = "today_generation_time"
CONF_PV1 = "pv1"
CONF_PV2 = "pv2"
UNIT_KILOWATT_HOURS = "kWh"
UNIT_HOURS = "h"
UNIT_KOHM = "kΩ"
UNIT_MILLIAMPERE = "mA"


CONF_INVERTER_MODULE_TEMP = "inverter_module_temp"
CONF_INVERTER_INNER_TEMP = "inverter_inner_temp"
CONF_INVERTER_BUS_VOLTAGE = "inverter_bus_voltage"
CONF_VOLTAGE_SAMPLED_BY_SECONDARY_CPU = "voltage_sampled_by_secondary_cpu"
CONF_INSULATION_OF_P_TO_GROUND = "insulation_of_p_to_ground"
CONF_INSULATION_OF_PV_N_TO_GROUND = "insulation_of_pv_n_to_ground"
CONF_GFCI_VALUE = "gfci_value"
CONF_DCI_OF_R = "dci_of_r"
CONF_DCI_OF_S = "dci_of_s"
CONF_DCI_OF_T = "dci_of_t"


AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@sourabhjaiswal"]

havells_solar_ns = cg.esphome_ns.namespace("havells_solar")
HavellsSolar = havells_solar_ns.class_(
    "HavellsSolar", cg.PollingComponent, modbus.ModbusDevice
)

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}
PV_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
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
    CONF_VOLTAGE_SAMPLED_BY_SECONDARY_CPU: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_INSULATION_OF_P_TO_GROUND: sensor.sensor_schema(
        unit_of_measurement=UNIT_KOHM,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PHASE_SCHEMA = cv.Schema(
    {cv.Optional(sensor): schema for sensor, schema in PHASE_SENSORS.items()}
)
PV_SCHEMA = cv.Schema(
    {cv.Optional(sensor): schema for sensor, schema in PV_SENSORS.items()}
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HavellsSolar),
            cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
            cv.Optional(CONF_PV1): PV_SCHEMA,
            cv.Optional(CONF_PV2): PV_SCHEMA,
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACTIVE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY_PRODUCTION_DAY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TOTAL_ENERGY_PRODUCTION): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TOTAL_GENERATION_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOURS,
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TODAY_GENERATION_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_INVERTER_MODULE_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INVERTER_INNER_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INVERTER_BUS_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INSULATION_OF_PV_N_TO_GROUND): sensor.sensor_schema(
                unit_of_measurement=UNIT_KOHM,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GFCI_VALUE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DCI_OF_R): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DCI_OF_S): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DCI_OF_T): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    if CONF_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_frequency_sensor(sens))

    if CONF_ACTIVE_POWER in config:
        sens = await sensor.new_sensor(config[CONF_ACTIVE_POWER])
        cg.add(var.set_active_power_sensor(sens))

    if CONF_REACTIVE_POWER in config:
        sens = await sensor.new_sensor(config[CONF_REACTIVE_POWER])
        cg.add(var.set_reactive_power_sensor(sens))

    if CONF_ENERGY_PRODUCTION_DAY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY_PRODUCTION_DAY])
        cg.add(var.set_today_production_sensor(sens))

    if CONF_TOTAL_ENERGY_PRODUCTION in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_ENERGY_PRODUCTION])
        cg.add(var.set_total_energy_production_sensor(sens))

    if CONF_TOTAL_GENERATION_TIME in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_GENERATION_TIME])
        cg.add(var.set_total_generation_time_sensor(sens))

    if CONF_TODAY_GENERATION_TIME in config:
        sens = await sensor.new_sensor(config[CONF_TODAY_GENERATION_TIME])
        cg.add(var.set_today_generation_time_sensor(sens))

    if CONF_INVERTER_MODULE_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_INVERTER_MODULE_TEMP])
        cg.add(var.set_inverter_module_temp_sensor(sens))

    if CONF_INVERTER_INNER_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_INVERTER_INNER_TEMP])
        cg.add(var.set_inverter_inner_temp_sensor(sens))

    if CONF_INVERTER_BUS_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_INVERTER_BUS_VOLTAGE])
        cg.add(var.set_inverter_bus_voltage_sensor(sens))

    if CONF_INSULATION_OF_PV_N_TO_GROUND in config:
        sens = await sensor.new_sensor(config[CONF_INSULATION_OF_PV_N_TO_GROUND])
        cg.add(var.set_insulation_pv_n_to_ground_sensor(sens))

    if CONF_GFCI_VALUE in config:
        sens = await sensor.new_sensor(config[CONF_GFCI_VALUE])
        cg.add(var.set_gfci_value_sensor(sens))

    if CONF_DCI_OF_R in config:
        sens = await sensor.new_sensor(config[CONF_DCI_OF_R])
        cg.add(var.set_dci_of_r_sensor(sens))

    if CONF_DCI_OF_S in config:
        sens = await sensor.new_sensor(config[CONF_DCI_OF_S])
        cg.add(var.set_dci_of_s_sensor(sens))

    if CONF_DCI_OF_T in config:
        sens = await sensor.new_sensor(config[CONF_DCI_OF_T])
        cg.add(var.set_dci_of_t_sensor(sens))

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
