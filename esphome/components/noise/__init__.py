import base64
import binascii
from collections.abc import Callable
import logging
import secrets
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_RECONNECT_INTERVAL
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEY,
    CONF_ON_BOOT,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_PRIORITY,
)
from esphome.core import CORE, ID
from esphome.core.config import CONF_LOOP_INTERVAL, get_loop_interval
from esphome.cpp_generator import MockObj, MockObjClass
import esphome.final_validate as fv
from esphome.helpers import fnv1_hash
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "noise"

_LOGGER = logging.getLogger(__name__)

# The setup priority at which TcpUart and UartTcp load a saved key, setup_priority::AFTER_WIFI.
# Keep it in step with their get_setup_priority() and the "below 200" in NoiseStream::save_key_().
_KEY_LOAD_PRIORITY = 200

# Keep in sync with platformio.ini and esphome/idf_component.yml.
# LIBSODIUM_VERSION must match the version noise-c pins in its manifests.
NOISE_C_VERSION = "0.1.31"
LIBSODIUM_VERSION = "1.10021.12"

noise_ns = cg.esphome_ns.namespace("noise")
NoiseStream = noise_ns.class_("NoiseStream")

CONFIG_SCHEMA = cv.Schema({})


def generate_encryption_key() -> str:
    """A fresh random key in the base64 form the yaml takes."""
    return base64.b64encode(secrets.token_bytes(32)).decode()


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


def _validate_required_key(value: Any) -> str:
    """validate_encryption_key for a key that cannot be left out."""
    value = cv.string_strict(value)
    try:
        decoded = base64.b64decode(value, validate=True)
    except binascii.Error:
        decoded = None
    if decoded == bytes(32):
        raise cv.Invalid(
            f"The all-zeros {CONF_KEY} is reserved; generate a real key with: "
            "openssl rand -base64 32"
        )
    return validate_encryption_key(value)


