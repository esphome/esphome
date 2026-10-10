from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import esp32_camera
from esphome.components.const import CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_ID, CONF_OFFSET_X, CONF_OFFSET_Y, CONF_WIDTH
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32_camera"]
DOMAIN = "esp32_camera_window"

# Not in esphome/const.py, which is frozen; named like the other hub references
# (climate_id, uart_id, ...).
CONF_CAMERA_ID = "camera_id"


esp32_camera_window_ns = cg.esphome_ns.namespace("esp32_camera_window")
Esp32CameraWindow = esp32_camera_window_ns.class_("Esp32CameraWindow", cg.Component)

WINDOW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_OFFSET_X): cv.int_range(min=0),
        cv.Required(CONF_OFFSET_Y): cv.int_range(min=0),
        cv.Required(CONF_WIDTH): cv.int_range(min=1),
        cv.Required(CONF_HEIGHT): cv.int_range(min=1),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Esp32CameraWindow),
            cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
            cv.Optional(CONF_WINDOW): WINDOW_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    # The window is applied through esp_camera.h, which is only available in ESP-IDF builds.
    cv.only_with_framework("esp-idf"),
)


async def to_code(config: ConfigType) -> None:
    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    var = cg.new_Pvariable(config[CONF_ID], camera)
    await cg.register_component(var, config)
    if (window := config.get(CONF_WINDOW)) is not None:
        cg.add(
            var.set_initial_window(
                window[CONF_OFFSET_X],
                window[CONF_OFFSET_Y],
                window[CONF_WIDTH],
                window[CONF_HEIGHT],
            )
        )
    cg.add_define("USE_ESP32_CAMERA_WINDOW")


WINDOW_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Esp32CameraWindow),
        cv.Required(CONF_OFFSET_X): cv.int_range(min=0),
        cv.Required(CONF_OFFSET_Y): cv.int_range(min=0),
        cv.Required(CONF_WIDTH): cv.int_range(min=1),
        cv.Required(CONF_HEIGHT): cv.int_range(min=1),
    }
)

automation.register_apply_action(
    f"{DOMAIN}.set",
    WINDOW_ACTION_SCHEMA,
    automation.ApplyCall(
        "set_window({}, {}, {}, {})",
        (
            (CONF_OFFSET_X, cg.int_),
            (CONF_OFFSET_Y, cg.int_),
            (CONF_WIDTH, cg.int_),
            (CONF_HEIGHT, cg.int_),
        ),
    ),
)

automation.register_apply_action(
    f"{DOMAIN}.reset",
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Esp32CameraWindow)}),
    automation.ApplyCall("reset_window()"),
)
