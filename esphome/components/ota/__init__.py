from esphome import automation
import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_ON_ERROR,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_TRIGGER_ID,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority

OTA_STATE_LISTENER_KEY = "ota_state_listener"

CODEOWNERS = ["@esphome/core"]


def AUTO_LOAD() -> list[str]:
    # Every backend computes its own MD5 over the transferred image. nrf52 is excluded:
    # its zephyr_mcumgr OTA path doesn't use this component's backend (see
    # ota_backend_factory.h) and doesn't need md5.
    components = ["safe_mode"]
    if not CORE.is_nrf52:
        components.append("md5")
    if CORE.is_esp32:
        components.extend(["watchdog"])
    return components


IS_PLATFORM_COMPONENT = True

CONF_ON_ABORT = "on_abort"
CONF_ON_BEGIN = "on_begin"
CONF_ON_END = "on_end"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_STATE_CHANGE = "on_state_change"


ota_ns = cg.esphome_ns.namespace("ota")
OTAComponent = ota_ns.class_("OTAComponent", cg.Component)
OTAState = ota_ns.enum("OTAState")
OTAAbortTrigger = ota_ns.class_("OTAAbortTrigger", automation.Trigger.template())
OTAEndTrigger = ota_ns.class_("OTAEndTrigger", automation.Trigger.template())
OTAErrorTrigger = ota_ns.class_("OTAErrorTrigger", automation.Trigger.template())
OTAProgressTrigger = ota_ns.class_("OTAProgressTrigger", automation.Trigger.template())
OTAStartTrigger = ota_ns.class_("OTAStartTrigger", automation.Trigger.template())
OTAStateChangeTrigger = ota_ns.class_(
    "OTAStateChangeTrigger", automation.Trigger.template()
)


def _ota_final_validate(config):
    if len(config) < 1:
        raise cv.Invalid(
            f"At least one platform must be specified for '{CONF_OTA}'; add '{CONF_PLATFORM}: {CONF_ESPHOME}' for original OTA functionality"
        )
    # CORE.is_zephyr is the `platform: zephyr` variant dispatch (native_sim, esp32_h2, esp32_c6, ...);
    # it excludes nrf52, which also builds on Zephyr but validates its own bootloader separately.
    if CORE.is_zephyr:
        from esphome.components.zephyr import (  # noqa: PLC0415
            ZEPHYR_VARIANT_NATIVE_SIM,
            zephyr_data,
            zephyr_variant,
        )
        from esphome.components.zephyr.const import (  # noqa: PLC0415
            BOOTLOADER_MCUBOOT,
            KEY_BOOTLOADER,
        )

        if zephyr_variant() != ZEPHYR_VARIANT_NATIVE_SIM:
            bootloader = zephyr_data()[KEY_BOOTLOADER]
            if bootloader != BOOTLOADER_MCUBOOT:
                raise cv.Invalid(f"'{bootloader}' bootloader does not support OTA")


FINAL_VALIDATE_SCHEMA = _ota_final_validate

BASE_OTA_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_STATE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStateChangeTrigger),
            }
        ),
        cv.Optional(CONF_ON_ABORT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAAbortTrigger),
            }
        ),
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAEndTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAProgressTrigger),
            }
        ),
    }
)


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config):
    cg.add_define("USE_OTA")
    CORE.add_job(final_step)

    if CORE.is_rp2 and CORE.using_arduino:
        cg.add_library("Updater", None)

    if CORE.is_zephyr:
        from esphome.components.zephyr import (  # noqa: PLC0415
            ZEPHYR_VARIANT_NATIVE_SIM,
            zephyr_add_prj_conf,
            zephyr_add_sysbuild_conf,
            zephyr_variant,
        )

        if zephyr_variant() != ZEPHYR_VARIANT_NATIVE_SIM:
            # Real Zephyr hardware: streams the image into MCUboot's secondary flash slot.
            # Same Kconfig chain zephyr_mcumgr/ota/__init__.py enables for its own OTA path.
            zephyr_add_prj_conf("STREAM_FLASH", True)
            zephyr_add_prj_conf("FLASH_MAP", True)
            zephyr_add_prj_conf("FLASH", True)
            zephyr_add_prj_conf("IMG_MANAGER", True)
            zephyr_add_prj_conf("IMG_ERASE_PROGRESSIVELY", True)
            zephyr_add_prj_conf("BOOTLOADER_MCUBOOT", True)
            zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
            # Only confirm the new image after a verified-good boot (safe_mode does the
            # confirming); see safe_mode.cpp's USE_ZEPHYR branch of mark_successful().
            cg.add_define("USE_OTA_ROLLBACK")


async def ota_to_code(var, config):
    await cg.past_safe_mode()
    use_state_callback = False
    for conf in config.get(CONF_ON_STATE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(OTAState, "state")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_ABORT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_BEGIN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_PROGRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)
        use_state_callback = True
    if use_state_callback:
        request_ota_state_listeners()


def request_ota_state_listeners() -> None:
    """Request that OTA state listeners be compiled in.

    Components that need to be notified about OTA state changes (start, progress,
    complete, error) should call this function during their code generation.
    This enables the add_state_listener() API on OTAComponent.
    """
    CORE.data[OTA_STATE_LISTENER_KEY] = True


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step():
    """Final code generation step to configure optional OTA features."""
    if CORE.data.get(OTA_STATE_LISTENER_KEY, False):
        cg.add_define("USE_OTA_STATE_LISTENER")


FILTER_SOURCE_FILES = filter_source_files_from_platform(
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
        "ota_backend_zephyr.cpp": {PlatformFramework.ZEPHYR_ZEPHYR},
    }
)
