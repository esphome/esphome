import esphome.codegen as cg
from esphome.components import binary_sensor, display
import esphome.config_validation as cv
from esphome.const import CONF_PAGE_ID, CONF_PAGES

from .. import CONF_TOUCHSCREEN_ID, TouchListener, Touchscreen, touchscreen_ns

DEPENDENCIES = ["touchscreen"]

TouchscreenBinarySensor = touchscreen_ns.class_(
    "TouchscreenBinarySensor",
    binary_sensor.BinarySensor,
    cg.Component,
    TouchListener,
    cg.Parented.template(Touchscreen),
)

CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"
CONF_USE_RAW = "use_raw"


def _validate_coords(config):
    if (
        config[CONF_X_MAX] < config[CONF_X_MIN]
        or config[CONF_Y_MAX] < config[CONF_Y_MIN]
    ):
        raise cv.Invalid(
            f"{CONF_X_MAX} is less than {CONF_X_MIN} or {CONF_Y_MAX} is less than {CONF_Y_MIN}"
        )
    return config


def _set_pages(config: dict) -> dict:
    if CONF_PAGES in config or CONF_PAGE_ID not in config:
        return config

    config = config.copy()
    config[CONF_PAGES] = [config.pop(CONF_PAGE_ID)]
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(TouchscreenBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
            cv.Optional(CONF_USE_RAW, default=False): cv.boolean,
            cv.Required(CONF_X_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_X_MAX): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MAX): cv.int_range(min=0, max=2000),
            cv.Exclusive(CONF_PAGE_ID, group_of_exclusion=CONF_PAGES): cv.use_id(
                display.DisplayPage
            ),
            cv.Exclusive(CONF_PAGES, group_of_exclusion=CONF_PAGES): cv.ensure_list(
                cv.use_id(display.DisplayPage)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_coords,
    _set_pages,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_TOUCHSCREEN_ID])

    cg.add(var.set_use_raw(config[CONF_USE_RAW]))
    cg.add(
        var.set_area(
            config[CONF_X_MIN],
            config[CONF_X_MAX],
            config[CONF_Y_MIN],
            config[CONF_Y_MAX],
        )
    )

    for page_id in config.get(CONF_PAGES, []):
        page = await cg.get_variable(page_id)
        cg.add(var.add_page(page))
