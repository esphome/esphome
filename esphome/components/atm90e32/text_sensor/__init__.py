import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C

from ..sensor import ATM90E32Component

CONF_PHASE_STATUS = "phase_status"
CONF_FREQUENCY_STATUS = "frequency_status"
PHASE_KEYS = [CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]

PHASE_STATUS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PHASE_A): text_sensor.text_sensor_schema(
            icon="mdi:flash-alert"
        ),
        cv.Optional(CONF_PHASE_B): text_sensor.text_sensor_schema(
            icon="mdi:flash-alert"
        ),
        cv.Optional(CONF_PHASE_C): text_sensor.text_sensor_schema(
            icon="mdi:flash-alert"
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ATM90E32Component),
        cv.Optional(CONF_PHASE_STATUS): PHASE_STATUS_SCHEMA,
        cv.Optional(CONF_FREQUENCY_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:lightbulb-alert"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if phase_cfg := config.get(CONF_PHASE_STATUS):
        for i, key in enumerate(PHASE_KEYS):
            if sub_phase_cfg := phase_cfg.get(key):
                sens = await text_sensor.new_text_sensor(sub_phase_cfg)
                cg.add(parent.set_phase_status_text_sensor(i, sens))

    if freq_status_config := config.get(CONF_FREQUENCY_STATUS):
        sens = await text_sensor.new_text_sensor(freq_status_config)
        cg.add(parent.set_freq_status_text_sensor(sens))
