"""VNC (RFB) client that shows a remote screen on a display."""

import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import display, socket
from esphome.components.const import CONF_HOST, CONF_RECONNECT_INTERVAL
from esphome.config import Config
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_DISPLAY_ID,
    CONF_ID,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_ROTATION,
)
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@MindFreeze"]
DEPENDENCIES = ["display", "network"]
AUTO_LOAD = ["socket"]
DOMAIN = "vnc"
MULTI_CONF = True

_LOGGER = logging.getLogger(__name__)

CONF_MAX_UPDATE_DELAY = "max_update_delay"
CONF_UPDATE_DELAY = "update_delay"
CONF_PIXEL_FORMAT = "pixel_format"

VNC_PASSWORD_MAX_BYTES = 8

vnc_ns = cg.esphome_ns.namespace("vnc")
VncClient = vnc_ns.class_("VncClient", cg.Component)
VncPixelFormat = vnc_ns.enum("VncPixelFormat", is_class=True)

PIXEL_FORMATS = {
    "auto": VncPixelFormat.VNC_PIXEL_FORMAT_AUTO,
    "rgb565": VncPixelFormat.VNC_PIXEL_FORMAT_RGB565,
    "rgb332": VncPixelFormat.VNC_PIXEL_FORMAT_RGB332,
}

# Drivers that cannot take another component's pixels yet.
UNSUPPORTED_DISPLAYS = {
    "mipi_dsi": "it is not supported by vnc yet",
    "mipi_rgb": "it is not supported by vnc yet",
    "mipi_spi": "it is not supported by vnc yet",
    "waveshare_epaper": "it draws every colour except pure black as ink; use epaper_spi",
}
# Their RGB565 fast path draws in panel coordinates and skips software rotation.
UNROTATED_DISPLAYS = ("rpi_dpi_rgb", "st7701s")


def validate_password(value: object) -> str:
    value = cv.string_strict(value)
    if not value:
        raise cv.Invalid(
            "The password must not be empty; leave it out for a server without one"
        )
    if len(value.encode("utf-8")) > VNC_PASSWORD_MAX_BYTES:
        _LOGGER.warning(
            "VNC passwords are limited to %d bytes; only the first %d are used",
            VNC_PASSWORD_MAX_BYTES,
            VNC_PASSWORD_MAX_BYTES,
        )
    return value


def vnc_auth_key(password: str) -> list[int]:
    """The DES key for a VNC password: 8 bytes, zero padded, each byte bit-reversed."""
    raw = password.encode("utf-8")[:VNC_PASSWORD_MAX_BYTES].ljust(
        VNC_PASSWORD_MAX_BYTES, b"\0"
    )
    return [int(f"{byte:08b}"[::-1], 2) for byte in raw]


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
            cv.Optional(CONF_PASSWORD): cv.sensitive(validate_password),
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
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _register_writer,
    socket.consume_sockets(1, DOMAIN),
)


def _config_for_id(full_config: Config, id_: ID) -> ConfigType:
    return full_config.get_config_for_path(full_config.get_path_for_id(id_)[:-1])


def _final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    disp_id = config[CONF_DISPLAY_ID]
    disp_conf = _config_for_id(full_config, disp_id)

    if (platform := disp_conf.get(CONF_PLATFORM)) in UNSUPPORTED_DISPLAYS:
        raise cv.Invalid(
            f"Display '{disp_id}' uses the {platform} platform: "
            f"{UNSUPPORTED_DISPLAYS[platform]}",
            path=[CONF_DISPLAY_ID],
        )
    if platform in UNROTATED_DISPLAYS and disp_conf.get(CONF_ROTATION, 0):
        raise cv.Invalid(
            f"Display '{disp_id}' uses the {platform} platform, which ignores rotation "
            "for pixels drawn by vnc; remove rotation from it",
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

FILTER_SOURCE_FILES = filter_source_files_from_defines({"vnc_des.cpp": "USE_VNC_AUTH"})

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_CONNECT, "add_on_connect_callback"),
    automation.CallbackAutomation(CONF_ON_DISCONNECT, "add_on_disconnect_callback"),
)


async def to_code(config: ConfigType) -> None:
    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    var = cg.new_Pvariable(config[CONF_ID], disp)
    await cg.register_component(var, config)
    socket.require_tcp_client_link()
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_update_delay(config[CONF_UPDATE_DELAY]))
    cg.add(var.set_max_update_delay(config[CONF_MAX_UPDATE_DELAY]))
    cg.add(var.set_pixel_format(config[CONF_PIXEL_FORMAT]))
    if (password := config.get(CONF_PASSWORD)) is not None:
        cg.add_define("USE_VNC_AUTH")
        key_id = ID(f"{config[CONF_ID]}_auth_key", is_declaration=True, type=cg.uint8)
        key = cg.progmem_array(key_id, cg.ArrayInitializer(*vnc_auth_key(password)))
        cg.add(var.set_auth_key(key))
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


VNC_ID_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(VncClient)},
)

for _name, _call in (
    ("vnc.connect", "connect()"),
    ("vnc.disconnect", "disconnect()"),
    ("vnc.refresh", "refresh()"),
):
    automation.register_apply_action(_name, VNC_ID_SCHEMA, automation.ApplyCall(_call))

automation.register_apply_condition("vnc.is_connected", VNC_ID_SCHEMA, "is_connected()")
