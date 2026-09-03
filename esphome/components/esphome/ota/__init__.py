import logging

import esphome.codegen as cg
from esphome.components.noise import (
    decode_encryption_key,
    encryption_schema,
    is_reserved_key,
)
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEY,
    CONF_NUM_ATTEMPTS,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_VERSION,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

CONF_ALLOW_PARTITION_ACCESS = "allow_partition_access"
CONF_CAPTIVE_PORTAL = "captive_portal"

_LOGGER = logging.getLogger(__name__)


CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def AUTO_LOAD(config: ConfigType) -> list[str]:
    """Auto-load noise only when encryption is configured."""
    base = ["sha256", "socket"]
    # A falsy config is a tooling probe for the maximal set (None from
    # dependency resolution, {} from the components-graph platform probe);
    # a validated config always carries defaults, never empty
    if not config or CONF_ENCRYPTION in config:
        return base + ["noise"]
    return base


esphome = cg.esphome_ns.namespace("esphome")
ESPHomeOTAComponent = esphome.class_("ESPHomeOTAComponent", OTAComponent)


def ota_esphome_final_validate(config: ConfigType) -> None:
    full_conf = fv.full_config.get()
    full_ota_conf = full_conf[CONF_OTA]
    new_ota_conf = []
    merged_ota_esphome_configs_by_port = {}
    ports_with_merged_configs = []
    for ota_conf in full_ota_conf:
        if ota_conf.get(CONF_PLATFORM) == CONF_ESPHOME:
            if (
                conf_port := ota_conf.get(CONF_PORT)
            ) not in merged_ota_esphome_configs_by_port:
                merged_ota_esphome_configs_by_port[conf_port] = ota_conf
            else:
                if merged_ota_esphome_configs_by_port[conf_port][
                    CONF_VERSION
                ] != ota_conf.get(CONF_VERSION):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_VERSION} is inconsistent"
                    )
                if (
                    merged_ota_esphome_configs_by_port[conf_port][CONF_ID].is_manual
                    and ota_conf.get(CONF_ID).is_manual
                ):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_ID} is inconsistent"
                    )
                if (
                    CONF_PASSWORD in merged_ota_esphome_configs_by_port[conf_port]
                    and CONF_PASSWORD in ota_conf
                    and merged_ota_esphome_configs_by_port[conf_port][CONF_PASSWORD]
                    != ota_conf[CONF_PASSWORD]
                ):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_PASSWORD} is inconsistent"
                    )
                # Encryption blocks conflict only when both pin a key; a bare
                # `encryption:` (a package/device split) is compatible with a
                # keyed one, and merge_config yields the keyed result
                merged_key = (
                    merged_ota_esphome_configs_by_port[conf_port]
                    .get(CONF_ENCRYPTION, {})
                    .get(CONF_KEY)
                )
                other_key = ota_conf.get(CONF_ENCRYPTION, {}).get(CONF_KEY)
                if merged_key and other_key and merged_key != other_key:
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_ENCRYPTION} is inconsistent"
                    )

                ports_with_merged_configs.append(conf_port)
                merged_ota_esphome_configs_by_port[conf_port] = merge_config(
                    merged_ota_esphome_configs_by_port[conf_port], ota_conf
                )
            if ota_conf.get(CONF_ALLOW_PARTITION_ACCESS) and not CORE.is_esp32:
                raise cv.Invalid(
                    f"{CONF_ALLOW_PARTITION_ACCESS} is only supported on the esp32"
                )
        else:
            new_ota_conf.append(ota_conf)

    if len(merged_ota_esphome_configs_by_port) > 1:
        raise cv.Invalid(
            f"Only a single port is supported for '{CONF_OTA}' "
            f"'{CONF_PLATFORM}: {CONF_ESPHOME}'. Got ports "
            f"{sorted(merged_ota_esphome_configs_by_port.keys())}. Consolidate "
            f"onto a single port; configs sharing a port are merged automatically."
        )

    new_ota_conf.extend(merged_ota_esphome_configs_by_port.values())

    api_conf = full_conf.get(CONF_API) or {}
    for ota_conf in merged_ota_esphome_configs_by_port.values():
        # Merging same-port blocks can combine a password from one block with
        # encryption from another; re-check the exclusion on the merged result.
        _validate_no_password_with_encryption(ota_conf)
        if (encryption_conf := ota_conf.get(CONF_ENCRYPTION)) is not None:
            _resolve_encryption_key(encryption_conf, api_conf)
    if any(
        conf.get(CONF_PLATFORM) == CONF_WEB_SERVER for conf in full_ota_conf
    ) and any(
        CONF_ENCRYPTION in conf for conf in merged_ota_esphome_configs_by_port.values()
    ):
        _warn_web_server_ota(full_conf)

    full_conf[CONF_OTA] = new_ota_conf
    fv.full_config.set(full_conf)

    if len(ports_with_merged_configs) > 0:
        _LOGGER.warning(
            "Found and merged multiple configurations for %s %s %s port(s) %s",
            CONF_OTA,
            CONF_PLATFORM,
            CONF_ESPHOME,
            ports_with_merged_configs,
        )


def _warn_web_server_ota(full_conf: ConfigType) -> None:
    """The web_server ota platform accepts the same image over plaintext HTTP
    with basic auth, bypassing the encryption; warn rather than fail so the
    operator keeps the recovery path."""
    if CONF_CAPTIVE_PORTAL in full_conf and CONF_WEB_SERVER not in full_conf:
        # The captive_portal auto-load: the endpoint only exists while the
        # fallback AP is active
        _LOGGER.warning(
            "OTA encryption does not cover the %s OTA platform (auto-loaded "
            "by captive_portal); the plaintext /update endpoint stays "
            "reachable while the fallback AP is active",
            CONF_WEB_SERVER,
        )
    else:
        _LOGGER.warning(
            "OTA encryption does not cover the %s OTA platform; its "
            "plaintext /update endpoint accepts the same image",
            CONF_WEB_SERVER,
        )


