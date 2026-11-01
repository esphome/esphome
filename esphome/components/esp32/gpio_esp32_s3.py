import logging

from esphome.const import CONF_DISABLED, CONF_MODE, PLATFORM_ESP32
from esphome.pins import PIN_SCHEMA_REGISTRY
from esphome.types import ConfigType

from .gpio_common import ESP32PinSpec
from .gpio_esp32_s2 import s2_s3_error_pins

_ESP32S3R8_PSRAM_PINS = {
    33: "SPIIO4",
    34: "SPIIO5",
    35: "SPIIO6",
    36: "SPIIO7",
    37: "SPIDQS",
}

_LOGGER = logging.getLogger(__name__)

ESP32_S3_PIN_SPEC = ESP32PinSpec(
    max_pin=48,
    strapping_pins=frozenset({0, 3, 45, 46}),
    usb_jtag_pins=frozenset({19, 20}),
    error_pins=s2_s3_error_pins("ESP32-S3"),
)


def esp32_s3_final_validate_pins(full_config: ConfigType) -> None:
    """Warn about GPIO33-37 usage, but only when octal PSRAM (which uses them) is set.

    These pins are only taken by the PSRAM interface in octal mode (ESP32-S3R8 /
    S3R8V); on quad-PSRAM variants -- or when the psram block is disabled, so the
    octal interface is never configured -- they are free. The per-pin validator
    can't know the PSRAM mode, so the check is deferred here, where
    PIN_SCHEMA_REGISTRY.pins_used already lists every used pin.
    """
    # Imported locally to avoid circular import issues
    from esphome.components.psram import DOMAIN as PSRAM_DOMAIN, TYPE_OCTAL

    psram_config = full_config.get(PSRAM_DOMAIN, {})
    if psram_config.get(CONF_DISABLED) or psram_config.get(CONF_MODE) != TYPE_OCTAL:
        return
    for number in sorted(
        number
        for key, _client_id, number in PIN_SCHEMA_REGISTRY.pins_used
        if key == PLATFORM_ESP32 and number in _ESP32S3R8_PSRAM_PINS
    ):
        _LOGGER.warning(
            "GPIO%d is used by the PSRAM interface in octal mode and should be avoided",
            number,
        )
