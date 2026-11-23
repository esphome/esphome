from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.i2c import CONF_I2C_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_INTERRUPT_PIN,
    CONF_OFFSET,
    CONF_TARGET,
    ICON_ARROW_EXPAND_VERTICAL,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["i2c"]

vl53l4cx_ns = cg.esphome_ns.namespace("vl53l4cx")
VL53L4CXSensor = vl53l4cx_ns.class_(
    "VL53L4CXSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

Target = vl53l4cx_ns.enum("Target")
TARGET_OPTIONS = Target.options("nearest", "farthest", "strongest", prefix="TARGET_")

# Values of ST's VL53L4CX_DISTANCEMODE_*
DISTANCE_MODES = {"short": 1, "medium": 2, "long": 3}

# ST's driver, forked by esphome-libs to run on any framework
LIBRARY = ("esphome/VL53L4CX", "2.0.0")
# ST time-of-flight sensors that also boot at 0x29; each family re-addresses its own sensors only
OTHER_BOOT_AT_0X29 = ("vl53l0x", "vl53l1x")

CONF_DISTANCE_MODE = "distance_mode"
CONF_TIMING_BUDGET = "timing_budget"
CONF_MIN_DISTANCE = "min_distance"

DEFAULT_ADDRESS = 0x29


def validate_address(config: ConfigType) -> ConfigType:
    if config[CONF_ADDRESS] != DEFAULT_ADDRESS and CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "An address other than 0x29 needs an enable_pin, so the sensor is back at 0x29 "
            "when it is re-addressed after a reboot",
            path=[CONF_ENABLE_PIN],
        )
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    # Every sensor boots at 0x29, so sensors sharing a bus are held in reset by their enable
    # pins and given their own addresses one at a time. validate_address already requires an
    # enable_pin for any other address.
    match = {CONF_I2C_ID: config[CONF_I2C_ID]}
    for other in OTHER_BOOT_AT_0X29:
        if fv.platform_entries(domain=sensor.DOMAIN, platform=other, match=match):
            raise cv.Invalid(
                f"A VL53L4CX cannot share an I2C bus with a {other.upper()}: each family holds "
                "only its own sensors in reset while re-addressing, so both would answer at 0x29",
                path=[CONF_I2C_ID],
            )
    siblings = fv.platform_entries(domain=sensor.DOMAIN, platform=DOMAIN, match=match)
    if len(siblings) < 2:
        return config
    address = config[CONF_ADDRESS]
    if address == DEFAULT_ADDRESS:
        raise cv.Invalid(
            f"VL53L4CX sensors sharing an I2C bus must all be re-addressed away from "
            f"0x{DEFAULT_ADDRESS:02X}",
            path=[CONF_ADDRESS],
        )
    if sum(conf[CONF_ADDRESS] == address for conf in siblings) > 1:
        raise cv.Invalid(
            f"Address 0x{address:02X} is used by more than one VL53L4CX sensor on this bus",
            path=[CONF_ADDRESS],
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
            # Defaults to long
            cv.Optional(CONF_DISTANCE_MODE): cv.enum(DISTANCE_MODES, lower=True),
            # Defaults to 33ms
            cv.Optional(CONF_TIMING_BUDGET): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=10),
                    max=cv.TimePeriod(milliseconds=1000),
                ),
            ),
            # Which of the up to four targets in a measurement the sensor reports; defaults to nearest
            cv.Optional(CONF_TARGET): cv.enum(TARGET_OPTIONS, lower=True),
            # Targets closer than this are ignored, e.g. dust or insects on the cover glass
            cv.Optional(CONF_MIN_DISTANCE): cv.All(
                cv.distance, cv.Range(min=0.0, max=6.0)
            ),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            # The sensor's GPIO1, low while a measurement is ready; saves an I2C poll per loop
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_OFFSET): cv.int_range(min=-1000, max=1000),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS)),
    validate_address,
)

FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await sensor.register_sensor(var, config)
    cg.add_library(*LIBRARY)

    cg.add_optional(var.set_distance_mode, config.get(CONF_DISTANCE_MODE))
    if (budget := config.get(CONF_TIMING_BUDGET)) is not None:
        cg.add(var.set_timing_budget(budget.total_milliseconds))
    cg.add_optional(var.set_target, config.get(CONF_TARGET))
    if (min_distance := config.get(CONF_MIN_DISTANCE)) is not None:
        cg.add(var.set_min_distance(round(min_distance * 1000)))
    cg.add_optional(var.set_offset, config.get(CONF_OFFSET))
    await cg.add_pin(var.set_enable_pin, config.get(CONF_ENABLE_PIN))
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
