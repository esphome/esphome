import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, i2c, sensor
from esphome.components.const import CONF_TIMING_BUDGET
from esphome.components.i2c import CONF_I2C_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ALLOW_OTHER_USES,
    CONF_ENABLE_PIN,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_NUMBER,
    CONF_OFFSET,
    CONF_TARGET,
    CONF_UPDATE_INTERVAL,
    ICON_ARROW_EXPAND_VERTICAL,
    SCHEDULER_DONT_RUN,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["i2c"]

vl53l4cx_ns = cg.esphome_ns.namespace("vl53l4cx")
VL53L4CXSensor = vl53l4cx_ns.class_(
    "VL53L4CXSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

Target = vl53l4cx_ns.enum("Target")
TARGET_OPTIONS = Target.options("nearest", "farthest", "strongest", prefix="TARGET_")

# ST's VL53L4CX_DISTANCEMODE_*; no short mode on the VL53L4CX
DISTANCE_MODES = {"medium": 2, "long": 3}

VL53L4CX_VERSION = "2.0.2"
# Other sensors that boot at 0x29; each family re-addresses only its own
OTHER_BOOT_AT_0X29 = ("vl53l0x", "vl53l1x")
VL6180X = "vl6180x"

CONF_DISTANCE_MODE = "distance_mode"
CONF_MIN_DISTANCE = "min_distance"

DEFAULT_ADDRESS = 0x29

validate_distance_mode = cv.enum(DISTANCE_MODES, lower=True)
validate_timing_budget = cv.All(
    cv.positive_time_period_milliseconds,
    # ST's driver caps the VL53L4CX at 200 ms
    cv.Range(min=cv.TimePeriod(milliseconds=10), max=cv.TimePeriod(milliseconds=200)),
)
DEFAULT_TIMING_BUDGET = cv.TimePeriod(milliseconds=33)
RECOMMENDED_FREQUENCY = 400_000
# Bus bits per measurement (about 150 bytes); 4.3 ms at 400 kHz on the bench
BITS_PER_MEASUREMENT = 1_900


def _interrupt_capable(pin: ConfigType) -> ConfigType:
    if pin.get(CONF_ALLOW_OTHER_USES):
        raise cv.Invalid(
            "The interrupt pin cannot be shared; give each sensor its own, or leave it out to poll over I2C"
        )
    if CORE.is_esp8266 and pin[CONF_NUMBER] == 16:
        raise cv.Invalid(
            "GPIO16 on ESP8266 cannot raise interrupts; use another pin, or leave it out to poll over I2C"
        )
    return pin


validate_interrupt_pin = cv.All(pins.internal_gpio_input_pin_schema, _interrupt_capable)


def validate_address(config: ConfigType) -> ConfigType:
    if config[CONF_ADDRESS] > 0x7F:
        raise cv.Invalid(
            "The address must be a 7-bit address, at most 0x7F", path=[CONF_ADDRESS]
        )
    if config[CONF_ADDRESS] != DEFAULT_ADDRESS and CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "An address other than 0x29 needs an enable_pin, so the sensor is back at 0x29 "
            "when it is re-addressed after a reboot",
            path=[CONF_ENABLE_PIN],
        )
    return config


