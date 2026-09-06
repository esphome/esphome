from esphome import automation
import esphome.codegen as cg
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
    CONF_TRIGGER_ID,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

OTA_STATE_LISTENER_KEY = "ota_state_listener"

CODEOWNERS = ["@esphome/core"]


def AUTO_LOAD() -> list[str]:
    # Every backend computes an MD5 over the transferred image, except: nrf52 (its
    # zephyr_mcumgr OTA path doesn't use this component's backend at all) and zephyr
    # (ota_backend_zephyr.cpp uses SHA256 instead -- NCS's PSA crypto drivers can't do MD5).
    # The hash component is loaded here rather than by each ota platform: the backend
    # header lands in every platform's build through ota_backend_factory.h, so a platform
    # that doesn't happen to request sha256 itself would otherwise fail to compile.
    components = ["safe_mode"]
    if CORE.is_zephyr:
        components.append("sha256")
    elif not CORE.is_nrf52:
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
CONF_SWAP_METHOD = "swap_method"

# Shared by every ota: platform that can run on Zephyr (platform: esphome and
# platform: zephyr_mcumgr both end up pushing the very same MCUboot sysbuild
# MCUBOOT_MODE_SWAP_* Kconfig choice symbol -- see
# esphome.components.zephyr.mcuboot). Defined here, not in the zephyr package,
# so that platform: esphome -- built on every platform, not just Zephyr -- can
# use it without importing esphome.components.zephyr at module-load time.
SWAP_METHOD_SCHEMA = {
    cv.SplitDefault(
        CONF_SWAP_METHOD,
        zephyr="offset",
        # esp32-family zephyr variants only support scratch/move, not offset --
        # explicit here to keep today's behavior (they had no override before,
        # so they picked up the old generic zephyr="scratch" default).
        zephyr_esp32="scratch",
        zephyr_esp32h2="scratch",
        zephyr_esp32c6="scratch",
        zephyr_esp32c5="scratch",
        zephyr_esp32c3="scratch",
        zephyr_nrf52="move",
        zephyr_nrf54l15="move",
        zephyr_nrf54lm20a="move",
    ): cv.one_of("scratch", "move", "offset", "direct", lower=True),
}


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


def _ota_final_validate(config: ConfigType) -> None:
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
            KEY_SINGLE_SLOT,
        )

        if zephyr_variant() != ZEPHYR_VARIANT_NATIVE_SIM:
            bootloader = zephyr_data()[KEY_BOOTLOADER]
            if bootloader != BOOTLOADER_MCUBOOT:
                raise cv.Invalid(f"'{bootloader}' bootloader does not support OTA")

        if zephyr_data()[KEY_SINGLE_SLOT]:
            # No secondary slot to write into instead of the one currently executing --
            # live OTA (any platform) risks corrupting the running image mid-transfer,
            # not just losing revert-on-power-loss safety. Serial/wired flashing only.
            raise cv.Invalid(
                f"'{CONF_OTA}:' is not supported with 'single_slot: true' -- there is no "
                f"secondary slot for OTA to write into, and writing into the slot "
                f"currently executing risks crashing mid-update. Flash over serial/USB "
                f"instead."
            )

        # platform: esphome and platform: zephyr_mcumgr can both be configured at
        # once, each with its own swap_method:. They're validated independently,
        # but both push the same MCUboot sysbuild swap-mode choice symbol -- a
        # mismatch would otherwise pass validation and silently write two
        # different, mutually-exclusive symbols =y into sysbuild.conf.
        methods = {
            ota_conf[CONF_PLATFORM]: ota_conf[CONF_SWAP_METHOD]
            for ota_conf in config
            if CONF_SWAP_METHOD in ota_conf
        }
        if len(set(methods.values())) > 1:
            raise cv.Invalid(
                f"'{CONF_SWAP_METHOD}:' must be the same across every '{CONF_OTA}:' "
                f"entry, got {methods}"
            )


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
async def to_code(config: ConfigType) -> None:
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


async def ota_to_code(var: MockObj, config: ConfigType) -> None:
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
        "ota_backend_zephyr.cpp": {PlatformFramework.ZEPHYR_ZEPHYR},
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
