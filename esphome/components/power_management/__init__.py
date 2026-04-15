from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    # VARIANT_ESP32H21,
    # VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    add_idf_sdkconfig_option,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv

from .const import (
    CONF_ENABLE_LIGHT_SLEEP,
    CONF_ESPHOME_LOCKS,
    CONF_IDLE_TIME_BEFORE_SLEEP,
    CONF_LOCK_TYPE,
    CONF_MAX_FREQUENCY,
    CONF_MIN_FREQUENCY,
    CONF_POWER_DOWN_FLASH,
    CONF_POWER_DOWN_PERIPHERALS,
    CONF_POWER_MANAGEMENT,
    CONF_PROFILING,
    CONF_TRACE,
)

CODEOWNERS = ["@rwrozelle"]
power_management_ns = cg.esphome_ns.namespace(CONF_POWER_MANAGEMENT)
PowerManagement = power_management_ns.class_("PowerManagement", cg.Component)

AcquireLockAction = power_management_ns.class_("AcquireLockAction", automation.Action)
ReleaseLockAction = power_management_ns.class_("ReleaseLockAction", automation.Action)

PM_ACTION_SCHEMA = automation.maybe_conf(
    CONF_LOCK_TYPE,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PowerManagement),
            cv.Optional(CONF_LOCK_TYPE, default="SLP"): cv.one_of(
                "CPU", "APB", "SLP", upper=True
            ),
        }
    ),
)


@automation.register_action(
    "power_management.acquire_lock",
    AcquireLockAction,
    PM_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "power_management.release_lock",
    ReleaseLockAction,
    PM_ACTION_SCHEMA,
    synchronous=True,
)
async def power_management_lock_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if (lock_type := config.get(CONF_LOCK_TYPE)) is not None:
        cg.add(var.set_lock_type(lock_type))
    return var


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PowerManagement),
            cv.Optional(CONF_MAX_FREQUENCY): cv.All(
                cv.frequency, cv.int_range(min=40000000)
            ),
            cv.Optional(CONF_MIN_FREQUENCY): cv.All(
                cv.frequency, cv.int_range(min=10000000)
            ),
            cv.Optional(CONF_ENABLE_LIGHT_SLEEP): cv.boolean,
            cv.Optional(CONF_IDLE_TIME_BEFORE_SLEEP, default=3): cv.int_range(
                min=2, max=4294967295
            ),
            # c5,c6,c61,h2,h21,h4,p4
            cv.SplitDefault(
                CONF_POWER_DOWN_PERIPHERALS,
                esp32_c5=True,
                esp32_c6=True,
                esp32_c61=True,
                esp32_h2=True,
                # esp32_h21=True,
                # esp32_h4=True,
                esp32_p4=True,
                esp32=False,  # esp32, s2, s3, c3, c2 — no TOP_PD
            ): cv.boolean,
            cv.Optional(CONF_POWER_DOWN_FLASH): cv.boolean,
            cv.Optional(CONF_ESPHOME_LOCKS): cv.boolean,
            cv.Optional(CONF_PROFILING): cv.boolean,
            cv.Optional(CONF_TRACE): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)
    add_idf_sdkconfig_option("CONFIG_ESP_PHY_MAC_BB_PD", True)

    if config.get(CONF_ESPHOME_LOCKS):
        cg.add_define("USE_POWER_MANAGEMENT")

    if (max_freq := config.get(CONF_MAX_FREQUENCY)) is not None:
        cg.add(var.set_max_freq_mhz(max_freq // 1000000))
    if (min_freq := config.get(CONF_MIN_FREQUENCY)) is not None:
        cg.add(var.set_min_freq_mhz(min_freq // 1000000))
    if config.get(CONF_PROFILING):
        add_idf_sdkconfig_option("CONFIG_PM_PROFILING", True)
    if config.get(CONF_TRACE):
        add_idf_sdkconfig_option("CONFIG_PM_TRACE", True)
    if config.get(CONF_ENABLE_LIGHT_SLEEP):
        # this causes automatic light sleep if no tasks are pending
        add_idf_sdkconfig_option("CONFIG_FREERTOS_USE_TICKLESS_IDLE", True)
        add_idf_sdkconfig_option("CONFIG_IEEE802154_SLEEP_ENABLE", True)
        if config.get(CONF_POWER_DOWN_PERIPHERALS):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option(
                "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP", True
            )
        if config.get(CONF_POWER_DOWN_FLASH):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option("CONFIG_ESP_SLEEP_POWER_DOWN_FLASH", True)
        if (itbs := config.get(CONF_IDLE_TIME_BEFORE_SLEEP)) is not None:
            add_idf_sdkconfig_option("CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP", itbs)


def _pm_recursive_validator(value):
    if isinstance(value, dict):
        for key, item in value.items():
            if key in (
                "power_management.acquire_lock",
                "power_management.release_lock",
            ):
                raise cv.Invalid(
                    f"Action: {key} not allowed when {CONF_POWER_MANAGEMENT} {CONF_ESPHOME_LOCKS} is not true"
                )
            _pm_recursive_validator(item)
    elif isinstance(value, list):
        for item in value:
            _pm_recursive_validator(item)


def _pm_final_validate(config):
    full_config = fv.full_config.get()
    if (
        config.get(CONF_ENABLE_LIGHT_SLEEP)
        and config.get(CONF_POWER_DOWN_FLASH)
        and full_config.get("psram")
    ):
        raise cv.Invalid(
            f"{CONF_POWER_DOWN_FLASH}: True not allowed when device has PSRAM"
        )

    if not (config.get(CONF_ENABLE_LIGHT_SLEEP)):
        if config.get(CONF_POWER_DOWN_PERIPHERALS):
            raise cv.Invalid(
                f"{CONF_POWER_DOWN_PERIPHERALS}: True not allowed when {CONF_ENABLE_LIGHT_SLEEP} not set to True"
            )
        if config.get(CONF_POWER_DOWN_FLASH):
            raise cv.Invalid(
                f"{CONF_POWER_DOWN_FLASH}: True not allowed when {CONF_ENABLE_LIGHT_SLEEP} not set to True"
            )

    # c5,c6,c61,h2,h21,h4,p4
    if pdp := config.get(CONF_POWER_DOWN_PERIPHERALS):
        esp32.only_on_variant(
            supported=[
                VARIANT_ESP32C5,
                VARIANT_ESP32C6,
                VARIANT_ESP32C61,
                VARIANT_ESP32H2,
                # VARIANT_ESP32H21,
                # VARIANT_ESP32H4,
                VARIANT_ESP32P4,
            ],
            msg_prefix="Power Down Peripherials",
        )(pdp)

    if not (
        (pm_conf := full_config.get(CONF_POWER_MANAGEMENT))
        and pm_conf.get(CONF_ESPHOME_LOCKS)
    ):
        # find all actions
        _pm_recursive_validator(full_config)


FINAL_VALIDATE_SCHEMA = _pm_final_validate
