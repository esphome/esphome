from dataclasses import dataclass, field
import logging
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, mdns, network, psram, socket, wifi
from esphome.components.const import CONF_ENABLED, CONF_MANUFACTURER
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ESPHOME,
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_MDNS,
    CONF_MODEL,
    CONF_NAME,
    CONF_PROJECT,
    CONF_SAMPLE_RATE,
    CONF_SOURCE,
    CONF_TASK_STACK_IN_PSRAM,
    CONF_VERSION,
    CONF_WIDTH,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# mdns for autodiscovery
AUTO_LOAD = ["mdns"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["network"]
DOMAIN = "sendspin"

CONF_DISPLAY_OFFSET = "display_offset"
CONF_SENDSPIN_ID = "sendspin_id"

CONF_FIRMWARE_VERSION = "firmware_version"

# An empty device information string would be sent to the server as an empty value rather than
# falling back, so reject it instead of silently substituting the fallback. The 127 byte cap keeps
# the length prefix of a protobuf string field to a single byte, matching `esphome: project:`.
DEVICE_INFO_STRING = cv.All(cv.string_strict, cv.Length(min=1), cv.ByteLength(max=127))

CONF_INITIAL_STATIC_DELAY = "initial_static_delay"
CONF_FIXED_DELAY = "fixed_delay"
CONF_DECODE_MEMORY = "decode_memory"
CONF_CODECS = "codecs"

CONF_STATIC_PAIRING_CODE = "static_pairing_code"
CONF_UNPAIRED_ACCESS = "unpaired_access"
CONF_ON_OPEN_PAIRING_WINDOW = "on_open_pairing_window"
CONF_ON_CLOSE_PAIRING_WINDOW = "on_close_pairing_window"
CONF_ON_DISPLAY_PAIRING_CODE = "on_display_pairing_code"
CONF_ON_CLEAR_PAIRING_CODE = "on_clear_pairing_code"
CONF_ON_PAIRING_SUCCEEDED = "on_pairing_succeeded"
CONF_ON_PAIRING_FAILED = "on_pairing_failed"

# A static pairing code is exactly 8 decimal digits.
STATIC_PAIRING_CODE_DIGITS = 8


def _validate_static_pairing_code(value: Any) -> str:
    # string_strict so leading zeros survive and `!secret` works.
    value = cv.string_strict(value)
    if len(value) != STATIC_PAIRING_CODE_DIGITS or not (
        value.isascii() and value.isdigit()
    ):
        raise cv.Invalid(
            f"{CONF_STATIC_PAIRING_CODE} must be exactly "
            f"{STATIC_PAIRING_CODE_DIGITS} decimal digits "
            '(quote the value so leading zeros are preserved, e.g. "01234567")'
        )
    return value


# Matches ARTWORK_MAX_SLOTS in sendspin-cpp.
MAX_ARTWORK_SLOTS = 4

# sendspin-cpp library lives in the global `sendspin` namespace.
sendspin_library_ns = cg.global_ns.namespace("sendspin")

# Library Enums
SendspinCodecFormat = sendspin_library_ns.enum("SendspinCodecFormat", is_class=True)
CODEC_FORMAT_FLAC = SendspinCodecFormat.enum("FLAC")
CODEC_FORMAT_OPUS = SendspinCodecFormat.enum("OPUS")
CODEC_FORMAT_PCM = SendspinCodecFormat.enum("PCM")
CODEC_FORMAT_UNSUPPORTED = SendspinCodecFormat.enum("UNSUPPORTED")

CODEC_FLAC = "flac"
CODEC_OPUS = "opus"
CODEC_PCM = "pcm"

CODECS = {
    CODEC_FLAC: CODEC_FORMAT_FLAC,
    CODEC_OPUS: CODEC_FORMAT_OPUS,
    CODEC_PCM: CODEC_FORMAT_PCM,
}

# Opus only supports 48 kHz audio, so it is left out of the default list at other rates.
DEFAULT_CODECS = [CODEC_FLAC, CODEC_OPUS, CODEC_PCM]
OPUS_SAMPLE_RATE = 48000

SendspinImageFormat = sendspin_library_ns.enum("SendspinImageFormat", is_class=True)
IMAGE_FORMAT_JPEG = SendspinImageFormat.enum("JPEG")
IMAGE_FORMAT_PNG = SendspinImageFormat.enum("PNG")

SendspinImageSource = sendspin_library_ns.enum("SendspinImageSource", is_class=True)
IMAGE_SOURCE_ALBUM = SendspinImageSource.enum("ALBUM")
IMAGE_SOURCE_ARTIST = SendspinImageSource.enum("ARTIST")

# Library Structs
AudioSupportedFormatObject = sendspin_library_ns.struct("AudioSupportedFormatObject")
PlayerRoleConfig = sendspin_library_ns.struct("PlayerRoleConfig")
ArtworkRoleConfig = sendspin_library_ns.struct("ArtworkRoleConfig")
ImageSlotPreference = sendspin_library_ns.struct("ImageSlotPreference")

# MemoryLocation enum (from sendspin/types.h) controls SPIRAM-vs-internal-RAM placement
# preference for the player role's transfer buffers.
SendspinMemoryLocation = sendspin_library_ns.enum("MemoryLocation", is_class=True)

MEMORY_PSRAM = "psram"
MEMORY_INTERNAL = "internal"
MEMORY_LOCATIONS = [MEMORY_PSRAM, MEMORY_INTERNAL]
MEMORY_LOCATION_ENUM = {
    MEMORY_PSRAM: SendspinMemoryLocation.PREFER_EXTERNAL,
    MEMORY_INTERNAL: SendspinMemoryLocation.PREFER_INTERNAL,
}

# Trailing underscore avoids clashing with sendspin-cpp's global `sendspin` namespace.
# Analysis tools strip the trailing underscore (same pattern as `template_`).
sendspin_ns = cg.esphome_ns.namespace("sendspin_")
SendspinHub = sendspin_ns.class_(
    "SendspinHub",
    cg.Component,
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_OPEN_PAIRING_WINDOW, "add_on_open_pairing_window_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_CLOSE_PAIRING_WINDOW, "add_on_close_pairing_window_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_DISPLAY_PAIRING_CODE,
        "add_on_display_pairing_code_callback",
        [(cg.std_string, "code")],
    ),
    automation.CallbackAutomation(
        CONF_ON_CLEAR_PAIRING_CODE, "add_on_clear_pairing_code_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_PAIRING_SUCCEEDED,
        "add_on_pairing_succeeded_callback",
        [(cg.std_string, "server_id")],
    ),
    automation.CallbackAutomation(
        CONF_ON_PAIRING_FAILED,
        "add_on_pairing_failed_callback",
        [(cg.std_string, "server_id"), (cg.StringRef, "reason")],
    ),
)


