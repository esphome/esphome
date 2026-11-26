"""wsniff: promiscuous counter of the frames one transmitter sends, per destination and frame type (esphome#23010).

Brings up Wi-Fi itself in station mode (so the radio ACKs frames sent to its own MAC) without joining a network; do
not add `wifi:`. With channel 0 it hops until it sees the target, then locks; every 5 s it logs one SNIF line.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["esp32"]
ns = cg.esphome_ns.namespace("wsniff")
WSniff = ns.class_("WSniff", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WSniff),
        cv.Required("target"): cv.mac_address,
        cv.Optional("channel", default=0): cv.int_range(0, 13),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    esp32.request_wifi()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_target(config["target"].as_hex))
    cg.add(var.set_channel(config["channel"]))
