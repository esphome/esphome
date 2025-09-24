from esphome.components.sensor import Sensor, new_sensor, sensor_schema
import esphome.config_validation as cv

from ..defines import CONF_WIDGET
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    LVGL_COMP_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvContext,
    lv_add,
    lvgl_static,
)
from ..types import LV_EVENT, LvNumber
from ..widgets import Widget, get_widgets, wait_for_widgets

CONFIG_SCHEMA = sensor_schema(Sensor).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
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
    async with LvContext(LVGL_COMP_ARG):
        lv_add(
            lvgl_static.add_event_cb(
                widget.obj,
                await lamb.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
                API_EVENT,
                UPDATE_EVENT,
            )
        )
