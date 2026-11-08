from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_APPARENT_POWER,
    CONF_CALIBRATION,
    CONF_CURRENT,
    CONF_FORWARD_ACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_NAME,
    CONF_PHASE_A,
    CONF_PHASE_ANGLE,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_POWER_FACTOR,
    CONF_RESET_PIN,
    CONF_REVERSE_ACTIVE_ENERGY,
    CONF_VOLTAGE,
    CONF_VOLTAGE_GAIN,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE_HOURS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

ade7880_ns = cg.esphome_ns.namespace("ade7880")
ADE7880 = ade7880_ns.class_("ADE7880", cg.PollingComponent, i2c.I2CDevice)
NeutralChannel = ade7880_ns.struct("NeutralChannel")
PowerChannel = ade7880_ns.struct("PowerChannel")

CONF_CURRENT_GAIN = "current_gain"
CONF_IRQ0_PIN = "irq0_pin"
CONF_IRQ1_PIN = "irq1_pin"
CONF_POWER_GAIN = "power_gain"

CONF_NEUTRAL = "neutral"

# Tuple of sensor types that can be configured for power channels
POWER_SENSOR_TYPES = (
    CONF_CURRENT,
    CONF_VOLTAGE,
    CONF_ACTIVE_POWER,
    CONF_APPARENT_POWER,
    CONF_POWER_FACTOR,
    CONF_FORWARD_ACTIVE_ENERGY,
    CONF_REVERSE_ACTIVE_ENERGY,
)
POWER_CALIBRATION_TYPES = (
    CONF_CURRENT_GAIN,
    CONF_VOLTAGE_GAIN,
    CONF_POWER_GAIN,
    CONF_PHASE_ANGLE,
)

# Channel config key, ADE7880 setter, sensor types, calibration types
CHANNELS = (
    (CONF_PHASE_A, "set_channel_a", POWER_SENSOR_TYPES, POWER_CALIBRATION_TYPES),
    (CONF_PHASE_B, "set_channel_b", POWER_SENSOR_TYPES, POWER_CALIBRATION_TYPES),
    (CONF_PHASE_C, "set_channel_c", POWER_SENSOR_TYPES, POWER_CALIBRATION_TYPES),
    (CONF_NEUTRAL, "set_channel_n", (CONF_CURRENT,), (CONF_CURRENT_GAIN,)),
)

NEUTRAL_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NeutralChannel),
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Required(CONF_CURRENT): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Required(CONF_CALIBRATION): cv.Schema(
            {
                cv.Required(CONF_CURRENT_GAIN): cv.int_,
            },
        ),
    }
)

POWER_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PowerChannel),
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_VOLTAGE): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_CURRENT): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_ACTIVE_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_APPARENT_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT_AMPS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_APPARENT_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_POWER_FACTOR): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_FORWARD_ACTIVE_ENERGY): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_REVERSE_ACTIVE_ENERGY): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            key=CONF_NAME,
        ),
        cv.Required(CONF_CALIBRATION): cv.Schema(
            {
                cv.Required(CONF_CURRENT_GAIN): cv.int_,
                cv.Required(CONF_VOLTAGE_GAIN): cv.int_,
                cv.Required(CONF_POWER_GAIN): cv.int_,
                cv.Required(CONF_PHASE_ANGLE): cv.int_,
            },
        ),
    }
)


def prefix_sensor_name(
    sensor_conf: ConfigType,
    channel_name: str,
    channel_config: ConfigType,
    sensor_type: str,
) -> None:
    """Helper to prefix sensor name with channel name.

    Args:
        sensor_conf: The sensor configuration (dict or string)
        channel_name: The channel name to prefix with
        channel_config: The channel configuration to update
        sensor_type: The sensor type key in the channel config
    """
    if isinstance(sensor_conf, dict) and CONF_NAME in sensor_conf:
        sensor_name = sensor_conf[CONF_NAME]
        if sensor_name and not sensor_name.startswith(channel_name):
            sensor_conf[CONF_NAME] = f"{channel_name} {sensor_name}"
    elif isinstance(sensor_conf, str):
        # Simple value case - convert to dict with prefixed name
        channel_config[sensor_type] = {CONF_NAME: f"{channel_name} {sensor_conf}"}


def process_channel_sensors(
    config: ConfigType, channel_key: str, sensor_types: tuple[str, ...]
) -> None:
    """Process sensors for a channel and prefix their names.

    Args:
        config: The main configuration
        channel_key: The channel key (e.g., CONF_PHASE_A, CONF_NEUTRAL)
        sensor_types: Tuple of sensor types to process for this channel
    """
    if not (channel_config := config.get(channel_key)) or not (
        channel_name := channel_config.get(CONF_NAME)
    ):
        return

    for sensor_type in sensor_types:
        if sensor_conf := channel_config.get(sensor_type):
            prefix_sensor_name(sensor_conf, channel_name, channel_config, sensor_type)


def preprocess_channels(config: ConfigType) -> ConfigType:
    """Preprocess channel configurations to add channel name prefix to sensor names."""
    for channel_key, _, sensor_types, _ in CHANNELS:
        process_channel_sensors(config, channel_key, sensor_types)

    return config


CONFIG_SCHEMA = cv.All(
    preprocess_channels,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADE7880),
            cv.Optional(CONF_FREQUENCY, default="50Hz"): cv.All(
                cv.frequency, cv.float_range(min=45.0, max=66.0)
            ),
            cv.Optional(CONF_IRQ0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_IRQ1_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_PHASE_A): POWER_CHANNEL_SCHEMA,
            cv.Optional(CONF_PHASE_B): POWER_CHANNEL_SCHEMA,
            cv.Optional(CONF_PHASE_C): POWER_CHANNEL_SCHEMA,
            cv.Optional(CONF_NEUTRAL): NEUTRAL_CHANNEL_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38)),
)


async def new_channel(
    config: ConfigType,
    sensor_types: tuple[str, ...],
    calibration_types: tuple[str, ...],
) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])

    sensors = sensor.sub_sensors(config)
    for sensor_type in sensor_types:
        await sensors(sensor_type, getattr(var, f"set_{sensor_type}"))

    calibration = config[CONF_CALIBRATION]
    for calib_type in calibration_types:
        cg.add(getattr(var, f"set_{calib_type}_calibration")(calibration[calib_type]))

    return var


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    await cg.add_pin(var.set_irq0_pin, config.get(CONF_IRQ0_PIN))
    await cg.add_pin(var.set_irq1_pin, config[CONF_IRQ1_PIN])
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))

    for channel_key, setter, sensor_types, calibration_types in CHANNELS:
        if channel := config.get(channel_key):
            chan = await new_channel(channel, sensor_types, calibration_types)
            cg.add(getattr(var, setter)(chan))
