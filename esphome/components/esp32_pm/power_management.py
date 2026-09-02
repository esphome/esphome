import logging

import esphome.codegen as cg
from esphome.components import esp32, power_management
from esphome.components.esp32 import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPENTHREAD, CONF_PLATFORM
from esphome.core import CORE, TimePeriodMilliseconds
import esphome.final_validate as fv

from .const import (
    CONF_ENABLE_LIGHT_SLEEP,
    CONF_IDLE_TIME_BEFORE_SLEEP,
    CONF_MAX_FREQUENCY,
    CONF_MIN_FREQUENCY,
    CONF_POWER_DOWN_FLASH,
    CONF_POWER_DOWN_PERIPHERALS,
    CONF_PROFILING,
    CONF_TRACE,
    CONF_ZIGBEE,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@rwrozelle"]
DEPENDENCIES = ["esp32"]

# Variants with a TOP power domain, i.e. those that support powering down
# peripherals during light sleep.
_TOP_PD_VARIANTS = [
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H21,
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S31,
]

esp32_pm_ns = cg.esphome_ns.namespace("esp32_pm")
PowerManagement = esp32_pm_ns.class_(
    "ESP32PowerManagement", power_management.PowerManagementComponent
)


def _validate_frequencies(config):
    if (max_freq := config.get(CONF_MAX_FREQUENCY)) is not None:
        variant = get_esp32_variant()
        valid_freqs = esp32.CPU_FREQUENCIES[variant]
        freq_str = f"{max_freq // 1000000}MHZ"
        if freq_str not in valid_freqs:
            raise cv.Invalid(
                f"{CONF_MAX_FREQUENCY}: {freq_str} is not a valid CPU frequency for "
                f"{variant}. Valid options are: {', '.join(valid_freqs)}"
            )
    min_freq = config.get(CONF_MIN_FREQUENCY)
    if max_freq is not None and min_freq is not None and min_freq > max_freq:
        raise cv.Invalid(
            f"{CONF_MIN_FREQUENCY} must not be greater than {CONF_MAX_FREQUENCY}"
        )
    return config


def _validate_power_down(config):
    if CONF_POWER_DOWN_PERIPHERALS not in config:
        # figure out a default
        light_sleep = config.get(CONF_ENABLE_LIGHT_SLEEP, False)
        if light_sleep:
            variant = get_esp32_variant()
            # esp32, s2, s3, c3, c2 — no TOP_PD
            config[CONF_POWER_DOWN_PERIPHERALS] = variant in _TOP_PD_VARIANTS
        else:
            config[CONF_POWER_DOWN_PERIPHERALS] = False
    # If it IS in config, user set it explicitly — leave it alone
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PowerManagement),
            cv.Optional(CONF_MAX_FREQUENCY): cv.All(cv.frequency, cv.int_),
            cv.Optional(CONF_MIN_FREQUENCY): cv.All(
                cv.frequency, cv.int_range(min=10000000)
            ),
            cv.Optional(CONF_ENABLE_LIGHT_SLEEP): cv.boolean,
            cv.Optional(CONF_IDLE_TIME_BEFORE_SLEEP): cv.int_range(
                min=2, max=4294967295
            ),
            cv.Optional(CONF_POWER_DOWN_PERIPHERALS): cv.boolean,
            cv.Optional(
                CONF_POWER_DOWN_FLASH, visibility=cv.Visibility.ADVANCED
            ): cv.boolean,
            cv.Optional(CONF_PROFILING): cv.boolean,
            cv.Optional(CONF_TRACE, visibility=cv.Visibility.ADVANCED): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(esp_idf=cv.Version(0, 0, 0)),
    _validate_frequencies,
    _validate_power_down,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await power_management.register_power_management(var, config)
    add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)

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
        add_idf_sdkconfig_option("CONFIG_ESP_PHY_MAC_BB_PD", True)
        if config.get(CONF_POWER_DOWN_PERIPHERALS):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option(
                "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP", True
            )
        if config.get(CONF_POWER_DOWN_FLASH):
            # There is a defined set of peripheral's that work with PM
            add_idf_sdkconfig_option("CONFIG_ESP_SLEEP_POWER_DOWN_FLASH", True)
        add_idf_sdkconfig_option(
            "CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP",
            config.get(CONF_IDLE_TIME_BEFORE_SLEEP, 3),
        )

        if (ot_conf := CORE.config.get(CONF_OPENTHREAD)) is not None:
            from esphome.components.openthread.const import (
                CONF_DEVICE_TYPE,
                CONF_POLL_PERIOD,
            )

            if (
                ot_conf.get(CONF_DEVICE_TYPE) == "MTD"
                and (poll_period := ot_conf.get(CONF_POLL_PERIOD)) is not None
                and poll_period > TimePeriodMilliseconds(milliseconds=0)
            ):
                add_idf_sdkconfig_option("CONFIG_LWIP_ND6", False)

        if CORE.config.get(CONF_OPENTHREAD) or CORE.config.get(CONF_ZIGBEE):
            add_idf_sdkconfig_option("CONFIG_IEEE802154_SLEEP_ENABLE", True)


def _pm_final_validate(config):
    full_config = fv.full_config.get()
    pm_entries = full_config.get("power_management", [])
    if sum(1 for entry in pm_entries if entry.get(CONF_PLATFORM) == "esp32_pm") > 1:
        raise cv.Invalid("Only one esp32_pm instance is allowed")
    if (
        (max_freq := config.get(CONF_MAX_FREQUENCY)) is not None
        and (esp32_conf := full_config.get("esp32")) is not None
        and (boot_freq := esp32_conf.get(esp32.CONF_CPU_FREQUENCY)) is not None
        and (boot_mhz := int(boot_freq[:-3])) != (max_mhz := max_freq // 1000000)
    ):
        if boot_mhz > max_mhz:
            _LOGGER.warning(
                "esp32.%s (%s) is higher than %s (%dMHZ); the CPU will be "
                "downclocked to %dMHZ as soon as power management is set up",
                esp32.CONF_CPU_FREQUENCY,
                boot_freq,
                CONF_MAX_FREQUENCY,
                max_mhz,
                max_mhz,
            )
        else:
            _LOGGER.warning(
                "esp32.%s (%s) is lower than %s (%dMHZ); the CPU may run as "
                "high as %dMHZ under load once power management is set up",
                esp32.CONF_CPU_FREQUENCY,
                boot_freq,
                CONF_MAX_FREQUENCY,
                max_mhz,
                max_mhz,
            )
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
        if CONF_IDLE_TIME_BEFORE_SLEEP in config:
            raise cv.Invalid(
                f"{CONF_IDLE_TIME_BEFORE_SLEEP} not allowed when {CONF_ENABLE_LIGHT_SLEEP} not set to True"
            )

    if pdp := config.get(CONF_POWER_DOWN_PERIPHERALS):
        esp32.only_on_variant(
            supported=_TOP_PD_VARIANTS,
            msg_prefix="Power Down Peripherals",
        )(pdp)


FINAL_VALIDATE_SCHEMA = _pm_final_validate
