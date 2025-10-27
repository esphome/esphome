import esphome.codegen as cg
from esphome.components import update
import esphome.config_validation as cv
from esphome.const import CONF_PATH, CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["esp32_hosted"]

esp32_hosted_ns = cg.esphome_ns.namespace("esp32_hosted")
Esp32HostedUpdate = esp32_hosted_ns.class_(
    "Esp32HostedUpdate", update.UpdateEntity, cg.Component
)

CONFIG_SCHEMA = (
    update.update_schema(Esp32HostedUpdate, device_class="firmware")
    .extend(
        {
            cv.Required(CONF_PATH): cv.file_,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
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
