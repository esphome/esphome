from pathlib import Path
import hashlib
import re
import requests


import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, sensor, uart
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_GAMMA_CORRECT,
    CONF_POWER,
    CONF_VOLTAGE,
    CONF_CURRENT,
    CONF_VERSION,
    CONF_URL,
    CONF_UPDATE_INTERVAL,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    CONF_MIN_BRIGHTNESS,
    CONF_MAX_BRIGHTNESS,
)
from esphome.core import HexInt, CORE

DOMAIN = "shelly_dimmer"
AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["uart", "esp8266"]

shelly_dimmer_ns = cg.esphome_ns.namespace("shelly_dimmer")
ShellyDimmer = shelly_dimmer_ns.class_(
    "ShellyDimmer", light.LightOutput, cg.PollingComponent, uart.UARTDevice
)

CONF_FIRMWARE = "firmware"
CONF_SHA256 = "sha256"
CONF_UPDATE = "update"

CONF_LEADING_EDGE = "leading_edge"
CONF_WARMUP_BRIGHTNESS = "warmup_brightness"
# CONF_WARMUP_TIME = "warmup_time"


CONF_NRST_PIN = "nrst_pin"
CONF_BOOT0_PIN = "boot0_pin"

KNOWN_FIRMWARE = {
    "51.5": (
        "https://github.com/jamesturton/shelly-dimmer-stm32/releases/download/v51.5/shelly-dimmer-stm32_v51.5.bin",
        "553fc1d78ed113227af7683eaa9c26189a961c4ea9a48000fb5aa8f8ac5d7b60",
    ),
    "51.6": (
        "https://github.com/jamesturton/shelly-dimmer-stm32/releases/download/v51.6/shelly-dimmer-stm32_v51.6.bin",
        "eda483e111c914723a33f5088f1397d5c0b19333db4a88dc965636b976c16c36",
    ),
    "51.7": (
        "https://github.com/jamesturton/shelly-dimmer-stm32/releases/download/v51.7/shelly-dimmer-stm32_v51.7.bin",
        "7a20f1c967c469917368a79bc56498009045237080408cef7190743e08031889",
    ),
}


def parse_firmware_version(value):
    match = re.match(r"(\d+).(\d+)", value)
    if match is None:
        raise ValueError(f"Not a valid version number {value}")
    major = int(match[1])
    minor = int(match[2])
    return major, minor


def get_firmware(value):
    if not value[CONF_UPDATE]:
        return None

    def dl(url):
        try:
            req = requests.get(url, timeout=30)
            req.raise_for_status()
        except requests.exceptions.RequestException as e:
            raise cv.Invalid(f"Could not download firmware file ({url}): {e}")

        h = hashlib.new("sha256")
        h.update(req.content)
        return req.content, h.hexdigest()

    url = value[CONF_URL]

    if CONF_SHA256 in value:  # we have a hash, enable caching
        path = Path(CORE.data_dir) / DOMAIN / (value[CONF_SHA256] + "_fw_stm.bin")

        if not path.is_file():
            firmware_data, dl_hash = dl(url)

            if dl_hash != value[CONF_SHA256]:
                raise cv.Invalid(
                    f"Hash mismatch for {url}: {dl_hash} != {value[CONF_SHA256]}"
                )

            path.parent.mkdir(exist_ok=True, parents=True)
            path.write_bytes(firmware_data)

        else:
            firmware_data = path.read_bytes()
    else:  # no caching, download every time
        firmware_data, dl_hash = dl(url)

    return [HexInt(x) for x in firmware_data]


def validate_firmware(value):
    config = value.copy()
    if CONF_URL not in config:
        try:
            config[CONF_URL], config[CONF_SHA256] = KNOWN_FIRMWARE[config[CONF_VERSION]]
        except KeyError as e:
            raise cv.Invalid(
                f"Firmware {config[CONF_VERSION]} is unknown, please specify an '{CONF_URL}' ..."
            ) from e
    get_firmware(config)
    return config


def validate_sha256(value):
    value = cv.string(value)
    if not value.isalnum() or not len(value) == 64:
        raise ValueError(f"Not a valid SHA256 hex string: {value}")
    return value


def validate_version(value):
    parse_firmware_version(value)
    return value


CONFIG_SCHEMA = (
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ShellyDimmer),
            cv.Optional(CONF_FIRMWARE, default="51.6"): cv.maybe_simple_value(
                {
                    cv.Optional(CONF_URL): cv.url,
                    cv.Optional(CONF_SHA256): validate_sha256,
                    cv.Required(CONF_VERSION): validate_version,
                    cv.Optional(CONF_UPDATE, default=False): cv.boolean,
                },
                validate_firmware,  # converts a simple version key to generate the full url
                key=CONF_VERSION,
            ),
            cv.Optional(CONF_NRST_PIN, default="GPIO5"): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BOOT0_PIN, default="GPIO4"): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LEADING_EDGE, default=False): cv.boolean,
            cv.Optional(CONF_WARMUP_BRIGHTNESS, default=100): cv.uint16_t,
            # cv.Optional(CONF_WARMUP_TIME, default=20): cv.uint16_t,
            cv.Optional(CONF_MIN_BRIGHTNESS, default=0): cv.uint16_t,
            cv.Optional(CONF_MAX_BRIGHTNESS, default=1000): cv.uint16_t,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                device_class=DEVICE_CLASS_CURRENT,
                accuracy_decimals=2,
            ),
            # Change the default gamma_correct setting.
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    fw_hex = get_firmware(config[CONF_FIRMWARE])
    fw_major, fw_minor = parse_firmware_version(config[CONF_FIRMWARE][CONF_VERSION])

    if fw_hex is not None:
        cg.add_define("USE_SHD_FIRMWARE_DATA", fw_hex)
    cg.add_define("USE_SHD_FIRMWARE_MAJOR_VERSION", fw_major)
    cg.add_define("USE_SHD_FIRMWARE_MINOR_VERSION", fw_minor)

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    config.pop(
        CONF_UPDATE_INTERVAL
    )  # drop UPDATE_INTERVAL as it does not apply to the light component

    yield light.register_light(var, config)
    yield uart.register_uart_device(var, config)

    nrst_pin = yield cg.gpio_pin_expression(config[CONF_NRST_PIN])
    cg.add(var.set_nrst_pin(nrst_pin))
    boot0_pin = yield cg.gpio_pin_expression(config[CONF_BOOT0_PIN])
    cg.add(var.set_boot0_pin(boot0_pin))

    cg.add(var.set_leading_edge(config[CONF_LEADING_EDGE]))
    cg.add(var.set_warmup_brightness(config[CONF_WARMUP_BRIGHTNESS]))
    # cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))
    cg.add(var.set_min_brightness(config[CONF_MIN_BRIGHTNESS]))
    cg.add(var.set_max_brightness(config[CONF_MAX_BRIGHTNESS]))

    for key in [CONF_POWER, CONF_VOLTAGE, CONF_CURRENT]:
        if key not in config:
            continue

        conf = config[key]
        sens = yield sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))
