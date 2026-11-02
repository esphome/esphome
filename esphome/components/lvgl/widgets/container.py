import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_WIDTH
from esphome.cpp_generator import MockObj

from ..defines import (
    CONF_CLICKABLE,
    CONF_CONTAINER,
    CONF_MAIN,
    CONF_SCROLLABLE,
    CONF_SCROLLBAR,
)
from ..lv_validation import size
from ..lvcode import lv_obj
from ..types import lv_obj_t
from . import WidgetType

CONTAINER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_HEIGHT, default="100%"): size,
        cv.Optional(CONF_WIDTH, default="100%"): size,
    }
)


class ContainerType(WidgetType):
    """
    A simple container widget that can hold other widgets and which defaults to a 100% size.
    Made from an obj with all styles removed
    """

    def __init__(self):
        super().__init__(
            CONF_CONTAINER,
            lv_obj_t,
            (CONF_MAIN, CONF_SCROLLBAR),
            schema=CONTAINER_SCHEMA,
            modify_schema={},
            lv_name=CONF_CONTAINER,
        )
        self.styles = {}

    async def on_create(self, var: MockObj, config: dict) -> None:
        # A container is not clickable, so dragging its background does not scroll it.
        # An explicit `scrollable: true` asks for that, unless `clickable` is also set.
        if config.get(CONF_SCROLLABLE) is True and CONF_CLICKABLE not in config:
            lv_obj.set_clickable(var, True)


container_spec = ContainerType()
