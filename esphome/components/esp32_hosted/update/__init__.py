import hashlib
from typing import Any

import esphome.codegen as cg
from esphome.components import esp32, update
import esphome.config_validation as cv
from esphome.const import CONF_PATH, CONF_RAW_DATA_ID, CONF_SOURCE
from esphome.core import CORE, HexInt

CODEOWNERS = ["@swoboda1337"]
AUTO_LOAD = ["sha256", "watchdog", "json"]
DEPENDENCIES = ["esp32_hosted"]

CONF_SHA256 = "sha256"
CONF_HTTP_REQUEST_ID = "http_request_id"

esp32_hosted_ns = cg.esphome_ns.namespace("esp32_hosted")
http_request_ns = cg.esphome_ns.namespace("http_request")
HttpRequestComponent = http_request_ns.class_("HttpRequestComponent", cg.Component)
Esp32HostedUpdate = esp32_hosted_ns.class_(
    "Esp32HostedUpdate", update.UpdateEntity, cg.PollingComponent
)


def _validate_sha256(value: Any) -> str:
    value = cv.string_strict(value)
    if len(value) != 64:
        raise cv.Invalid("SHA256 must be 64 hexadecimal characters")
    try:
        bytes.fromhex(value)
    except ValueError as e:
        raise cv.Invalid(f"SHA256 must be valid hexadecimal: {e}") from e
    return value


def _validate_config(config: dict[str, Any]) -> dict[str, Any]:
    """Validate mutual exclusion of embedded mode (path) vs HTTP mode (source)."""
    has_path = CONF_PATH in config
    has_source = CONF_SOURCE in config

    if has_path and has_source:
        raise cv.Invalid(
            f"Cannot specify both '{CONF_PATH}' (embedded mode) and '{CONF_SOURCE}' (HTTP mode)"
        )
    if not has_path and not has_source:
        raise cv.Invalid(
            f"Must specify either '{CONF_PATH}' (embedded mode) or '{CONF_SOURCE}' (HTTP mode)"
        )
    if has_path and CONF_SHA256 not in config:
        raise cv.Invalid(f"'{CONF_SHA256}' is required when using '{CONF_PATH}'")

    return config


CONFIG_SCHEMA = cv.All(
    update.update_schema(Esp32HostedUpdate, device_class="firmware")
    .extend(
        {
            # Embedded mode (existing)
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_PATH): cv.file_,
            cv.Optional(CONF_SHA256): _validate_sha256,
            # HTTP mode (new)
            cv.Optional(CONF_SOURCE): cv.url,
            cv.Optional(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        }
    )
    .extend(cv.polling_component_schema("6h")),
    esp32.only_on_variant(
        supported=[
            esp32.VARIANT_ESP32H2,
            esp32.VARIANT_ESP32P4,
        ]
    ),
    _validate_config,
)


def _validate_firmware(config: dict[str, Any]) -> None:
    # Only validate firmware for embedded mode
    if CONF_PATH not in config:
        return

    path = CORE.relative_config_path(config[CONF_PATH])
    with open(path, "rb") as f:
        firmware_data = f.read()
    calculated = hashlib.sha256(firmware_data).hexdigest()
    expected = config[CONF_SHA256].lower()
    if calculated != expected:
        raise cv.Invalid(
            f"SHA256 mismatch for {config[CONF_PATH]}: expected {expected}, got {calculated}"
        )


FINAL_VALIDATE_SCHEMA = _validate_firmware


async def to_code(config: dict[str, Any]) -> None:
    var = await update.new_update(config)

    if CONF_PATH in config:
        # Embedded mode: firmware embedded in binary
        path = config[CONF_PATH]
        with open(CORE.relative_config_path(path), "rb") as f:
            firmware_data = f.read()
        rhs = [HexInt(x) for x in firmware_data]
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

        sha256_bytes = bytes.fromhex(config[CONF_SHA256])
        cg.add(var.set_firmware_sha256([HexInt(b) for b in sha256_bytes]))
        cg.add(var.set_firmware_data(prog_arr))
        cg.add(var.set_firmware_size(len(firmware_data)))
    else:
        # HTTP mode: firmware fetched from URL
        cg.add(var.set_source_url(config[CONF_SOURCE]))

        # Get http_request component - either from config or auto-find
        if CONF_HTTP_REQUEST_ID in config:
            http_request_var = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
        else:
            http_request_var = await cg.get_variable(
                CORE.config["http_request"][cv.CONF_ID]
            )
        cg.add(var.set_http_request_parent(http_request_var))

        cg.add_define("USE_ESP32_HOSTED_HTTP_UPDATE")

    await cg.register_component(var, config)
