import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import TimePeriod
from esphome.types import ConfigType

AUTO_LOAD = ["remote_base"]
CODEOWNERS = ["@Bofr5555"]
DEPENDENCIES = ["remote_receiver"]
DOMAIN = "vevor_dtx"
MULTI_CONF = True

CONF_BIT_TIME = "bit_time"
CONF_STATION_ID = "station_id"
CONF_VEVOR_DTX_ID = "vevor_dtx_id"

vevor_dtx_ns = cg.esphome_ns.namespace("vevor_dtx")
VevorDtxComponent = vevor_dtx_ns.class_(
    "VevorDtxComponent", cg.Component, remote_base.RemoteReceiverListener
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VevorDtxComponent),
            cv.Optional(CONF_BIT_TIME, default="88us"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(
                    min=TimePeriod(microseconds=50),
                    max=TimePeriod(microseconds=150),
                ),
            ),
            cv.Optional(CONF_STATION_ID): cv.hex_uint16_t,
        }
    )
    .extend(remote_base.REMOTE_LISTENER_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_listener(var, config)
    cg.add(var.set_bit_time_us(config[CONF_BIT_TIME]))
    if (station_id := config.get(CONF_STATION_ID)) is not None:
        cg.add(var.set_station_id(station_id))
