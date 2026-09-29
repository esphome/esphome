from esphome.components.sensor import Sensor, new_sensor, sensor_schema
import esphome.config_validation as cv

from ..defines import CONF_TRIGGER, CONF_WIDGET
from ..lvcode import EVENT_ARG, LambdaContext, LvContext, lv_add, lvgl_static
from ..schemas import TRIGGER_EVENT_MAP, VALUE_TRIGGER_SCHEMA
from ..types import LvNumber
from ..widgets import Widget, get_widgets, wait_for_widgets

CONFIG_SCHEMA = sensor_schema(Sensor).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
        **VALUE_TRIGGER_SCHEMA,
    }
)


async def to_code(config):
    sensor = await new_sensor(config)
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    assert isinstance(widget, Widget)
    await wait_for_widgets()
    async with LambdaContext(EVENT_ARG) as lamb:
        lv_add(sensor.publish_state(widget.get_value()))
    async with LvContext():
        lv_add(
            lvgl_static.add_event_cb(
                widget.obj,
                await lamb.get_lambda(),
                *TRIGGER_EVENT_MAP[config[CONF_TRIGGER]],
            )
        )
