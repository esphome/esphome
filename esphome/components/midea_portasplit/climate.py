"""Midea PortaSplit climate platform (sync byte 0x00 protocol variant)."""

import esphome.codegen as cg
from esphome.components import (
    binary_sensor,
    climate,
    number,
    select,
    sensor,
    switch,
    text_sensor,
    uart,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BEEPER,
    CONF_ENERGY,
    CONF_HUMIDITY,
    CONF_OUTDOOR_TEMPERATURE,
    CONF_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_WATT,
)

CODEOWNERS = ["@Fexiven"]
DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "switch", "select", "number"]

CONF_OUTDOOR_FAN_SPEED = "outdoor_fan_speed"
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_EVAPORATOR_TEMPERATURE = "evaporator_temperature"
CONF_CONDENSER_TEMPERATURE = "condenser_temperature"
CONF_DISCHARGE_TEMPERATURE = "discharge_temperature"
CONF_SUCTION_TEMPERATURE = "suction_temperature"
CONF_COMPRESSOR_RUNTIME = "compressor_runtime"
CONF_INDOOR_FAN_RPM = "indoor_fan_rpm"
CONF_COMPRESSOR_FREQUENCY_G1 = "compressor_frequency_g1"
CONF_COMPRESSOR_CURRENT = "compressor_current"
CONF_SERIAL_NUMBER = "serial_number"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_DEFROST = "defrost"
CONF_ION = "ion"
CONF_SELF_CLEAN = "self_clean"
CONF_LED_DISPLAY = "led_display"
CONF_TEMPERATURE_RANGE = "temperature_range"
CONF_SILENT_MODE = "silent_mode"
CONF_POWER_LIMIT = "power_limit"
CONF_FAN_SPEED = "fan_speed"
CONF_TEMPERATURE_RANGE_MIN = "temperature_range_min"
CONF_TEMPERATURE_RANGE_MAX = "temperature_range_max"
CONF_ENERGY_FORMAT = "energy_format"
CONF_LOG_FRAMES = "log_frames"

midea_portasplit_ns = cg.esphome_ns.namespace("midea_portasplit")
PortaSplitClimate = midea_portasplit_ns.class_(
    "PortaSplitClimate", climate.Climate, uart.UARTDevice, cg.Component
)
PortaSplitSwitch = midea_portasplit_ns.class_("PortaSplitSwitch", switch.Switch)
PortaSplitSelect = midea_portasplit_ns.class_("PortaSplitSelect", select.Select)
PortaSplitNumber = midea_portasplit_ns.class_("PortaSplitNumber", number.Number)

PortaSplitSwitchType = midea_portasplit_ns.enum("PortaSplitSwitchType")
PortaSplitSelectType = midea_portasplit_ns.enum("PortaSplitSelectType")
PortaSplitNumberType = midea_portasplit_ns.enum("PortaSplitNumberType")

POWER_LIMIT_OPTIONS = ["50%", "75%", "100%"]