def _warn_paced_by_budget(config: ConfigType) -> ConfigType:
    interval = config[CONF_UPDATE_INTERVAL].total_milliseconds
    budget = config.get(CONF_TIMING_BUDGET, DEFAULT_TIMING_BUDGET).total_milliseconds
    if interval != SCHEDULER_DONT_RUN and interval <= budget:
        _LOGGER.warning(
            "VL53L4CX '%s' starts a measurement every %d ms, but each takes %d ms, so updates are "
            "skipped. Set 'update_interval: never' to measure continuously.",
            config[CONF_ID],
            interval,
            budget,
        )
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    match = {CONF_I2C_ID: config[CONF_I2C_ID]}
    others = [
        other
        for other in OTHER_BOOT_AT_0X29
        if fv.platform_entries(domain=sensor.DOMAIN, platform=other, match=match)
    ]
    if any(
        conf[CONF_I2C_ID] == config[CONF_I2C_ID]
        for conf in fv.full_config.get().get(VL6180X, [])
    ):
        others.append(VL6180X)
    if others:
        raise cv.Invalid(
            f"A VL53L4CX cannot share an I2C bus with a {others[0].upper()}: each family holds "
            "only its own sensors in reset while re-addressing, so both would answer at 0x29",
            path=[CONF_I2C_ID],
        )
    if (
        config[CONF_ADDRESS] == DEFAULT_ADDRESS
        and len(fv.platform_entries(domain=sensor.DOMAIN, platform=DOMAIN, match=match))
        > 1
    ):
        raise cv.Invalid(
            f"VL53L4CX sensors sharing an I2C bus must all be re-addressed away from "
            f"0x{DEFAULT_ADDRESS:02X}",
            path=[CONF_ADDRESS],
        )
    return config


def _warn_slow_bus(config: ConfigType) -> ConfigType:
    frequency = fv.get_declaring_config(config[CONF_I2C_ID]).get(CONF_FREQUENCY)
    if frequency is not None and frequency < RECOMMENDED_FREQUENCY:
        _LOGGER.warning(
            "The I2C bus of VL53L4CX '%s' runs at %d kHz, so reading each measurement blocks the "
            "main loop for about %d ms. Set 'frequency: 400kHz' on the bus if its other devices allow it.",
            config[CONF_ID],
            frequency // 1000,
            BITS_PER_MEASUREMENT * 1000 // frequency,
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        VL53L4CXSensor,
        kind=sensor.LENGTH_M,
        accuracy_decimals=3,
        icon=ICON_ARROW_EXPAND_VERTICAL,
    )
    .extend(
        {
            cv.Optional(CONF_DISTANCE_MODE): validate_distance_mode,
            cv.Optional(CONF_TIMING_BUDGET): validate_timing_budget,
            cv.Optional(CONF_TARGET): cv.enum(TARGET_OPTIONS, lower=True),
            cv.Optional(CONF_MIN_DISTANCE): cv.All(
                cv.distance, cv.Range(min=0.0, max=6.0)
            ),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTERRUPT_PIN): validate_interrupt_pin,
            cv.Optional(CONF_OFFSET): cv.int_range(min=-1000, max=1000),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS)),
    validate_address,
)

_unique_address = fv.unique_platform_key(
    domain=sensor.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_I2C_ID, CONF_ADDRESS),
    message="Address 0x{address:02X} is used by more than one VL53L4CX sensor on this bus",
)

FINAL_VALIDATE_SCHEMA = cv.All(
    _final_validate, _unique_address, _warn_paced_by_budget, _warn_slow_bus
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await sensor.register_sensor(var, config)
    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/vl53l4cx", ref=VL53L4CX_VERSION)
    else:
        cg.add_library("esphome/VL53L4CX", VL53L4CX_VERSION)

    cg.add_optional(var.set_distance_mode, config.get(CONF_DISTANCE_MODE))
    if (budget := config.get(CONF_TIMING_BUDGET)) is not None:
        cg.add(var.set_timing_budget(budget.total_milliseconds))
    cg.add_optional(var.set_target, config.get(CONF_TARGET))
    if (min_distance := config.get(CONF_MIN_DISTANCE)) is not None:
        cg.add(var.set_min_distance(round(min_distance * 1000)))
    cg.add_optional(var.set_offset, config.get(CONF_OFFSET))
    if (enable_pin := config.get(CONF_ENABLE_PIN)) is not None:
        cg.add_define("USE_VL53L4CX_ENABLE_PIN")
        await cg.add_pin(var.set_enable_pin, enable_pin)
    if (interrupt_pin := config.get(CONF_INTERRUPT_PIN)) is not None:
        cg.add_define("USE_VL53L4CX_INTERRUPT_PIN")
        await cg.add_pin(var.set_interrupt_pin, interrupt_pin)
