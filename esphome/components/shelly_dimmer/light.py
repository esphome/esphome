import hashlib
from pathlib import Path
import re
from typing import Any

from esphome import external_files, pins
import esphome.codegen as cg
from esphome.components import light, sensor, uart
from esphome.components.const import CONF_SHA256
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_GAMMA_CORRECT,
    CONF_MAX_BRIGHTNESS,
    CONF_MIN_BRIGHTNESS,
    CONF_OUTPUT_ID,
    CONF_POWER,
    CONF_UPDATE_INTERVAL,
    CONF_URL,
    CONF_VERSION,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.core import HexInt
from esphome.external_files import RemoteFile
from esphome.types import ConfigType

DOMAIN = "shelly_dimmer"
AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["uart", "esp8266"]

shelly_dimmer_ns = cg.esphome_ns.namespace("shelly_dimmer")
ShellyDimmer = shelly_dimmer_ns.class_(
    "ShellyDimmer", light.LightOutput, cg.PollingComponent, uart.UARTDevice
)

CONF_FIRMWARE = "firmware"
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


def parse_firmware_version(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"(\d+)\.(\d+)", value)
    if match is None:
        raise ValueError(f"Not a valid version number {value}")
    major = int(match[1])
    minor = int(match[2])
    return major, minor


def _firmware_cache_path(name: str) -> Path:
    return external_files.compute_local_file_dir(DOMAIN) / f"{name}_fw_stm.bin"


def _firmware_path(url: str, sha: str | None) -> Path:
    """Cache path for a firmware blob: sha-keyed when verifiable, else
    URL-keyed. Shared by the validator and the prefetch hook."""
    return _firmware_cache_path(
        sha.lower() if sha else external_files.url_cache_key(url)
    )


def get_firmware(value: ConfigType) -> list[HexInt] | None:
    if not value[CONF_UPDATE]:
        return None

    url = value[CONF_URL]

    if expected := value.get(CONF_SHA256):
        expected = expected.lower()
        path = _firmware_path(url, expected)
        if path.is_file():
            firmware_data = path.read_bytes()
            if hashlib.sha256(firmware_data).hexdigest() == expected:
                return [HexInt(x) for x in firmware_data]
            # A corrupted or foreign cache entry must never be trusted just
            # because the file exists; discard it and download again.
            path.unlink()
        firmware_data = external_files.download_content(url, path)
        if (actual := hashlib.sha256(firmware_data).hexdigest()) != expected:
            path.unlink(missing_ok=True)
            raise cv.Invalid(f"Hash mismatch for {url}: {actual} != {expected}")
    else:
        # No hash to verify the bytes, so an unrevalidated copy is an
        # error rather than a silent fallback.
        firmware_data = external_files.download_content(
            url,
            _firmware_path(url, None),
            allow_stale=False,
        )

    return [HexInt(x) for x in firmware_data]


def _extract_firmware_ref(entry: ConfigType) -> RemoteFile | None:
    firmware = entry.get(CONF_FIRMWARE)
    if not isinstance(firmware, dict):
        return None
    try:
        # cv.boolean, not truthiness: `update: "false"` is a valid False.
        if not cv.boolean(firmware.get(CONF_UPDATE, False)):
            return None
    except cv.Invalid:
        return None
    url = firmware.get(CONF_URL)
    sha = firmware.get(CONF_SHA256)
    if url is None and (known := KNOWN_FIRMWARE.get(str(firmware.get(CONF_VERSION)))):
        url, sha = known
    if not isinstance(url, str):
        return None
    if sha is not None:
        # Reject anything but a well-formed hash; a raw string would
        # otherwise become a path component before validation runs.
        try:
            sha = validate_sha256(sha)
        except (cv.Invalid, ValueError, TypeError):
            return None
    path = _firmware_path(url, sha)
    if sha is not None and path.is_file():
        # Content-addressed and already on disk; get_firmware verifies it
        # by hash, so there is nothing to revalidate.
        return None
    # No hash means no stale copies, matching the validator's policy.
    return RemoteFile(url, path, allow_stale=sha is not None)


PREFETCH_FILES = external_files.single_stage_prefetch(_extract_firmware_ref)


def validate_firmware(value: ConfigType) -> ConfigType:
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


def validate_sha256(value: Any) -> str:
    value = cv.string(value)
    if not re.fullmatch(r"[0-9a-fA-F]{64}", value):
        raise ValueError(f"Not a valid SHA256 hex string: {value}")
    return value


def validate_version(value: str) -> str:
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
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                device_class=DEVICE_CLASS_CURRENT,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # Change the default gamma_correct setting.
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "shelly_dimmer", baud_rate=115200, require_rx=True, require_tx=True
)


async def to_code(config: ConfigType) -> None:
    fw_hex = get_firmware(config[CONF_FIRMWARE])
    fw_major, fw_minor = parse_firmware_version(config[CONF_FIRMWARE][CONF_VERSION])

    if fw_hex is not None:
        cg.add_define("USE_SHD_FIRMWARE_DATA", fw_hex)
    cg.add_define("USE_SHD_FIRMWARE_MAJOR_VERSION", fw_major)
    cg.add_define("USE_SHD_FIRMWARE_MINOR_VERSION", fw_minor)

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    config.pop(
        CONF_UPDATE_INTERVAL
    )  # drop UPDATE_INTERVAL as it does not apply to the light component

    await light.register_light(var, config)
    await uart.register_uart_device(var, config)

    nrst_pin = await cg.gpio_pin_expression(config[CONF_NRST_PIN])
    cg.add(var.set_nrst_pin(nrst_pin))
    boot0_pin = await cg.gpio_pin_expression(config[CONF_BOOT0_PIN])
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
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))
