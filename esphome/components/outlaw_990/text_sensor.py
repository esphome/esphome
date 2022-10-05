import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import CONF_OUTLAW_990_ID, Outlaw990

DEPENDENCIES = ["outlaw_990"]

CONF_SENSOR_DISPLAY = "sensor_display"
CONF_SENSOR_AUDIO_IN = "sensor_audio_in"
CONF_SENSOR_VIDEO_IN = "sensor_video_in"

TYPES = [
    CONF_SENSOR_DISPLAY,
    CONF_SENSOR_AUDIO_IN,
    CONF_SENSOR_VIDEO_IN
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OUTLAW_990_ID): cv.use_id(Outlaw990),

        cv.Required(CONF_SENSOR_DISPLAY):
            text_sensor.text_sensor_schema(),
        cv.Required(CONF_SENSOR_AUDIO_IN):
            text_sensor.text_sensor_schema(),
        cv.Required(CONF_SENSOR_VIDEO_IN):
            text_sensor.text_sensor_schema()
    }
)

async def to_code(config):
    comp = await cg.get_variable(config[CONF_OUTLAW_990_ID])

    if CONF_SENSOR_DISPLAY in config:
        conf = config[CONF_SENSOR_DISPLAY]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(comp.set_display_text_sensor(sens))

    if CONF_SENSOR_AUDIO_IN in config:
        conf = config[CONF_SENSOR_AUDIO_IN]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(comp.set_audio_in_text_sensor(sens))

    if CONF_SENSOR_VIDEO_IN in config:
        conf = config[CONF_SENSOR_VIDEO_IN]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(comp.set_video_in_text_sensor(sens))