@dataclass
class SendspinConfiguration:
    artwork_support: bool = False
    controller_support: bool = False
    metadata_support: bool = False
    player_support: bool = False
    visualizer_support: bool = False
    pairing_code_display_support: bool = False
    switch_types: set[str] = field(default_factory=set)

    artwork_preferences: list[ConfigType] = field(default_factory=list)
    player_config: ConfigType | None = None


def _get_data() -> SendspinConfiguration:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SendspinConfiguration()
    return CORE.data[DOMAIN]


def request_artwork_support() -> None:
    """Request artwork role support for Sendspin."""
    _get_data().artwork_support = True


def request_controller_support() -> None:
    """Request controller role support for Sendspin."""
    _get_data().controller_support = True


def request_metadata_support() -> None:
    """Request metadata role support for Sendspin."""
    _get_data().metadata_support = True


def request_player_support() -> None:
    """Request player role support for Sendspin."""
    _get_data().player_support = True


def request_visualizer_support() -> None:
    """Request visualizer role support for Sendspin."""
    _get_data().visualizer_support = True


def request_pairing_code_display_support() -> None:
    """Mark that the device can emit a dynamic pairing code (e.g. a pairing_code text sensor)."""
    _get_data().pairing_code_display_support = True


def request_switch(switch_type: str) -> None:
    """Mark that a sendspin switch of this type drives the matching hub setting."""
    _get_data().switch_types.add(switch_type)


