import esphome.codegen as cg
import esphome.config_validation as cv

dashboard_import_ns = cg.esphome_ns.namespace("dashboard_import")

# payload is in `esphomelib` mdns record, which only exists if api
# is enabled
DEPENDENCIES = ["api"]

CONF_IMPORT_CONFIG = "import_config"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_IMPORT_CONFIG): cv.string_strict,
    }
)


async def to_code(config):
    cg.add_define("USE_DASHBOARD_IMPORT")
    cg.add(dashboard_import_ns.set_dashboard_import_config(config[CONF_IMPORT_CONFIG]))
