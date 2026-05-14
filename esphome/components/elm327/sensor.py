import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
)

DEPENDENCIES = ["uart"]

CONF_ENGINE_RPM = "engine_rpm"
CONF_VEHICLE_SPEED = "vehicle_speed"
CONF_COOLANT_TEMPERATURE = "coolant_temperature"
CONF_ENGINE_LOAD = "engine_load"
CONF_THROTTLE_POSITION = "throttle_position"
CONF_INTAKE_AIR_TEMPERATURE = "intake_air_temperature"
CONF_MAF_RATE = "maf_rate"
CONF_FUEL_LEVEL = "fuel_level"
CONF_CUSTOM_PID = "custom_pid"
CONF_MODE = "mode"
CONF_PID = "pid"
CONF_RESPONSE_BYTES = "response_bytes"

UNIT_GRAMS_PER_SECOND = "g/s"

elm327_ns = cg.esphome_ns.namespace("elm327")
ELM327Component = elm327_ns.class_(
    "ELM327Component", cg.PollingComponent, uart.UARTDevice
)

_SENSOR_KEYS = [
    CONF_ENGINE_RPM,
    CONF_VEHICLE_SPEED,
    CONF_COOLANT_TEMPERATURE,
    CONF_ENGINE_LOAD,
    CONF_THROTTLE_POSITION,
    CONF_INTAKE_AIR_TEMPERATURE,
    CONF_MAF_RATE,
    CONF_FUEL_LEVEL,
    CONF_BATTERY_VOLTAGE,
]


def _validate_pid(value):
    if isinstance(value, int):
        if 0 <= value <= 0xFFFF:
            return value
        raise cv.Invalid(f"PID must be between 0x00 and 0xFFFF, got {value}")
    if isinstance(value, str):
        try:
            parsed = int(value, 16)
            if 0 <= parsed <= 0xFFFF:
                return parsed
            raise cv.Invalid(f"PID must be between 0x00 and 0xFFFF, got {value!r}")
        except ValueError:
            pass
    raise cv.Invalid(
        f"Invalid PID {value!r}: expected a hex value (e.g. '0xFF', 'FF', '0xB201', or 255)"
    )


CUSTOM_PID_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.Required(CONF_PID): _validate_pid,
        cv.Optional(CONF_MODE, default=0x01): cv.int_range(min=0x01, max=0x3F),
        cv.Optional(CONF_RESPONSE_BYTES, default=1): cv.int_range(min=1, max=4),
    }
)


def _validate_has_sensor(config):
    if not any(key in config for key in _SENSOR_KEYS) and not config.get(
        CONF_CUSTOM_PID
    ):
        raise cv.Invalid("At least one sensor must be configured")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ELM327Component),
            cv.Optional(CONF_ENGINE_RPM): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VEHICLE_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COOLANT_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENGINE_LOAD): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_THROTTLE_POSITION): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INTAKE_AIR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAF_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_GRAMS_PER_SECOND,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FUEL_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CUSTOM_PID): cv.ensure_list(CUSTOM_PID_SCHEMA),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_has_sensor,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if engine_rpm := config.get(CONF_ENGINE_RPM):
        sens = await sensor.new_sensor(engine_rpm)
        cg.add(var.set_engine_rpm_sensor(sens))

    if vehicle_speed := config.get(CONF_VEHICLE_SPEED):
        sens = await sensor.new_sensor(vehicle_speed)
        cg.add(var.set_vehicle_speed_sensor(sens))

    if coolant_temperature := config.get(CONF_COOLANT_TEMPERATURE):
        sens = await sensor.new_sensor(coolant_temperature)
        cg.add(var.set_coolant_temperature_sensor(sens))

    if engine_load := config.get(CONF_ENGINE_LOAD):
        sens = await sensor.new_sensor(engine_load)
        cg.add(var.set_engine_load_sensor(sens))

    if throttle_position := config.get(CONF_THROTTLE_POSITION):
        sens = await sensor.new_sensor(throttle_position)
        cg.add(var.set_throttle_position_sensor(sens))

    if intake_air_temperature := config.get(CONF_INTAKE_AIR_TEMPERATURE):
        sens = await sensor.new_sensor(intake_air_temperature)
        cg.add(var.set_intake_air_temperature_sensor(sens))

    if maf_rate := config.get(CONF_MAF_RATE):
        sens = await sensor.new_sensor(maf_rate)
        cg.add(var.set_maf_rate_sensor(sens))

    if fuel_level := config.get(CONF_FUEL_LEVEL):
        sens = await sensor.new_sensor(fuel_level)
        cg.add(var.set_fuel_level_sensor(sens))

    if battery_voltage := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(battery_voltage)
        cg.add(var.set_battery_voltage_sensor(sens))

    custom_pids = config.get(CONF_CUSTOM_PID, [])
    cg.add_define("ELM327_CUSTOM_PID_COUNT", len(custom_pids))
    for custom_pid_conf in custom_pids:
        sens = await sensor.new_sensor(custom_pid_conf)
        cg.add(
            var.add_custom_pid(
                custom_pid_conf[CONF_MODE],
                custom_pid_conf[CONF_PID],
                custom_pid_conf[CONF_RESPONSE_BYTES],
                sens,
            )
        )
