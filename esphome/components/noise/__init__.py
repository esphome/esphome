import base64
import binascii
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENCRYPTION, CONF_KEY
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

# Keep in sync with platformio.ini and esphome/idf_component.yml.
# LIBSODIUM_VERSION must match the version noise-c pins in its manifests.
NOISE_C_VERSION = "0.1.30"
LIBSODIUM_VERSION = "1.10021.11"

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
    if not any(decoded):
        # The device treats the all-zeros key as no key at all (it is the
        # provisioning sentinel), so it must never reach a build
        raise cv.Invalid(
            f"The all-zeros {CONF_KEY} is reserved and provides no protection; "
            f"omit the {CONF_KEY} to provision it at runtime, or generate a real "
            "key with: openssl rand -base64 32"
        )

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


def static_encryption_key(conf: ConfigType) -> str | None:
    """The build time key of a component config; None without one or when
    the key is provisioned at runtime."""
    return (conf.get(CONF_ENCRYPTION) or {}).get(CONF_KEY) or None


def new_psk_progmem(key: str) -> MockObj:
    """Emit the decoded key as a PROGMEM array; the component keeps a pointer
    so the key never occupies RAM. Components sharing one key (api and ota)
    share the array."""
    return cg.shared_progmem_array(
        "noise_psk", cg.uint8, list(decode_encryption_key(key))
    )


def encryption_schema(config: ConfigType | None) -> ConfigType:
    # A bare `encryption:` block is valid; a missing key means the consumer
    # falls back to its keyless behavior (api provisioning, ota inheriting
    # the api key).
    if config is None:
        config = {}
    return ENCRYPTION_SCHEMA(config)


def _use_managed_components() -> bool:
    """Whether noise-c and libsodium come from the ESP-IDF component registry.

    Both build as ESP-IDF components, so on ESP32 they skip the PlatformIO library
    converter unless arduino-esp32 bundles its own libsodium. Not toolchain
    dependent: the PlatformIO toolchain reads the project manifest too, and every
    consumer of libsodium must make the same choice or a second copy appears.
    """
    if not CORE.is_esp32:
        return False

    from esphome.components.esp32 import arduino_bundles_libsodium

    return not arduino_bundles_libsodium()


def enable_spare_ephemeral() -> None:
    """Compile the spare ephemeral key slot; the component that refills it calls this."""
    cg.add_define("USE_NOISE_SPARE_EPHEMERAL")


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_NOISE")
    # libsodium is declared next to noise-c so the library manager sees both up front
    # and nothing else pulls a second copy; the version must match noise-c's own pin
    if _use_managed_components():
        from esphome.components.esp32 import add_idf_component

        add_idf_component(name="esphome/noise-c", ref=NOISE_C_VERSION)
        add_idf_component(name="esphome/libsodium", ref=LIBSODIUM_VERSION)
    else:
        cg.add_library("esphome/noise-c", NOISE_C_VERSION)
        cg.add_library("esphome/libsodium", LIBSODIUM_VERSION)
    # Enable optimized memzero/memcmp in libsodium instead of volatile byte loops
    cg.add_build_flag("-DHAVE_WEAK_SYMBOLS=1")
    cg.add_build_flag("-DHAVE_INLINE_ASM=1")
