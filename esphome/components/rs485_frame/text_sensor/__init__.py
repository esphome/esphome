import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_RS485_FRAME_ID, RS485FrameHub, rs485_frame_ns

AUTO_LOAD = ["rs485_frame"]

RS485FrameTextSensor = rs485_frame_ns.class_(
    "RS485FrameTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

# The text_sensor platform exposes a single diagnostic: the most recent validated frame
# type (first two payload bytes) as a 4-character hex string. User text decoding is done
# via on_frame: + a template text_sensor.
#
# Polled rather than published per frame: on a live bus the last frame type changes on
# nearly every frame (keep-alive, LED and display frames interleave; a master polls
# several devices in turn), so a change filter alone still publishes at the frame rate.
# The 60s default matches the sensor platform's high-rate decodes (frames_received,
# last_keepalive_ms).
CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        RS485FrameTextSensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    cg.add(var.set_parent(hub))
