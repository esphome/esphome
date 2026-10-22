"""VNC (RFB) client that shows a remote screen on a display."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import display, network, socket
from esphome.components.const import (
    BYTE_ORDER_LITTLE,
    CONF_HOST,
    CONF_RECONNECT_INTERVAL,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_DISPLAY_ID,
    CONF_ID,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PLATFORM,
    CONF_PORT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@MindFreeze"]
DEPENDENCIES = ["display", "network"]
AUTO_LOAD = ["socket"]
DOMAIN = "vnc"
MULTI_CONF = True

CONF_MAX_UPDATE_DELAY = "max_update_delay"
CONF_UPDATE_DELAY = "update_delay"

vnc_ns = cg.esphome_ns.namespace("vnc")
VncClient = vnc_ns.class_("VncClient", cg.Component)

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
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


VNC_ID_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(VncClient)},
)

automation.register_apply_condition("vnc.is_connected", VNC_ID_SCHEMA, "is_connected()")