def register_artwork_preference(config: ConfigType) -> int:
    """Register an artwork slot preference and return the slot it was given.

    A slot is a preference's position in the list, which is also the order the roles are
    advertised to the server in.
    """
    request_artwork_support()
    preferences = _get_data().artwork_preferences
    if len(preferences) >= MAX_ARTWORK_SLOTS:
        raise cv.Invalid(
            f"Too many Sendspin image slots. Maximum is {MAX_ARTWORK_SLOTS}."
        )
    preferences.append(config)
    return len(preferences) - 1


def register_player_config(config: ConfigType) -> None:
    """Register the player role config from the media source subcomponent."""
    data = _get_data()
    request_player_support()
    if data.player_config is not None:
        raise cv.Invalid(
            "Only one sendspin media_source player configuration is supported"
        )
    data.player_config = config


def _request_high_performance_networking(config: ConfigType) -> ConfigType:
    """Request high performance networking for Sendspin streaming.

    Also enables wake_loop_threadsafe support for fast defer() callbacks
    from background threads (WebSocket handler, image decoder).
    """
    network.require_high_performance_networking()
    # Socket consumption varies by mode:
    # - Server mode: 1 listening socket + 4 client connections (established connection, unproven connections, and a spare)
    # - Client mode: 1 outbound connection
    socket.consume_sockets(
        1, "sendspin_websocket_server", socket.SocketType.TCP_LISTEN
    )(config)
    socket.consume_sockets(4, "sendspin_websocket_server")(config)
    socket.consume_sockets(1, "sendspin_websocket_client")(config)

    wifi.enable_runtime_power_save_control()
    wifi.enable_runtime_roaming_suppression()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_MANUFACTURER): DEVICE_INFO_STRING,
            cv.Optional(CONF_MODEL): DEVICE_INFO_STRING,
            cv.Optional(CONF_FIRMWARE_VERSION): DEVICE_INFO_STRING,
            cv.Optional(CONF_STATIC_PAIRING_CODE): cv.sensitive(
                _validate_static_pairing_code
            ),
            # No default: the unpaired access switch rejects this key when it is set.
            cv.Optional(CONF_UNPAIRED_ACCESS): cv.boolean,
            cv.Optional(CONF_ON_OPEN_PAIRING_WINDOW): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_CLOSE_PAIRING_WINDOW): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_DISPLAY_PAIRING_CODE): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_CLEAR_PAIRING_CODE): automation.validate_automation({}),
            cv.Optional(CONF_ON_PAIRING_SUCCEEDED): automation.validate_automation({}),
            cv.Optional(CONF_ON_PAIRING_FAILED): automation.validate_automation({}),
        }
    ),
    cv.only_on_esp32,
    # sendspin-cpp needs noise-c as an ESP-IDF component, which Arduino below IDF 6.0 cannot use.
    cv.only_with_framework("esp-idf"),
    _request_high_performance_networking,
)


def _offers_dynamic_pairing_code(config: ConfigType) -> bool:
    """Whether the device can show a dynamic pairing code, so dynamic_pairing_code is advertised."""
    return bool(
        config.get(CONF_ON_DISPLAY_PAIRING_CODE)
        or _get_data().pairing_code_display_support
    )


def _has_pairing_method(config: ConfigType) -> bool:
    """Whether the config gives a server any way to pair with the device."""
    return CONF_STATIC_PAIRING_CODE in config or _offers_dynamic_pairing_code(config)


