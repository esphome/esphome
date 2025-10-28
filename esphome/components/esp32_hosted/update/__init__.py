import esphome.codegen as cg
from esphome.components import esp32, update
import esphome.config_validation as cv
from esphome.const import CONF_PATH, CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt

CODEOWNERS = ["@swoboda1337"]
AUTO_LOAD = ["watchdog"]
DEPENDENCIES = ["esp32_hosted"]

esp32_hosted_ns = cg.esphome_ns.namespace("esp32_hosted")
Esp32HostedUpdate = esp32_hosted_ns.class_(
    "Esp32HostedUpdate", update.UpdateEntity, cg.Component
)

CONFIG_SCHEMA = cv.All(
    update.update_schema(Esp32HostedUpdate, device_class="firmware").extend(
        {
            cv.Required(CONF_PATH): cv.file_,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    ),
    esp32.only_on_variant(
        supported=[
            esp32.const.VARIANT_ESP32H2,
            esp32.const.VARIANT_ESP32P4,
        ]
    ),
)


async def to_code(config):
    var = await update.new_update(config)

    path = config[CONF_PATH]
    with open(CORE.relative_config_path(path), "rb") as f:
        firmware_data = f.read()
    rhs = [HexInt(x) for x in firmware_data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    cg.add(var.set_firmware_data(prog_arr))
    cg.add(var.set_firmware_size(len(firmware_data)))
    await cg.register_component(var, config)
