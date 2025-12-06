import hashlib
from typing import Any

import esphome.codegen as cg
from esphome.components import esp32, update
import esphome.config_validation as cv
from esphome.const import CONF_PATH, CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt

CODEOWNERS = ["@swoboda1337"]
AUTO_LOAD = ["sha256", "watchdog"]
DEPENDENCIES = ["esp32_hosted"]

CONF_SHA256 = "sha256"

esp32_hosted_ns = cg.esphome_ns.namespace("esp32_hosted")
Esp32HostedUpdate = esp32_hosted_ns.class_(
    "Esp32HostedUpdate", update.UpdateEntity, cg.Component
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


CONFIG_SCHEMA = cv.All(
    update.update_schema(Esp32HostedUpdate, device_class="firmware").extend(
        {
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Required(CONF_PATH): cv.file_,
            cv.Required(CONF_SHA256): _validate_sha256,
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

    path = config[CONF_PATH]
    with open(CORE.relative_config_path(path), "rb") as f:
        firmware_data = f.read()
    rhs = [HexInt(x) for x in firmware_data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    sha256_bytes = bytes.fromhex(config[CONF_SHA256])
    cg.add(var.set_firmware_sha256([HexInt(b) for b in sha256_bytes]))
    cg.add(var.set_firmware_data(prog_arr))
    cg.add(var.set_firmware_size(len(firmware_data)))
    await cg.register_component(var, config)