def _final_validate(config: ConfigType) -> ConfigType:
    dynamic_code = _offers_dynamic_pairing_code(config)
    # The protocol allows only one pairing code method.
    if dynamic_code and CONF_STATIC_PAIRING_CODE in config:
        raise cv.Invalid(
            f"'{CONF_STATIC_PAIRING_CODE}' cannot be used with a dynamic pairing code "
            f"({CONF_ON_DISPLAY_PAIRING_CODE} or a pairing_code text sensor), since only "
            "one pairing code method can be offered",
            path=[CONF_STATIC_PAIRING_CODE],
        )
    if not config.get(CONF_UNPAIRED_ACCESS, True) and not _has_pairing_method(config):
        _LOGGER.warning(
            "'%s' is off but there is no pairing method (%s or a dynamic pairing code), so no "
            "new server can pair with this device",
            CONF_UNPAIRED_ACCESS,
            CONF_STATIC_PAIRING_CODE,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def _request_controller_role(config: ConfigType) -> ConfigType:
    """Request the controller role for the sendspin.switch action."""
    request_controller_support()
    return config


# Selects the hub. sendspin.switch adds the controller role it needs; the pairing window actions need no role.
SENDSPIN_HUB_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SendspinHub),
        }
    )
)

SENDSPIN_SIMPLE_ACTION_SCHEMA = cv.All(
    SENDSPIN_HUB_ACTION_SCHEMA, _request_controller_role
)


automation.register_apply_action(
    "sendspin.switch",
    SENDSPIN_SIMPLE_ACTION_SCHEMA,
    automation.ApplyCall("switch_client()"),
)


automation.register_apply_action(
    "sendspin.confirm_pairing_window",
    SENDSPIN_HUB_ACTION_SCHEMA,
    automation.ApplyCall("confirm_pairing_window()"),
)

