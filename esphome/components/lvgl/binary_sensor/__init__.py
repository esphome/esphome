import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.binary_sensor import (
    binary_sensor_schema,
    BinarySensor,
    new_binary_sensor,
)
from .. import (
    add_init_lambda,
    LVGL_SCHEMA,
)
from ..defines import CONF_WIDGET, CONF_LVGL_ID
from ..lv_validation import requires_component
from ..types import lv_pseudo_button_t
from ..widget import get_widget, Widget

BASE_SCHEMA = binary_sensor_schema(BinarySensor).extend(LVGL_SCHEMA)
CONFIG_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
        }
    ),
    requires_component("binary_sensor"),
)


async def to_code(config):
    sensor = await new_binary_sensor(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_WIDGET])
    assert isinstance(widget, Widget)
    init = [f"{sensor}->publish_initial_state({widget.is_pressed()})"]
    init.extend(
        widget.set_event_cb(f"{sensor}->publish_state(true);", "LV_EVENT_PRESSING")
    )
    init.extend(
        widget.set_event_cb(f"{sensor}->publish_state(false);", "LV_EVENT_RELEASED")
    )
    await add_init_lambda(paren, init)
