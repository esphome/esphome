"""Validation for the `mipi_csi` component.

`validate_resolution` runs while this component's own section is checked. The other two need to see
the whole configuration -- how fast PSRAM is clocked, and whether something else already powers the
camera's supply rail -- so they run from `final_validate` instead.
"""

import logging
import re

from esphome.components.esp32.const import VARIANT_ESP32P4
from esphome.components.esp_ldo import DOMAIN as ESP_LDO_DOMAIN
from esphome.components.psram import SPIRAM_SPEEDS
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_DISABLED,
    CONF_RESOLUTION,
    CONF_SPEED,
    CONF_VOLTAGE,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import CONF_FRAMERATE, CONF_INIT_LDO, CONF_PIXEL_FORMAT

_LOGGER = logging.getLogger(__name__)

_RESOLUTION_PATTERN = re.compile(r"^\s*(\d+)\s*[xX]\s*(\d+)\s*$")


def validate_resolution(value: str) -> tuple[int, int]:
    """Parses a `WIDTHxHEIGHT` resolution into a pair of pixel counts."""
    value = cv.string(value)
    match = _RESOLUTION_PATTERN.match(value)
    if match is None:
        raise cv.Invalid(f"Resolution must be written as WIDTHxHEIGHT, got '{value}'")
    return int(match.group(1)), int(match.group(2))


# Bytes each pixel occupies in the capture buffer, used to estimate PSRAM traffic. Must cover every
# format offered by `PIXEL_FORMATS`.
PIXEL_FORMAT_BYTES = {
    "RGB565": 2,
    "RGB888": 3,
    "YUV422": 2,
    "GRAYSCALE": 1,
}

# The P4 reaches PSRAM over a 16-bit double data rate bus, so a clock of N MHz moves 4N MB/s at
# best. Refresh, arbitration between the camera and JPEG engines, and CPU traffic all eat into
# that, so only part of it is really available for the camera.
PSRAM_BYTES_PER_MHZ = 4
PSRAM_USABLE_FRACTION = 0.8

# The MIPI D-PHY draws its 2.5 V supply from on-chip LDO channel 3, and esp_video acquires that
# channel itself unless it is told not to.
MIPI_DPHY_LDO_CHANNEL = 3
MIPI_DPHY_LDO_VOLTAGE = 2.5


def _check_psram_bandwidth(config: ConfigType) -> None:
    """Warn when PSRAM is too slow to carry the configured video.

    Every frame crosses PSRAM twice: the camera writes it, then the JPEG engine reads it back.
    When the two together outrun the bus nothing reports an error; the engines simply run short of
    data and the picture is quietly corrupted, which is very hard to recognise for what it is.
    """
    psram_config = fv.full_config.get().get("psram")
    if psram_config is None or psram_config.get(CONF_DISABLED):
        return

    resolution = config.get(CONF_RESOLUTION)
    if resolution is None:
        # Without an explicit resolution the sensor picks its own, so there is nothing to estimate.
        return

    speed_mhz = int(psram_config[CONF_SPEED][:-3])
    available = speed_mhz * PSRAM_BYTES_PER_MHZ * PSRAM_USABLE_FRACTION
    width, height = resolution
    frame_bytes = width * height * PIXEL_FORMAT_BYTES[config[CONF_PIXEL_FORMAT]]
    # One pass to write the frame and one to read it back for encoding.
    required = frame_bytes * int(config[CONF_FRAMERATE]) * 2 / 1e6
    if required <= available:
        return

    faster = [s for s in SPIRAM_SPEEDS[VARIANT_ESP32P4] if s > speed_mhz]
    advice = (
        f" Set 'speed: {faster[-1]}MHZ' on the psram component, or lower the resolution or"
        " framerate."
        if faster
        else " Lower the resolution or framerate."
    )
    _LOGGER.warning(
        "PSRAM at %d MHz provides roughly %.0f MB/s, but %dx%d at %d fps needs about "
        "%.0f MB/s. Expect corrupted frames.%s",
        speed_mhz,
        available,
        width,
        height,
        int(config[CONF_FRAMERATE]),
        required,
        advice,
    )


def _resolve_ldo(config: ConfigType) -> None:
    """Decide whether esp_video should power the D-PHY rail itself.

    On some ESP32-P4 boards the same LDO channel already feeds another peripheral, typically a
    MIPI-DSI display, and it is then brought up by an `esp_ldo` entry in the configuration. A second
    acquisition of that channel fails whenever the first one asked for a different voltage or for an
    adjustable channel, which would stop the camera from starting. Detecting that here means the
    common case needs no extra configuration, while `init_ldo` remains available for boards that
    power the rail in some other way.
    """
    if CONF_INIT_LDO in config:
        return

    claimed = [
        ldo
        for ldo in fv.full_config.get().get(ESP_LDO_DOMAIN, [])
        if ldo[CONF_CHANNEL] == MIPI_DPHY_LDO_CHANNEL
    ]
    config[CONF_INIT_LDO] = not claimed
    if not claimed:
        return

    _LOGGER.info(
        "LDO channel %d is already set up elsewhere in this configuration, so the camera will use "
        "it as it is rather than claiming it again. Set '%s: true' to override.",
        MIPI_DPHY_LDO_CHANNEL,
        CONF_INIT_LDO,
    )
    voltage = claimed[0][CONF_VOLTAGE]
    if voltage != MIPI_DPHY_LDO_VOLTAGE:
        _LOGGER.warning(
            "LDO channel %d is set to %sV, but the MIPI D-PHY expects %sV. The camera may not "
            "work.",
            MIPI_DPHY_LDO_CHANNEL,
            voltage,
            MIPI_DPHY_LDO_VOLTAGE,
        )


def final_validate(config: ConfigType) -> ConfigType:
    """Runs every check that needs to see the whole configuration.

    `_resolve_ldo` fills in `init_ldo` when it was left out, which works because the validator is
    handed the live configuration object rather than a copy of it.
    """
    _check_psram_bandwidth(config)
    _resolve_ldo(config)
    return config