def _names_entry(node: object, action: str, entry_id: ID) -> bool:
    """Whether an `action` in the config subtree names entry_id."""
    if isinstance(node, dict):
        return any(
            (
                key == action
                and isinstance(value, dict)
                and value.get(CONF_ID) == entry_id
            )
            or _names_entry(value, action, entry_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_names_entry(item, action, entry_id) for item in node)
    return False


def final_validate_stream_key(owner: str) -> Callable[[ConfigType], ConfigType]:
    """Reject a stream key that equals this device's api or ota key: the peer
    device holds it. `<owner>.set_encryption_key` needs `encryption:` without a
    key, in on_boot a priority below 200, and warns in an api without encryption."""

    action = f"{owner}.set_encryption_key"

    def validator(config: ConfigType) -> ConfigType:
        full_config = fv.full_config.get()
        key = static_encryption_key(config)
        if (CONF_ENCRYPTION not in config or key is not None) and _names_entry(
            full_config, action, config[CONF_ID]
        ):
            raise cv.Invalid(
                f"'{action}' needs '{CONF_ENCRYPTION}:' without a '{CONF_KEY}' "
                f"on '{config[CONF_ID]}'"
            )
        for boot in full_config.get(CONF_ESPHOME, {}).get(CONF_ON_BOOT, []):
            if boot[CONF_PRIORITY] >= _KEY_LOAD_PRIORITY and _names_entry(
                boot, action, config[CONF_ID]
            ):
                raise cv.Invalid(
                    f"'{action}' in '{CONF_ON_BOOT}' needs a '{CONF_PRIORITY}' "
                    f"below {_KEY_LOAD_PRIORITY}"
                )
        if key is None:
            api = full_config.get(CONF_API)
            if (
                api is not None
                and CONF_ENCRYPTION not in api
                and _names_entry(api, action, config[CONF_ID])
            ):
                _LOGGER.warning(
                    "'%s' on '%s' is in an 'api' action, but 'api' has no '%s:'; "
                    "the key crosses the network in plain text",
                    action,
                    config[CONF_ID],
                    CONF_ENCRYPTION,
                )
            return config
        others = [(CONF_API, full_config.get(CONF_API) or {})]
        others += [
            (CONF_OTA, conf)
            for conf in full_config.get(CONF_OTA) or []
            if conf.get(CONF_PLATFORM) == CONF_ESPHOME
        ]
        own = decode_encryption_key(key)
        for name, conf in others:
            other = static_encryption_key(conf)
            if other is not None and decode_encryption_key(other) == own:
                raise cv.Invalid(
                    f"'{owner}' {CONF_ENCRYPTION} {CONF_KEY} must differ from the "
                    f"'{name}' {CONF_ENCRYPTION} {CONF_KEY}; the peer device holds "
                    "this key",
                    path=[CONF_ENCRYPTION, CONF_KEY],
                )
        return config

    return validator


def final_validate_reconnect_interval(config: ConfigType) -> ConfigType:
    """A reconnect_interval under one loop pass voids the wait after a failed
    handshake."""
    if CONF_ENCRYPTION not in config:
        return config
    loop_interval = get_loop_interval()
    if config[CONF_RECONNECT_INTERVAL] < loop_interval:
        raise cv.Invalid(
            f"With {CONF_ENCRYPTION}, {CONF_RECONNECT_INTERVAL} must last at least one "
            f"main loop pass ({CONF_LOOP_INTERVAL} of {loop_interval}). Set it to "
            f"{loop_interval} or more.",
            path=[CONF_RECONNECT_INTERVAL],
        )
    return config


def require_stream() -> None:
    """Compile NoiseStream; call from a consumer's to_code."""
    cg.add_define("USE_NOISE_STREAM")


def stream_auto_load(domain: str, base: list[str]) -> Callable[[], list[str]]:
    """AUTO_LOAD of a stream consumer: base, and noise when an entry has
    `encryption:`. It reads the raw config, because an AUTO_LOAD that takes
    the config runs after the DEPENDENCIES checks."""

    def auto_load() -> list[str]:
        raw = (CORE.raw_config or {}).get(domain)
        confs = raw if isinstance(raw, list) else [raw]
        # Without a config (tooling) the maximal set
        if CORE.raw_config is None or any(
            isinstance(conf, dict) and CONF_ENCRYPTION in conf for conf in confs
        ):
            return [*base, DOMAIN]
        return base

    return auto_load


def new_stream(owner_id: ID, key: str | None, initiator: bool) -> MockObj:
    """Create the NoiseStream of one link. The side that answers takes the
    spare ephemeral key, as the api server does. Without a key the stream gets
    one at runtime from the set_encryption_key action and saves it."""
    require_stream()
    if not initiator:
        enable_spare_ephemeral()
    stream_id = ID(f"{owner_id.id}_noise", is_declaration=True, type=NoiseStream)
    if key is not None:
        return cg.new_Pvariable(stream_id, new_psk_progmem(key), initiator)
    cg.add_define("USE_NOISE_STREAM_RUNTIME_KEY")
    # ':' is not in any object id, so no entity's preference hash comes from this string
    key_hash = fnv1_hash(f"noise_stream_key:{owner_id.id}")
    stream = cg.new_Pvariable(stream_id, key_hash, initiator)
    CORE.add_job(_reserve_device_keys, stream)
    return stream


async def _reserve_device_keys(stream: MockObj) -> None:
    """Give a runtime key stream the keys its key must differ from: the api's,
    also one the api gets at runtime, and the esphome ota key from yaml."""
    api_conf = CORE.config.get(CONF_API) or {}
    api_key = static_encryption_key(api_conf)
    if CONF_ENCRYPTION in api_conf:
        api = await cg.get_variable(api_conf[CONF_ID])
        cg.add(stream.set_api_context(api.get_noise_ctx()))
    # Validation merges the esphome ota entries into one
    for conf in CORE.config.get(CONF_OTA) or []:
        key = static_encryption_key(conf)
        if conf.get(CONF_PLATFORM) == CONF_ESPHOME and key not in (None, api_key):
            cg.add(stream.set_ota_key(new_psk_progmem(key)))


def register_set_key_action(domain: str, owner: MockObjClass) -> None:
    """Register `<domain>.set_encryption_key`; owner provides set_encryption_key().
    final_validate_stream_key() allows it only on an entry without a key in yaml."""
    automation.register_apply_action(
        f"{domain}.set_encryption_key",
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(owner),
                cv.Required(CONF_KEY): cv.sensitive(
                    cv.templatable(_validate_required_key)
                ),
            }
        ),
        automation.ApplyField(CONF_KEY, "set_encryption_key", cg.std_string),
    )


def encryption_schema(config: ConfigType | None) -> ConfigType:
    # A bare `encryption:` block is valid; a missing key means the consumer
    # falls back to its keyless behavior (api provisioning, ota inheriting
    # the api key, a stream key set at runtime).
    if config is None:
        config = {}
    return ENCRYPTION_SCHEMA(config)


# A stream's key may be left out and set at runtime
STREAM_ENCRYPTION_SCHEMA = encryption_schema


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


FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"noise_stream.cpp": "USE_NOISE_STREAM"}
)
