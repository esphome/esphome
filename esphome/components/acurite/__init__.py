import esphome.codegen as cg
from esphome.components import remote_receiver
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["remote_receiver"]

CONF_RECEIVER_ID = "receiver_id"

acurite_ns = cg.esphome_ns.namespace("acurite")
AcuRiteComponent = acurite_ns.class_("AcuRiteComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AcuRiteComponent),
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(
            remote_receiver.RemoteReceiverComponent
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    cg.add(var.set_srcrecv(receiver))
