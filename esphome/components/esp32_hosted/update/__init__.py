import hashlib
from typing import Any

import esphome.codegen as cg
from esphome.components import esp32, update
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PATH, CONF_SOURCE, CONF_TYPE
from esphome.core import CORE, ID, HexInt

CODEOWNERS = ["@swoboda1337"]
AUTO_LOAD = ["sha256", "watchdog", "json"]
DEPENDENCIES = ["esp32_hosted"]

CONF_SHA256 = "sha256"
CONF_HTTP_REQUEST_ID = "http_request_id"

TYPE_EMBEDDED = "embedded"
TYPE_HTTP = "http"

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


BASE_SCHEMA = update.update_schema(Esp32HostedUpdate, device_class="firmware").extend(
    cv.polling_component_schema("6h")
)

EMBEDDED_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_PATH): cv.file_,
        cv.Required(CONF_SHA256): _validate_sha256,
    }
)

HTTP_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.Required(CONF_SOURCE): cv.url,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            TYPE_EMBEDDED: EMBEDDED_SCHEMA,
            TYPE_HTTP: HTTP_SCHEMA,
        }
    ),
    esp32.only_on_variant(
        supported=[
            esp32.VARIANT_ESP32H2,
            esp32.VARIANT_ESP32P4,
        ]
    ),
)


def _validate_firmware(config: dict[str, Any]) -> None:
    if config[CONF_TYPE] != TYPE_EMBEDDED:
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

    if config[CONF_TYPE] == TYPE_EMBEDDED:
        path = config[CONF_PATH]
        with open(CORE.relative_config_path(path), "rb") as f:
            firmware_data = f.read()
        rhs = [HexInt(x) for x in firmware_data]
        arr_id = ID(f"{config[CONF_ID]}_data", is_declaration=True, type=cg.uint8)
        prog_arr = cg.progmem_array(arr_id, rhs)

        sha256_bytes = bytes.fromhex(config[CONF_SHA256])
        cg.add(var.set_firmware_sha256([HexInt(b) for b in sha256_bytes]))
        cg.add(var.set_firmware_data(prog_arr))
        cg.add(var.set_firmware_size(len(firmware_data)))
    else:
        http_request_var = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
        cg.add(var.set_http_request_parent(http_request_var))
        cg.add(var.set_source_url(config[CONF_SOURCE]))
        cg.add_define("USE_ESP32_HOSTED_HTTP_UPDATE")

    await cg.register_component(var, config)
