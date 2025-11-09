import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_WIDTH
from esphome.cpp_generator import MockObj

from ..defines import CONF_CONTAINER, CONF_MAIN, CONF_OBJ, CONF_SCROLLBAR
from ..lv_validation import size
from ..lvcode import lv
from ..types import WidgetType, lv_obj_t

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
            lv_name=CONF_OBJ,
        )
        self.styles = {}

    def on_create(self, var: MockObj, config: dict):
        lv.obj_remove_style_all(var)


container_spec = ContainerType()
