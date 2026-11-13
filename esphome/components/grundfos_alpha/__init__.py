import esphome.codegen as cg
from esphome.components import genibus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

AUTO_LOAD = ["genibus"]
CODEOWNERS = ["@tomwellnitz"]
DOMAIN = "grundfos_alpha"
MULTI_CONF = True

CONF_GRUNDFOS_ALPHA_ID = "grundfos_alpha_id"

grundfos_alpha_ns = cg.esphome_ns.namespace("grundfos_alpha")
GrundfosAlpha = grundfos_alpha_ns.class_("GrundfosAlpha", genibus.GenibusDevice)

# A poll is one request per object read, and a request without an answer takes the bus's 1 s timeout and 250 ms
# quiet period. An update is skipped while the last poll still runs, so a shorter interval only keeps the pump's
# Bluetooth link busy.
MIN_UPDATE_INTERVAL = cv.TimePeriod(seconds=5)
validate_update_interval = cv.All(cv.update_interval, cv.Range(min=MIN_UPDATE_INTERVAL))

# 0xE7 is the address the ALPHA pumps answer to over Bluetooth
CONFIG_SCHEMA = (
    genibus.genibus_device_schema(0xE7)
    .extend({cv.GenerateID(): cv.declare_id(GrundfosAlpha)})
    .extend(cv.polling_component_schema("30s"))
    .extend(
        {
            cv.Optional(CONF_UPDATE_INTERVAL, default="30s"): validate_update_interval,
        }
    )
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await genibus.register_genibus_device(var, config)