automation.register_apply_action(
    "sendspin.cancel_pairing_window",
    SENDSPIN_HUB_ACTION_SCHEMA,
    automation.ApplyCall("cancel_pairing_window()"),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()

    # Device information for the server's client/hello message. Falls back to the project
    # information, which is written as `manufacturer.model`. Anything still unset keeps the
    # default the hub itself applies: the ESPHome name and version.
    project = CORE.config[CONF_ESPHOME].get(CONF_PROJECT, {})
    project_manufacturer, _, project_model = project.get(CONF_NAME, "").partition(".")
    for value, setter in (
        (config.get(CONF_MANUFACTURER) or project_manufacturer, var.set_manufacturer),
        (config.get(CONF_MODEL) or project_model, var.set_model),
        (
            config.get(CONF_FIRMWARE_VERSION) or project.get(CONF_VERSION),
            var.set_firmware_version,
        ),
    ):
        if value:
            cg.add(setter(value))

    if (code := config.get(CONF_STATIC_PAIRING_CODE)) is not None:
        cg.add(var.set_static_pairing_code(code))

    # The client starts once both are set: here, or by the switch that drives the setting.
    switch_types = _get_data().switch_types
    if CONF_ENABLED not in switch_types:
        cg.add(var.set_enabled(True))
    if CONF_UNPAIRED_ACCESS not in switch_types:
        cg.add(var.set_unpaired_access_enabled(config.get(CONF_UNPAIRED_ACCESS, True)))

    if _offers_dynamic_pairing_code(config):
        cg.add(var.set_pairing_code_display_supported(True))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    # sendspin-cpp library
    esp32.add_idf_component(name="sendspin/sendspin-cpp", ref="0.9.3")
    # esp_websocket_client links esp_tls even for ws:// connections.
    esp32.request_tls()

    cg.add_define("USE_SENDSPIN", True)  # for MDNS and the native API

    # Service starts disabled and the hub enables it; always advertised where unsupported
    if mdns.request_service_enable_disable():
        mdns_var = await cg.get_variable(CORE.config[CONF_MDNS][CONF_ID])
        cg.add(var.set_mdns(mdns_var))

    data = _get_data()

    # The color and source roles are not yet wired up in ESPHome; disable them in the library for now.
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_COLOR", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE", False)

    # Configure Sendspin roles based on requested features (ESPHome internally via USE_SENDSPIN_*)
    # and disable building unused code paths in the sendspin-cpp library (IDF SDKConfig via CONFIG_SENDSPIN_ENABLE_*).
    if data.artwork_support:
        cg.add_define("USE_SENDSPIN_ARTWORK", True)

        # require_frame_done is always on: SendspinImageSlot always acks a delivery, either
        # immediately or from the transition_finished action.
        preference_structs = [
            cg.StructInitializer(
                ImageSlotPreference,
                ("source", pref[CONF_SOURCE]),
                ("format", pref[CONF_FORMAT]),
                ("width", pref[CONF_WIDTH]),
                ("height", pref[CONF_HEIGHT]),
                ("require_frame_done", True),
                ("display_offset_ms", pref[CONF_DISPLAY_OFFSET]),
            )
            for pref in data.artwork_preferences
        ]

        artwork_psram_stack = bool(config.get(CONF_TASK_STACK_IN_PSRAM))
        artwork_config = cg.StructInitializer(
            ArtworkRoleConfig,
            ("preferred_formats", preference_structs),
            ("psram_stack", artwork_psram_stack),
        )
        cg.add(var.set_artwork_config(artwork_config))
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_ARTWORK", False)

    if data.controller_support:
        cg.add_define("USE_SENDSPIN_CONTROLLER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_CONTROLLER", False)

    if data.metadata_support:
        cg.add_define("USE_SENDSPIN_METADATA", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_METADATA", False)

    if data.player_support:
        cg.add_define("USE_SENDSPIN_PLAYER", True)

        # Configures the player role. Each configured codec is advertised for 16 bits per sample
        # mono and stereo at the configured sample rate. The order is a preference order, both for
        # the codecs themselves and for stereo over mono.
        player_cfg = data.player_config
        sample_rate = player_cfg[CONF_SAMPLE_RATE]

        codecs = [CODECS[codec] for codec in player_cfg[CONF_CODECS]]

        def _audio_format(codec: MockObj, channels: int) -> cg.StructInitializer:
            return cg.StructInitializer(
                AudioSupportedFormatObject,
                ("codec", codec),
                ("channels", channels),
                ("sample_rate", sample_rate),
                ("bit_depth", 16),
            )

        audio_format_structs = [
            _audio_format(codec, channels) for codec in codecs for channels in (2, 1)
        ]

        psram_stack = player_cfg.get(CONF_TASK_STACK_IN_PSRAM, False)
        if psram_stack:
            psram.request_external_task_stack()

        player_struct_fields = [
            ("audio_formats", audio_format_structs),
            ("audio_buffer_capacity", player_cfg[CONF_BUFFER_SIZE]),
            ("fixed_delay_us", player_cfg[CONF_FIXED_DELAY]),
            # The released YAML key name is kept for compatibility.
            ("initial_output_delay_ms", player_cfg[CONF_INITIAL_STATIC_DELAY]),
            ("psram_stack", psram_stack),
        ]
        if (decode_memory := player_cfg.get(CONF_DECODE_MEMORY)) is not None:
            player_struct_fields.append(
                ("decode_buffer_location", MEMORY_LOCATION_ENUM[decode_memory])
            )
        player_config_struct = cg.StructInitializer(
            PlayerRoleConfig,
            *player_struct_fields,
        )
        cg.add(var.set_player_config(player_config_struct))
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_PLAYER", False)

    if not data.player_support or CODEC_OPUS not in data.player_config[CONF_CODECS]:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_OPUS", False)

    if data.visualizer_support:
        cg.add_define("USE_SENDSPIN_VISUALIZER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_VISUALIZER", False)
