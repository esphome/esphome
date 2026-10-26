"""VNC (RFB) client that shows a remote screen on a display."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import display, network, socket
from esphome.components.const import (
    BYTE_ORDER_LITTLE,
    CONF_BUTTONS,
    CONF_HOST,
    CONF_RECONNECT_INTERVAL,
)
from esphome.components.touchscreen import CONF_TOUCHSCREEN_ID, Touchscreen
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_AUTO_CLEAR_ENABLED,
    CONF_DISPLAY,
    CONF_DISPLAY_ID,
    CONF_ID,
    CONF_KEY,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_TEXT,
    CONF_X,
    CONF_Y,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType, TemplateArgsType

from .keysyms import button_mask, keysym

CODEOWNERS = ["@MindFreeze"]
DEPENDENCIES = ["display", "network"]
AUTO_LOAD = ["socket", "unicode"]
DOMAIN = "vnc"
MULTI_CONF = True

CONF_MAX_UPDATE_DELAY = "max_update_delay"
CONF_UPDATE_DELAY = "update_delay"
CONF_PIXEL_FORMAT = "pixel_format"

vnc_ns = cg.esphome_ns.namespace("vnc")
VncClient = vnc_ns.class_("VncClient", cg.Component)
VncPixelFormat = vnc_ns.enum("VncPixelFormat", is_class=True)
VncKeyAction = vnc_ns.enum("VncKeyAction", is_class=True)

PIXEL_FORMATS = VncPixelFormat.options(
    "auto", "rgb565", "rgb332", prefix="VNC_PIXEL_FORMAT_"
)

KEY_ACTIONS = VncKeyAction.options("press", "down", "up", prefix="VNC_KEY_ACTION_")

# Drivers that cannot take another component's pixels yet.
UNSUPPORTED_DISPLAYS = dict.fromkeys(
    ("mipi_dsi", "mipi_rgb", "mipi_spi"), "it is not supported by vnc yet"
) | {
    "waveshare_epaper": "it draws every colour except pure black as ink; use epaper_spi"
}


def _register_writer(config: ConfigType) -> ConfigType:
    display.register_external_writer(config[CONF_DISPLAY_ID], DOMAIN)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VncClient),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.Display),
            cv.Required(CONF_HOST): socket.ipv4_host,
            cv.Optional(CONF_PORT, default=5900): cv.port,
            cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(seconds=1)),
            ),
            cv.Optional(
                CONF_UPDATE_DELAY, default="200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_MAX_UPDATE_DELAY, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PIXEL_FORMAT, default="auto"): cv.enum(
                PIXEL_FORMATS, lower=True
            ),
            cv.Optional(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _register_writer,
    socket.consume_sockets(1, DOMAIN),
    network.require_ipv4(DOMAIN),
)


def _final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    disp_id = config[CONF_DISPLAY_ID]
    disp_conf = fv.get_declaring_config(disp_id)

    if (platform := disp_conf.get(CONF_PLATFORM)) in UNSUPPORTED_DISPLAYS:
        raise cv.Invalid(
            f"Display '{disp_id}' uses the {platform} platform: "
            f"{UNSUPPORTED_DISPLAYS[platform]}",
            path=[CONF_DISPLAY_ID],
        )
    if (
        display.get_display_metadata(disp_id).has_writer
        or disp_conf.get(CONF_AUTO_CLEAR_ENABLED) is True
    ):
        raise cv.Invalid(
            f"vnc draws the whole of display '{disp_id}'; remove lambda, pages, "
            "auto_clear_enabled and show_test_card from it",
            path=[CONF_DISPLAY_ID],
        )
    if len(writers := display.get_external_writers(disp_id)) > 1:
        raise cv.Invalid(
            f"Display '{disp_id}' is drawn by more than one component: {', '.join(writers)}",
            path=[CONF_DISPLAY_ID],
        )
    lvgl_configs = full_config.get("lvgl") or []
    if isinstance(lvgl_configs, dict):
        lvgl_configs = [lvgl_configs]
    for lvgl_config in lvgl_configs:
        if any(d.id == disp_id.id for d in lvgl_config.get("displays", [])):
            raise cv.Invalid(
                f"Display '{disp_id}' is also used by LVGL", path=[CONF_DISPLAY_ID]
            )
    if (ts_id := config.get(CONF_TOUCHSCREEN_ID)) is not None:
        ts_display = fv.get_declaring_config(ts_id).get(CONF_DISPLAY)
        if ts_display is not None and ts_display.id != disp_id.id:
            raise cv.Invalid(
                f"Touchscreen '{ts_id}' belongs to display '{ts_display}', "
                f"not '{disp_id}'",
                path=[CONF_TOUCHSCREEN_ID],
            )


FINAL_VALIDATE_SCHEMA = _final_validate

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_CONNECT, "add_on_connect_callback"),
    automation.CallbackAutomation(CONF_ON_DISCONNECT, "add_on_disconnect_callback"),
)


async def to_code(config: ConfigType) -> None:
    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    # Ask the server for the display's own byte order, so block-copying drivers skip the per-pixel path
    meta = display.get_display_metadata(config[CONF_DISPLAY_ID])
    var = cg.new_Pvariable(config[CONF_ID], disp, meta.byte_order != BYTE_ORDER_LITTLE)
    await cg.register_component(var, config)
    socket.require_tcp_client_link()
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_update_delay(config[CONF_UPDATE_DELAY]))
    cg.add(var.set_max_update_delay(config[CONF_MAX_UPDATE_DELAY]))
    cg.add(var.set_pixel_format(config[CONF_PIXEL_FORMAT]))
    if (
        await cg.add_variable(var.register_touchscreen, config.get(CONF_TOUCHSCREEN_ID))
        is not None
    ):
        cg.add_define("USE_VNC_INPUT")
        cg.add_define("USE_VNC_TOUCHSCREEN")
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


_CLIENT_ID = {cv.GenerateID(): cv.use_id(VncClient)}

VNC_ID_SCHEMA = automation.maybe_simple_id(_CLIENT_ID)

automation.register_apply_action(
    "vnc.refresh", VNC_ID_SCHEMA, automation.ApplyCall("refresh()")
)

automation.register_apply_condition("vnc.is_connected", VNC_ID_SCHEMA, "is_connected()")


def _register_input_action(
    name: str, schema: cv.Schema, *fields: automation.ApplyField | automation.ApplyCall
) -> None:
    builder = automation.apply_action_builder(name, schema, *fields)

    @automation.register_action(name, automation.ApplyAction, schema, synchronous=True)
    async def input_action_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: TemplateArgsType,
    ) -> MockObj:
        cg.add_define("USE_VNC_INPUT")
        return await builder(config, action_id, template_arg, args)


_register_input_action(
    "vnc.send_pointer",
    cv.Schema(
        {
            **_CLIENT_ID,
            cv.Required(CONF_X): cv.templatable(cv.uint16_t),
            cv.Required(CONF_Y): cv.templatable(cv.uint16_t),
            cv.Optional(CONF_BUTTONS, default=0): cv.templatable(button_mask),
        }
    ),
    automation.ApplyCall(
        "send_pointer({}, {}, {})",
        ((CONF_X, cg.uint16), (CONF_Y, cg.uint16), (CONF_BUTTONS, cg.uint8)),
    ),
)

_register_input_action(
    "vnc.send_key",
    automation.maybe_conf(
        CONF_KEY,
        cv.Schema(
            {
                **_CLIENT_ID,
                cv.Required(CONF_KEY): cv.templatable(keysym),
                cv.Optional(CONF_ACTION, default="press"): cv.enum(
                    KEY_ACTIONS, lower=True
                ),
            }
        ),
    ),
    automation.ApplyCall(
        "send_key({}, {})", ((CONF_KEY, cg.uint32), (CONF_ACTION, VncKeyAction))
    ),
)

_register_input_action(
    "vnc.type_text",
    automation.maybe_conf(
        CONF_TEXT,
        cv.Schema(
            {
                **_CLIENT_ID,
                cv.Required(CONF_TEXT): cv.templatable(cv.string),
            }
        ),
    ),
    automation.ApplyField(
        CONF_TEXT, "type_text", cg.std_string, const_fn=automation.literal_with_length
    ),
)
