"""DSP-408 text_sensor sub-platform.

Currently exposes one read-only entity:
  * model — the device identity string returned by GET_INFO
    (e.g. ``"MYDW-AV1.06"`` on stock DSP-408 firmware v1.06).
"""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_DSP408_ID, CONF_KIND, DSP408

DEPENDENCIES = ["dsp408"]

KIND_MODEL = "MODEL"

KINDS = {KIND_MODEL: "MODEL"}


CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DSP408_ID): cv.use_id(DSP408),
        cv.Required(CONF_KIND): cv.enum(KINDS, upper=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DSP408_ID])
    var = await text_sensor.new_text_sensor(config)
    if config[CONF_KIND] == KIND_MODEL:
        cg.add(parent.set_model_text_sensor(var))
