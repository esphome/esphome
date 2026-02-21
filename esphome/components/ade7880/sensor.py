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

# Tuple of power channel phases
POWER_PHASES = (CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C)

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
    config: ConfigType, channel_key: str, sensor_types: tuple
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
    # Process power channels
    for channel in POWER_PHASES:
        process_channel_sensors(config, channel, POWER_SENSOR_TYPES)

    # Process neutral channel
    process_channel_sensors(config, CONF_NEUTRAL, (CONF_CURRENT,))

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


async def neutral_channel(config):
    var = cg.new_Pvariable(config[CONF_ID])

    current = config[CONF_CURRENT]
    sens = await sensor.new_sensor(current)
    cg.add(var.set_current(sens))

    cg.add(
        var.set_current_gain_calibration(config[CONF_CALIBRATION][CONF_CURRENT_GAIN])
    )

    return var


async def power_channel(config):
    var = cg.new_Pvariable(config[CONF_ID])

    for sensor_type in POWER_SENSOR_TYPES:
        if conf := config.get(sensor_type):
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_{sensor_type}")(sens))

    for calib_type in [
        CONF_CURRENT_GAIN,
        CONF_VOLTAGE_GAIN,
        CONF_POWER_GAIN,
        CONF_PHASE_ANGLE,
    ]:
        cg.add(
            getattr(var, f"set_{calib_type}_calibration")(
                config[CONF_CALIBRATION][calib_type]
            )
        )

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if irq0_pin := config.get(CONF_IRQ0_PIN):
        pin = await cg.gpio_pin_expression(irq0_pin)
        cg.add(var.set_irq0_pin(pin))

    pin = await cg.gpio_pin_expression(config[CONF_IRQ1_PIN])
    cg.add(var.set_irq1_pin(pin))

    if reset_pin := config.get(CONF_RESET_PIN):
        pin = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(pin))

    if frequency := config.get(CONF_FREQUENCY):
        cg.add(var.set_frequency(frequency))

    if channel := config.get(CONF_PHASE_A):
        chan = await power_channel(channel)
        cg.add(var.set_channel_a(chan))

    if channel := config.get(CONF_PHASE_B):
        chan = await power_channel(channel)
        cg.add(var.set_channel_b(chan))

    if channel := config.get(CONF_PHASE_C):
        chan = await power_channel(channel)
        cg.add(var.set_channel_c(chan))

    if channel := config.get(CONF_NEUTRAL):
        chan = await neutral_channel(channel)
        cg.add(var.set_channel_n(chan))