def _resolve_encryption_key(encryption_conf: ConfigType, api_conf: ConfigType) -> None:
    """Resolve the one encryption key per device into the ota block.

    An explicit ota key must match the api key, a bare block inherits it,
    a runtime provisioned api key cannot be inherited, and the all-zeros
    provisioning sentinel is rejected (the device treats it as no key).
    """
    api_key = api_conf.get(CONF_ENCRYPTION, {}).get(CONF_KEY)
    if ota_key := encryption_conf.get(CONF_KEY):
        if api_key and ota_key != api_key:
            raise cv.Invalid(
                f"'{CONF_OTA}' {CONF_ENCRYPTION} {CONF_KEY} must match the "
                f"'{CONF_API}' {CONF_ENCRYPTION} {CONF_KEY}; omit the "
                f"'{CONF_OTA}' {CONF_KEY} to use the '{CONF_API}' one"
            )
    elif not api_key:
        if CONF_ENCRYPTION in api_conf:
            raise cv.Invalid(
                f"the '{CONF_API}' {CONF_ENCRYPTION} {CONF_KEY} is provisioned at "
                f"runtime and cannot be inherited at build time; set an explicit "
                f"'{CONF_OTA}' {CONF_ENCRYPTION} {CONF_KEY}"
            )
        raise cv.Invalid(
            f"'{CONF_OTA}' {CONF_ENCRYPTION} has no {CONF_KEY} and there is no "
            f"'{CONF_API}' {CONF_ENCRYPTION} {CONF_KEY} to inherit; set one of them"
        )
    else:
        encryption_conf[CONF_KEY] = api_key
    if is_reserved_key(encryption_conf[CONF_KEY]):
        raise cv.Invalid(
            f"The all-zeros {CONF_KEY} is reserved and provides no protection; "
            f"generate a real key with: openssl rand -base64 32"
        )


# Also called on merged same-port configs in final validate, where schemas
# do not run
def _validate_no_password_with_encryption(config: ConfigType) -> ConfigType:
    if CONF_PASSWORD in config and CONF_ENCRYPTION in config:
        raise cv.Invalid(
            f"'{CONF_PASSWORD}' cannot be combined with '{CONF_ENCRYPTION}'; the "
            f"encryption key already authenticates the uploader, remove '{CONF_PASSWORD}'"
        )
    return config


def _consume_ota_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for OTA component."""
    from esphome.components import socket

    # OTA needs 1 listening socket. The active transfer connection during an update
    # uses a TCP PCB from the general pool, covered by MIN_TCP_SOCKETS headroom.
    socket.consume_sockets(1, "ota", socket.SocketType.TCP_LISTEN)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPHomeOTAComponent),
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, int=True),
            cv.SplitDefault(
                CONF_PORT,
                esp8266=8266,
                esp32=3232,
                rp2=2040,
                bk72xx=8892,
                ln882x=8820,
                rtl87xx=8892,
                host=8082,
            ): cv.port,
            cv.Optional(CONF_ALLOW_PARTITION_ACCESS, default=False): cv.boolean,
            cv.Optional(CONF_PASSWORD): cv.sensitive(),
            cv.Optional(CONF_ENCRYPTION): encryption_schema,
            cv.Optional(CONF_NUM_ATTEMPTS): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_REBOOT_TIMEOUT): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_SAFE_MODE): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _validate_no_password_with_encryption,
    _consume_ota_sockets,
)

FINAL_VALIDATE_SCHEMA = ota_esphome_final_validate


def FILTER_SOURCE_FILES() -> list[str]:
    """Filter out the noise transport when no ota entry configures encryption."""
    for ota_conf in CORE.config.get(CONF_OTA, []):
        if (
            ota_conf.get(CONF_PLATFORM) == CONF_ESPHOME
            and ota_conf.get(CONF_ENCRYPTION) is not None
        ):
            return []
    return ["ota_esphome_noise.cpp"]


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_port(config[CONF_PORT]))

    # Compile the auth path whenever `password:` is present in YAML, even if empty.
    # An empty password opts in to the auth code path so set_auth_password() can be
    # called at runtime (e.g. to rotate the password from a lambda). When `password:`
    # is omitted entirely, the auth path is excluded to save flash on small devices.
    if CONF_PASSWORD in config:
        cg.add_define("USE_OTA_PASSWORD")
        if config[CONF_PASSWORD]:
            cg.add(var.set_auth_password(config[CONF_PASSWORD]))
    cg.add_define("USE_OTA_VERSION", config[CONF_VERSION])

    if config.get(CONF_ALLOW_PARTITION_ACCESS):
        cg.add_define("USE_OTA_PARTITIONS")

    if (encryption_conf := config.get(CONF_ENCRYPTION)) is not None:
        # A missing key was resolved from the api component in final validate.
        key = encryption_conf[CONF_KEY]
        cg.add_define("USE_OTA_ENCRYPTION")
        cg.add(var.set_noise_psk(list(decode_encryption_key(key))))

    # Build flag so lwip_fast_select.c (a .c file that can't include defines.h) sees it.
    cg.add_build_flag("-DUSE_OTA_PLATFORM_ESPHOME")

    await cg.register_component(var, config)
    await ota_to_code(var, config)
