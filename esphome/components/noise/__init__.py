import base64
import binascii
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_KEY
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

noise_ns = cg.esphome_ns.namespace("noise")

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
    cg.add_library("esphome/noise-c", "0.1.21")
    # noise-c depends on libsodium, but declaring it here too lets the
    # library manager see the full set up front instead of discovering
    # libsodium only after noise-c has downloaded, so the two can download
    # in parallel. The version must match noise-c's library.json.
    cg.add_library("esphome/libsodium", "1.10021.4")
    # Enable optimized memzero/memcmp in libsodium instead of volatile byte loops
    cg.add_build_flag("-DHAVE_WEAK_SYMBOLS=1")
    cg.add_build_flag("-DHAVE_INLINE_ASM=1")
