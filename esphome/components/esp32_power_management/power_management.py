import logging

import esphome.codegen as cg
from esphome.components import esp32, power_management
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.openthread.const import CONF_POLL_PERIOD
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
from esphome.components.zigbee import DOMAIN as ZIGBEE_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPENTHREAD
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@rwrozelle"]
DEPENDENCIES = ["esp32"]

CONF_ENABLE_LIGHT_SLEEP = "enable_light_sleep"
CONF_IDLE_TIME_BEFORE_SLEEP = "idle_time_before_sleep"
CONF_MAX_FREQUENCY = "max_frequency"
CONF_MIN_FREQUENCY = "min_frequency"
CONF_POWER_DOWN_FLASH = "power_down_flash"
CONF_POWER_DOWN_PERIPHERALS = "power_down_peripherals"
CONF_PROFILING = "profiling"
CONF_TRACE = "trace"

# Variants with a TOP power domain, which can power down peripherals in light sleep.
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

# Crystal frequencies each variant can be built for (ESP-IDF's SOC_XTAL_FREQ_* values).
# Below the PLL frequencies the CPU runs from the crystal through an integer divider.
_XTAL_MHZ = {
    VARIANT_ESP32: (24, 26, 40),
    VARIANT_ESP32C2: (26, 32, 40),
    VARIANT_ESP32C3: (32, 40),
    VARIANT_ESP32C5: (40, 48),
    VARIANT_ESP32C6: (40,),
    VARIANT_ESP32C61: (40,),
    VARIANT_ESP32H2: (32,),
    VARIANT_ESP32H21: (32,),
    VARIANT_ESP32H4: (32,),
    VARIANT_ESP32P4: (40,),
    VARIANT_ESP32S2: (40,),
    VARIANT_ESP32S3: (32, 40),
    VARIANT_ESP32S31: (40,),
}
# esp_pm_configure() keeps the 1 MHz reference clock reachable: its divider must be
# at least 10 on the ESP32 and 2 on every other variant.
_MIN_FREQUENCY_FLOOR_MHZ = {VARIANT_ESP32: 10}

esp32_power_management_ns = cg.esphome_ns.namespace("esp32_power_management")
PowerManagement = esp32_power_management_ns.class_(
    "ESP32PowerManagement", power_management.PowerManagementComponent
)


def _mhz(value: object) -> int:
    """Validate a frequency and keep it as whole megahertz."""
    return int(cv.frequency(value)) // 1_000_000


