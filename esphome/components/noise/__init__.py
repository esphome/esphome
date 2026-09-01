import base64
import binascii
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_KEY
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

noise_ns = cg.esphome_ns.namespace("noise")

# Keep in sync with platformio.ini and esphome/idf_component.yml.
# LIBSODIUM_VERSION must match the version noise-c pins in its manifests.
NOISE_C_VERSION = "0.1.21"
LIBSODIUM_VERSION = "1.10021.4"

CONFIG_SCHEMA = cv.Schema({})


def validate_encryption_key(value: Any) -> str:
    value = cv.string_strict(value)
    try:
        decoded = base64.b64decode(value, validate=True)
    except ValueError as err:
        raise cv.Invalid("Invalid key format, please check it's using base64") from err

    if len(decoded) != 32:
        raise cv.Invalid("Encryption key must be base64 and 32 bytes long")

    # Return original data for roundtrip conversion
    return value


def decode_encryption_key(value: str) -> bytes:
    """Decode a base64 encryption key to its 32 raw bytes.

    a2b_base64 matches the decode the clients use (aioesphomeapi
    decode_noise_psk), so both ends derive the same bytes. The length is
    re-checked so a caller cannot turn an unvalidated short decode into a
    zero-padded PSK.
    """
    try:
        decoded = binascii.a2b_base64(value)
    except ValueError as err:
        raise cv.Invalid("Invalid key format, please check it's using base64") from err
    if len(decoded) != 32:
        raise cv.Invalid("Encryption key must be base64 and 32 bytes long")
    return decoded


ENCRYPTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_KEY): cv.sensitive(validate_encryption_key),
    }
)


def encryption_schema(config: ConfigType | None) -> ConfigType:
    # A bare `encryption:` block is valid; a missing key means the consumer
    # falls back to its keyless behavior (api provisioning, ota inheriting
    # the api key).
    if config is None:
        config = {}
    return ENCRYPTION_SCHEMA(config)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_NOISE")
    # Both libraries build themselves as ESP-IDF components, so on ESP32 they
    # are pulled straight from the component registry instead of going through
    # ESPHome's PlatformIO-library converter. Deliberately not conditional on
    # the toolchain: wireguard splits on the same condition, and if the two
    # disagree one of them converts a second libsodium next to the managed one.
    #
    # Not on the Arduino framework though: arduino-esp32 depends on
    # espressif/libsodium of its own (on IDF < 6.0), so the component manager
    # would see two managed components whose names match once the namespace is
    # stripped, and refuse to pick between them.
    #
    # libsodium is declared alongside noise-c rather than left to noise-c's own
    # manifest either way: it lets the library manager see the full set up front
    # instead of discovering libsodium only after noise-c has downloaded, and it
    # keeps other components that depend on it (wireguard) from converting a
    # second copy next to the managed one. The version must match the one
    # noise-c pins.
    if CORE.is_esp32 and not CORE.using_arduino:
        from esphome.components.esp32 import add_idf_component

        add_idf_component(name="esphome/noise-c", ref=NOISE_C_VERSION)
        add_idf_component(name="esphome/libsodium", ref=LIBSODIUM_VERSION)
    else:
        cg.add_library("esphome/noise-c", NOISE_C_VERSION)
        cg.add_library("esphome/libsodium", LIBSODIUM_VERSION)
    # Enable optimized memzero/memcmp in libsodium instead of volatile byte loops
    cg.add_build_flag("-DHAVE_WEAK_SYMBOLS=1")
    cg.add_build_flag("-DHAVE_INLINE_ASM=1")
