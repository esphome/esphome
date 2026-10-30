from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_ON_STATE_CHANGE
from esphome.config_helpers import (
    filter_source_files_from_defines,
    filter_source_files_from_platform,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_ON_ERROR,
    CONF_OTA,
    CONF_PLATFORM,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

OTA_STATE_LISTENER_KEY = "ota_state_listener"

CODEOWNERS = ["@esphome/core"]
DOMAIN = "ota"


def AUTO_LOAD() -> list[str]:
    components = ["safe_mode"]
    if not CORE.using_zephyr:
        components.extend(["md5"])
    if CORE.is_esp32:
        components.extend(["watchdog"])
    return components


IS_PLATFORM_COMPONENT = True

CONF_ON_ABORT = "on_abort"
CONF_ON_BEGIN = "on_begin"
CONF_ON_END = "on_end"
CONF_ON_PROGRESS = "on_progress"


ota_ns = cg.esphome_ns.namespace("ota")
OTAComponent = ota_ns.class_("OTAComponent", cg.Component)
OTAState = ota_ns.enum("OTAState")

_request_state_callback_slot = cg.slot_counter("OTA_STATE_CALLBACK_COUNT")

# Callback parameters, shared by every key and every OTA component
_STATE_CALLBACK_PARAMS: TemplateArgsType = [
    (OTAState, "state"),
    (cg.float_, "progress"),
    (cg.uint8, "error"),
    (OTAComponent.operator("ptr"), "component"),
]
# conf key: (automation args, forwarded expressions, state filter)
_STATE_AUTOMATIONS: dict[str, tuple[TemplateArgsType, list[str], MockObj | None]] = {
    CONF_ON_STATE_CHANGE: ([(OTAState, "state")], ["state"], None),
    CONF_ON_ABORT: ([], [], ota_ns.OTA_ABORT),
    CONF_ON_BEGIN: ([], [], ota_ns.OTA_STARTED),
    CONF_ON_PROGRESS: ([(cg.float_, "x")], ["progress"], ota_ns.OTA_IN_PROGRESS),
    CONF_ON_END: ([], [], ota_ns.OTA_COMPLETED),
    CONF_ON_ERROR: ([(cg.uint8, "x")], ["error"], ota_ns.OTA_ERROR),
}


def _ota_final_validate(config: ConfigType) -> None:
    if len(config) < 1:
        raise cv.Invalid(
            f"At least one platform must be specified for '{CONF_OTA}'; add '{CONF_PLATFORM}: {CONF_ESPHOME}' for original OTA functionality"
        )


FINAL_VALIDATE_SCHEMA = _ota_final_validate

BASE_OTA_SCHEMA = cv.Schema(
    {cv.Optional(key): automation.validate_automation({}) for key in _STATE_AUTOMATIONS}
)


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_OTA")
    CORE.add_job(final_step)

    if CORE.is_rp2 and CORE.using_arduino:
        cg.add_library("Updater", None)


async def ota_to_code(var: MockObj, config: ConfigType) -> None:
    await cg.past_safe_mode()
    for key, (args, forward, state) in _STATE_AUTOMATIONS.items():
        when = "component == {parent}"
        if state is not None:
            when += f" && state == {state}"
        for conf in config.get(key, []):
            _request_state_callback_slot()
            callback = await automation.build_trigger_callback(
                args, conf, _STATE_CALLBACK_PARAMS, forward, when, var
            )
            cg.add(MockObj(f"{OTAComponent}::add_state_callback")(callback))
            request_ota_state_listeners()


def request_ota_state_listeners() -> None:
    """Request that OTA state listeners be compiled in.

    Components that need to be notified about OTA state changes (start, progress,
    complete, error) should call this function during their code generation.
    This enables the add_state_listener() API on OTAComponent.
    """
    CORE.data[OTA_STATE_LISTENER_KEY] = True


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step() -> None:
    """Final code generation step to configure optional OTA features."""
    if CORE.data.get(OTA_STATE_LISTENER_KEY, False):
        cg.add_define("USE_OTA_STATE_LISTENER")


_filter_backend_source_files = filter_source_files_from_platform(
    {
        "ota_backend_esp_idf.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "ota_backend_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "ota_backend_arduino_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
        "ota_backend_arduino_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "ota_backend_host.cpp": {PlatformFramework.HOST_NATIVE},
    }
)


# USE_OTA_SIGNED_VERIFICATION_MULTI_KEY is set only on ESP32/IDF;
# USE_OTA_PARTITIONS is set by the esphome OTA platform when
# allow_partition_access is enabled.
_filter_define_source_files = filter_source_files_from_defines(
    {
        "ota_signature_esp_idf.cpp": "USE_OTA_SIGNED_VERIFICATION_MULTI_KEY",
        "ota_bootloader_esp_idf.cpp": "USE_OTA_PARTITIONS",
        "ota_partitions_esp_idf.cpp": "USE_OTA_PARTITIONS",
    }
)


def FILTER_SOURCE_FILES() -> list[str]:
    return _filter_backend_source_files() + _filter_define_source_files()
