from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import (
    CONF_LOCK_TYPE,
    CONF_MAX_FREQUENCY,
    CONF_MIN_FREQUENCY,
    CONF_POWER_DOWN_FLASH,
    CONF_POWER_DOWN_PERIPHERALS,
    CONF_PROFILING,
    CONF_TICKLESS_IDLE,
    CONF_TIMER_LOCK_DURATION,
    CONF_TRACE,
)

CODEOWNERS = ["@rwrozelle"]
power_management_ns = cg.esphome_ns.namespace("power_management")
PowerManagement = power_management_ns.class_("PowerManagement", cg.Component)

AcquireLockAction = power_management_ns.class_("AcquireLockAction", automation.Action)
ReleaseLockAction = power_management_ns.class_("ReleaseLockAction", automation.Action)

PM_ACTION_ACQUIRE_SCHEMA = automation.maybe_conf(
    CONF_LOCK_TYPE,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PowerManagement),
            cv.Optional(CONF_LOCK_TYPE, default="CPU"): cv.one_of(
                "TMR", "CPU", "APB", "SLP", upper=True
            ),
            cv.Optional(
                CONF_TIMER_LOCK_DURATION, default="0ms"
            ): cv.positive_time_period_milliseconds,
        }
    ),
)
PM_ACTION_RELEASE_SCHEMA = automation.maybe_conf(
    CONF_LOCK_TYPE,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PowerManagement),
            cv.Optional(CONF_LOCK_TYPE, default="CPU"): cv.one_of(
                "CPU", "APB", "SLP", upper=True
            ),
        }
    ),
)


@automation.register_action(
    "power_management.acquire_lock", AcquireLockAction, PM_ACTION_ACQUIRE_SCHEMA
)
@automation.register_action(
    "power_management.release_lock", ReleaseLockAction, PM_ACTION_RELEASE_SCHEMA
)
async def power_management_lock_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if (lock_type := config.get(CONF_LOCK_TYPE)) is not None:
        cg.add(var.set_lock_type(lock_type))
    if (timer_lock_duration := config.get(CONF_TIMER_LOCK_DURATION)) is not None:
        cg.add(var.set_timer_lock_duration(timer_lock_duration))
    return var


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PowerManagement),
            cv.Optional(CONF_TIMER_LOCK_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_FREQUENCY): cv.All(cv.frequency, cv.int_),
            cv.Optional(CONF_MIN_FREQUENCY): cv.All(cv.frequency, cv.int_),
            cv.Optional(CONF_POWER_DOWN_PERIPHERALS): cv.boolean,
            cv.Optional(CONF_POWER_DOWN_FLASH): cv.boolean,
            cv.Optional(CONF_PROFILING): cv.boolean,
            cv.Optional(CONF_TICKLESS_IDLE): cv.boolean,
            cv.Optional(CONF_TRACE): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_POWER_MANAGEMENT", True)
    add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)

    if (timer_lock_duration := config.get(CONF_TIMER_LOCK_DURATION)) is not None:
        cg.add(var.set_timer_lock_duration(timer_lock_duration))
    if (max_freq := config.get(CONF_MAX_FREQUENCY)) is not None:
        cg.add(var.set_max_freq_mhz(max_freq))
    if (min_freq := config.get(CONF_MIN_FREQUENCY)) is not None:
        cg.add(var.set_min_freq_mhz(min_freq))
    if config.get(CONF_PROFILING):
        add_idf_sdkconfig_option("CONFIG_PM_PROFILING", True)
    if config.get(CONF_TRACE):
        add_idf_sdkconfig_option("CONFIG_PM_TRACE", True)
    if config.get(CONF_TICKLESS_IDLE):
        # this causes automatic light sleep if no tasks are pending
        add_idf_sdkconfig_option("CONFIG_FREERTOS_USE_TICKLESS_IDLE", True)
        # TBD move into a finalize that looks for openthread configuration
        add_idf_sdkconfig_option("CONFIG_IEEE802154_SLEEP_ENABLE", True)
        if config.get(CONF_POWER_DOWN_PERIPHERALS):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option(
                "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP", True
            )
        if config.get(CONF_POWER_DOWN_FLASH):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option("CONFIG_ESP_SLEEP_POWER_DOWN_FLASH", True)