CONFIG_SCHEMA = (
    climate.climate_schema(PortaSplitClimate)
    .extend(
        {
            # ---- sensors ----
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_OUTDOOR_FAN_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                icon=ICON_FAN,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_HUMIDITY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPRESSOR_FREQUENCY): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:sine-wave",
            ),
            cv.Optional(CONF_EVAPORATOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CONDENSER_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DISCHARGE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:thermometer-high",
            ),
            cv.Optional(CONF_SUCTION_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPRESSOR_RUNTIME): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:timer-outline",
            ),
            cv.Optional(CONF_INDOOR_FAN_RPM): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=0,
                icon=ICON_FAN,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPRESSOR_FREQUENCY_G1): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                accuracy_decimals=0,
                icon="mdi:sine-wave",
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPRESSOR_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # ---- text sensors ----
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # ---- binary sensors ----
            cv.Optional(CONF_DEFROST): binary_sensor.binary_sensor_schema(
                icon="mdi:snowflake-melt",
            ),
            # ---- switches ----
            cv.Optional(CONF_ION): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:atom-variant",
            ),
            cv.Optional(CONF_BEEPER): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:volume-source",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_SELF_CLEAN): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:shimmer",
            ),
            cv.Optional(CONF_LED_DISPLAY): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:led-on",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_TEMPERATURE_RANGE): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:thermometer-lines",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_SILENT_MODE): switch.switch_schema(
                PortaSplitSwitch,
                icon="mdi:volume-off",
            ),
            # ---- selects ----
            cv.Optional(CONF_POWER_LIMIT): select.select_schema(
                PortaSplitSelect,
                icon="mdi:speedometer-slow",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            # ---- numbers ----
            cv.Optional(CONF_FAN_SPEED): number.number_schema(
                PortaSplitNumber,
                icon=ICON_FAN,
                unit_of_measurement=UNIT_PERCENT,
            ),
            cv.Optional(CONF_TEMPERATURE_RANGE_MIN): number.number_schema(
                PortaSplitNumber,
                icon="mdi:thermometer-chevron-down",
                unit_of_measurement=UNIT_CELSIUS,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_TEMPERATURE_RANGE_MAX): number.number_schema(
                PortaSplitNumber,
                icon="mdi:thermometer-chevron-up",
                unit_of_measurement=UNIT_CELSIUS,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            # ---- config ----
            cv.Optional(CONF_ENERGY_FORMAT, default="binary"): cv.one_of(
                "binary", "bcd", lower=True
            ),
            cv.Optional(CONF_LOG_FRAMES, default=False): cv.boolean,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "midea_portasplit", require_tx=True, require_rx=True
)

SENSORS = [
    (CONF_OUTDOOR_TEMPERATURE, "set_outdoor_temperature_sensor"),
    (CONF_POWER, "set_power_sensor"),
    (CONF_ENERGY, "set_energy_sensor"),
    (CONF_OUTDOOR_FAN_SPEED, "set_outdoor_fan_sensor"),
    (CONF_HUMIDITY, "set_humidity_sensor"),
    (CONF_COMPRESSOR_FREQUENCY, "set_compressor_freq_sensor"),
    (CONF_EVAPORATOR_TEMPERATURE, "set_evap_temp_sensor"),
    (CONF_CONDENSER_TEMPERATURE, "set_cond_temp_sensor"),
    (CONF_DISCHARGE_TEMPERATURE, "set_discharge_temp_sensor"),
    (CONF_SUCTION_TEMPERATURE, "set_suction_temp_sensor"),
    (CONF_COMPRESSOR_RUNTIME, "set_compressor_runtime_sensor"),
    (CONF_INDOOR_FAN_RPM, "set_indoor_fan_rpm_sensor"),
    (CONF_COMPRESSOR_FREQUENCY_G1, "set_comp_frequency_sensor"),
    (CONF_COMPRESSOR_CURRENT, "set_comp_current_sensor"),
]

TEXT_SENSORS = [
    (CONF_SERIAL_NUMBER, "set_serial_number_sensor"),
    (CONF_FIRMWARE_VERSION, "set_firmware_sensor"),
]

SWITCHES = [
    (CONF_ION, "SW_ION", "set_ion_switch"),
    (CONF_BEEPER, "SW_BEEPER", "set_beeper_switch"),
    (CONF_SELF_CLEAN, "SW_SELF_CLEAN", "set_self_clean_switch"),
    (CONF_LED_DISPLAY, "SW_LED", "set_led_switch"),
    (CONF_TEMPERATURE_RANGE, "SW_TEMP_RANGE", "set_temp_range_switch"),
    (CONF_SILENT_MODE, "SW_SILENT", "set_silent_switch"),
]

NUMBERS = [
    (CONF_FAN_SPEED, "NUM_FAN_SPEED", "set_fan_number", 1, 100, 1),
    (CONF_TEMPERATURE_RANGE_MIN, "NUM_RANGE_MIN", "set_range_min_number", 16, 30, 0.5),
    (CONF_TEMPERATURE_RANGE_MAX, "NUM_RANGE_MAX", "set_range_max_number", 16, 30, 0.5),
]


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_bcd_energy(config[CONF_ENERGY_FORMAT] == "bcd"))
    cg.add(var.set_log_frames(config[CONF_LOG_FRAMES]))

    # Sensors
    for key, setter in SENSORS:
        if conf := config.get(key):
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, setter)(sens))

    # Text sensors
    for key, setter in TEXT_SENSORS:
        if conf := config.get(key):
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(var, setter)(sens))

    # Binary sensors
    if defrost := config.get(CONF_DEFROST):
        bsens = await binary_sensor.new_binary_sensor(defrost)
        cg.add(var.set_defrost_sensor(bsens))

    # Switches
    for key, sw_type, setter in SWITCHES:
        if conf := config.get(key):
            sw = await switch.new_switch(conf)
            cg.add(sw.set_parent(var, getattr(PortaSplitSwitchType, sw_type)))
            cg.add(getattr(var, setter)(sw))

    # Selects
    if limit := config.get(CONF_POWER_LIMIT):
        sel = await select.new_select(limit, options=POWER_LIMIT_OPTIONS)
        cg.add(sel.set_parent(var, PortaSplitSelectType.SEL_POWER_LIMIT))
        cg.add(var.set_power_limit_select(sel))

    # Numbers
    for key, num_type, setter, mn, mx, step in NUMBERS:
        if conf := config.get(key):
            num = await number.new_number(conf, min_value=mn, max_value=mx, step=step)
            cg.add(num.set_parent(var, getattr(PortaSplitNumberType, num_type)))
            cg.add(getattr(var, setter)(num))
