import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_PORT, CONF_CAMERA
from esphome.components import esp32_camera


CODEOWNERS = ["@crossan007"]
DEPENDENCIES = ["network", "esp32_camera"]
AUTO_LOAD = ["async_tcp"]

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RTSPServer = rtsp_server_ns.class_("RTSPServer", cg.Component)

CONF_RTSP_SERVER_ID = "rtsp_server_id"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RTSPServer),
        cv.Optional(CONF_PORT, default=554): cv.port,
        cv.GenerateID(CONF_CAMERA): cv.use_id(esp32_camera.ESP32Camera),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    camera = await cg.get_variable(config["camera"])

    cg.add_define("USE_ESPASYNCRTSP")
    cg.add_library("crossan007/ESPAsyncRTSPServer-esphome", "0.1.3")
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_camera(camera))