def valid_min_frequencies(variant: str) -> list[int]:
    """The minimum CPU frequencies esp_pm_configure() accepts on a variant, in MHz.

    A value must divide one of the variant's crystal frequencies, stay above the
    reference clock floor, or be one of the PLL frequencies.
    """
    floor = _MIN_FREQUENCY_FLOOR_MHZ.get(variant, 2)
    mhz = {
        xtal // divider
        for xtal in _XTAL_MHZ[variant]
        for divider in range(1, xtal // floor + 1)
        if xtal % divider == 0
    }
    mhz.update(int(freq[:-3]) for freq in esp32.CPU_FREQUENCIES[variant])
    return sorted(mhz)


def _validate(config: ConfigType) -> ConfigType:
    variant = get_esp32_variant()
    if (max_mhz := config.get(CONF_MAX_FREQUENCY)) is not None:
        valid = esp32.CPU_FREQUENCIES[variant]
        if f"{max_mhz}MHZ" not in valid:
            raise cv.Invalid(
                f"{max_mhz}MHZ is not a valid CPU frequency for {variant}. "
                f"Valid options are: {', '.join(valid)}",
                path=[CONF_MAX_FREQUENCY],
            )
    if (min_mhz := config.get(CONF_MIN_FREQUENCY)) is not None and (
        min_mhz not in (valid_min := valid_min_frequencies(variant))
    ):
        raise cv.Invalid(
            f"{min_mhz}MHZ is not a valid minimum frequency for {variant}; it must "
            f"divide the crystal frequency. Valid options are: "
            f"{', '.join(f'{mhz}MHZ' for mhz in valid_min)}",
            path=[CONF_MIN_FREQUENCY],
        )
    if not config[CONF_ENABLE_LIGHT_SLEEP]:
        for key in (CONF_POWER_DOWN_PERIPHERALS, CONF_POWER_DOWN_FLASH):
            if config.get(key):
                raise cv.Invalid(
                    f"{key} requires {CONF_ENABLE_LIGHT_SLEEP}: true", path=[key]
                )
        config[CONF_POWER_DOWN_PERIPHERALS] = False
    elif CONF_POWER_DOWN_PERIPHERALS not in config:
        config[CONF_POWER_DOWN_PERIPHERALS] = variant in _TOP_PD_VARIANTS
    elif config[CONF_POWER_DOWN_PERIPHERALS] and variant not in _TOP_PD_VARIANTS:
        raise cv.Invalid(
            f"{CONF_POWER_DOWN_PERIPHERALS} is only available on "
            f"{', '.join(_TOP_PD_VARIANTS)}",
            path=[CONF_POWER_DOWN_PERIPHERALS],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PowerManagement),
            cv.Optional(CONF_MAX_FREQUENCY): _mhz,
            cv.Optional(CONF_MIN_FREQUENCY): _mhz,
            cv.Optional(CONF_ENABLE_LIGHT_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_IDLE_TIME_BEFORE_SLEEP, default=3): cv.int_range(
                min=2, max=4294967295
            ),
            cv.Optional(CONF_POWER_DOWN_PERIPHERALS): cv.boolean,
            cv.Optional(CONF_POWER_DOWN_FLASH, default=False): cv.boolean,
            cv.Optional(CONF_PROFILING, default=False): cv.boolean,
            # Repurposes GPIOs as trace outputs, so keep it out of the visual editor.
            cv.Optional(
                CONF_TRACE, default=False, visibility=cv.Visibility.YAML_ONLY
            ): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await power_management.register_power_management(var, config)
    add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)

    cg.add_optional(var.set_max_freq_mhz, config.get(CONF_MAX_FREQUENCY))
    cg.add_optional(var.set_min_freq_mhz, config.get(CONF_MIN_FREQUENCY))
    if config[CONF_PROFILING]:
        add_idf_sdkconfig_option("CONFIG_PM_PROFILING", True)
    if config[CONF_TRACE]:
        add_idf_sdkconfig_option("CONFIG_PM_TRACE", True)
    if not config[CONF_ENABLE_LIGHT_SLEEP]:
        return

    # Automatic light sleep whenever no task is runnable.
    add_idf_sdkconfig_option("CONFIG_FREERTOS_USE_TICKLESS_IDLE", True)
    add_idf_sdkconfig_option("CONFIG_ESP_PHY_MAC_BB_PD", True)
    # Kconfig's own "depends on" no-ops this where it doesn't apply.
    add_idf_sdkconfig_option("CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION", True)
    add_idf_sdkconfig_option(
        "CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP", config[CONF_IDLE_TIME_BEFORE_SLEEP]
    )
    if config[CONF_POWER_DOWN_PERIPHERALS]:
        add_idf_sdkconfig_option("CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP", True)
    if config[CONF_POWER_DOWN_FLASH]:
        add_idf_sdkconfig_option("CONFIG_ESP_SLEEP_POWER_DOWN_FLASH", True)

    ot_conf = CORE.config.get(CONF_OPENTHREAD)
    # A polling (sleepy) Thread end device cannot answer neighbour solicitations.
    if (
        ot_conf is not None
        and (poll_period := ot_conf.get(CONF_POLL_PERIOD)) is not None
        and poll_period.total_milliseconds > 0
    ):
        add_idf_sdkconfig_option("CONFIG_LWIP_ND6", False)
    if ot_conf is not None or ZIGBEE_DOMAIN in CORE.config:
        add_idf_sdkconfig_option("CONFIG_IEEE802154_SLEEP_ENABLE", True)


_validate_single_instance = fv.unique_platform_key(
    power_management.DOMAIN, DOMAIN, message=f"Only one {DOMAIN} instance is allowed"
)


def _final_validate(config: ConfigType) -> ConfigType:
    _validate_single_instance(config)
    full_config = fv.full_config.get()
    boot_freq = full_config[esp32.DOMAIN][esp32.CONF_CPU_FREQUENCY]
    boot_mhz = int(boot_freq[:-3])
    max_mhz = config.get(CONF_MAX_FREQUENCY)
    # Without max_frequency the boot frequency is the ceiling esp_pm_configure() gets.
    if (min_mhz := config.get(CONF_MIN_FREQUENCY)) is not None and min_mhz > (
        max_mhz if max_mhz is not None else boot_mhz
    ):
        raise cv.Invalid(
            f"{CONF_MIN_FREQUENCY} must not be greater than the maximum frequency",
            path=[CONF_MIN_FREQUENCY],
        )
    if max_mhz is not None:
        if boot_mhz > max_mhz:
            _LOGGER.info(
                "esp32.%s (%s) is higher than %s (%dMHZ); the CPU will be "
                "downclocked to %dMHZ as soon as power management is set up",
                esp32.CONF_CPU_FREQUENCY,
                boot_freq,
                CONF_MAX_FREQUENCY,
                max_mhz,
                max_mhz,
            )
        elif boot_mhz < max_mhz:
            _LOGGER.warning(
                "esp32.%s (%s) is lower than %s (%dMHZ); the CPU may run as "
                "high as %dMHZ under load once power management is set up",
                esp32.CONF_CPU_FREQUENCY,
                boot_freq,
                CONF_MAX_FREQUENCY,
                max_mhz,
                max_mhz,
            )
    if config[CONF_POWER_DOWN_FLASH] and PSRAM_DOMAIN in full_config:
        raise cv.Invalid(
            f"{CONF_POWER_DOWN_FLASH} is not allowed when the device has PSRAM",
            path=[CONF_POWER_DOWN_FLASH],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate
