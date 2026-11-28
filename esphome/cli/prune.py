"""``esphome prune``."""

from __future__ import annotations

import argparse
import logging

_LOGGER = logging.getLogger(__name__)


def _recommended_idf_versions() -> set[str]:
    """The ESP-IDF versions the recommended ESP32 frameworks build with."""
    from esphome.components.esp32 import (
        ARDUINO_FRAMEWORK_VERSION_LOOKUP,
        ARDUINO_IDF_VERSION_LOOKUP,
        ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
    )

    arduino = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]
    return {
        str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]),
        str(ARDUINO_IDF_VERSION_LOOKUP[arduino]),
    }


def _recommended_arduino8266_frameworks() -> set[str]:
    """The release tag of the recommended ESP8266 Arduino core."""
    from esphome.arduino8266.framework import framework_release
    from esphome.components.esp8266 import RECOMMENDED_ARDUINO_FRAMEWORK_VERSION

    return {framework_release(RECOMMENDED_ARDUINO_FRAMEWORK_VERSION).tag}


def _recommended_sdk_nrf_versions() -> set[str]:
    """Install names of the nRF Connect SDK versions used when none is set.

    Some SoCs need a newer SDK than the recommended one and default to that.
    """
    from esphome.components.nrf52 import (
        MIN_SDK_NRF_VERSIONS,
        RECOMMENDED_SDK_NRF_VERSION,
    )
    from esphome.core import Version

    recommended = Version.parse(RECOMMENDED_SDK_NRF_VERSION)
    versions = {recommended} | {
        max(recommended, Version.parse(minimum))
        for minimum in MIN_SDK_NRF_VERSIONS.values()
    }
    return {f"v{ver.major}.{ver.minor}.{ver.patch}" for ver in versions}


def _recommended_platformio_sources() -> dict[str, str]:
    """Where the recommended version of each PlatformIO platform comes from."""
    from esphome.components.libretiny import ARDUINO_VERSIONS
    from esphome.components.rp2 import (
        RECOMMENDED_ARDUINO_PLATFORM_VERSION,
        _parse_platform_version,
    )
    from esphome.platformio.prune import LIBRETINY, RASPBERRYPI

    return {
        RASPBERRYPI: _parse_platform_version(RECOMMENDED_ARDUINO_PLATFORM_VERSION),
        LIBRETINY: ARDUINO_VERSIONS["recommended"][1],
    }


def command_prune(args: argparse.Namespace) -> int | None:
    from esphome.arduino8266.prune import prune_arduino8266_installs
    from esphome.components.nrf52.prune import prune_sdk_nrf_installs
    from esphome.espidf.prune import prune_idf_installs
    from esphome.platformio.prune import DEPRECATED_PLATFORMS, prune_platformio_installs

    try:
        removed = prune_idf_installs(_recommended_idf_versions(), dry_run=args.dry_run)
        removed += prune_arduino8266_installs(
            _recommended_arduino8266_frameworks(), dry_run=args.dry_run
        )
        removed += prune_sdk_nrf_installs(
            _recommended_sdk_nrf_versions(), dry_run=args.dry_run
        )
        removed += prune_platformio_installs(
            DEPRECATED_PLATFORMS,
            _recommended_platformio_sources(),
            dry_run=args.dry_run,
        )
    except OSError as err:
        _LOGGER.error("Error pruning installs: %s", err)
        return 1

    if not removed:
        _LOGGER.info("Nothing to prune")
    elif not args.dry_run:
        _LOGGER.info("Done!")
    return 0
