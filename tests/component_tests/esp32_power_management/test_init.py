"""Tests for esp32_power_management configuration validation and sdkconfig wiring."""

from contextlib import nullcontext
import logging
from unittest import mock

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_DEVICE_TYPE
from esphome.components.esp32 import (
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
)
from esphome.components.esp32_power_management.power_management import (
    CONF_ENABLE_LIGHT_SLEEP,
    CONF_IDLE_TIME_BEFORE_SLEEP,
    CONF_MAX_FREQUENCY,
    CONF_MIN_FREQUENCY,
    CONF_POWER_DOWN_FLASH,
    CONF_POWER_DOWN_PERIPHERALS,
    CONFIG_SCHEMA,
    _final_validate,
    to_code,
    valid_min_frequencies,
)
from esphome.components.openthread.const import CONF_POLL_PERIOD
from esphome.const import CONF_OPENTHREAD, KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.core import CORE, TimePeriodMilliseconds
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

LIGHT_SLEEP = {CONF_ENABLE_LIGHT_SLEEP: True}


def _set_variant(
    set_core_config: SetCoreConfigCallable,
    variant: str,
    /,
    *,
    full_config: ConfigType | None = None,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
        platform_data={KEY_VARIANT: variant},
        full_config=full_config,
    )


@pytest.mark.parametrize(
    ("variant", "config", "expectation"),
    [
        pytest.param(
            VARIANT_ESP32,
            {CONF_MAX_FREQUENCY: "96MHZ"},
            pytest.raises(cv.Invalid, match="not a valid CPU frequency"),
            id="max_invalid_for_variant",
        ),
        pytest.param(
            VARIANT_ESP32,
            {CONF_POWER_DOWN_PERIPHERALS: True},
            pytest.raises(cv.Invalid, match="requires enable_light_sleep"),
            id="power_down_peripherals_without_light_sleep",
        ),
        pytest.param(
            VARIANT_ESP32,
            {CONF_POWER_DOWN_FLASH: True},
            pytest.raises(cv.Invalid, match="requires enable_light_sleep"),
            id="power_down_flash_without_light_sleep",
        ),
        pytest.param(
            VARIANT_ESP32,
            LIGHT_SLEEP | {CONF_POWER_DOWN_PERIPHERALS: True},
            pytest.raises(cv.Invalid, match="only available on"),
            id="power_down_peripherals_unsupported_variant",
        ),
        pytest.param(
            VARIANT_ESP32H2,
            LIGHT_SLEEP | {CONF_POWER_DOWN_PERIPHERALS: True},
            nullcontext(),
            id="power_down_peripherals_top_pd_variant",
        ),
        # The ESP32 keeps its reference clock divider at 10 or more
        pytest.param(
            VARIANT_ESP32,
            {CONF_MIN_FREQUENCY: "8MHZ"},
            pytest.raises(cv.Invalid, match="not a valid minimum frequency"),
            id="min_frequency_below_esp32_floor",
        ),
        # 26 MHz crystal modules exist, so 13 MHz is reachable on an ESP32
        pytest.param(
            VARIANT_ESP32,
            {CONF_MIN_FREQUENCY: "13MHZ"},
            nullcontext(),
            id="min_frequency_esp32_26mhz_crystal",
        ),
        pytest.param(
            VARIANT_ESP32,
            {CONF_MIN_FREQUENCY: "80MHZ"},
            nullcontext(),
            id="min_frequency_pll",
        ),
        # The ESP32-H2 runs from a 32 MHz crystal, so 8 MHz divides it and 10 MHz does not
        pytest.param(
            VARIANT_ESP32H2,
            {CONF_MIN_FREQUENCY: "8MHZ"},
            nullcontext(),
            id="min_frequency_h2_crystal_divider",
        ),
        pytest.param(
            VARIANT_ESP32H2,
            {CONF_MIN_FREQUENCY: "10MHZ"},
            pytest.raises(cv.Invalid, match="must divide the crystal"),
            id="min_frequency_h2_not_a_divider",
        ),
        pytest.param(
            VARIANT_ESP32C6,
            {CONF_MIN_FREQUENCY: "1MHZ"},
            pytest.raises(cv.Invalid, match="not a valid minimum frequency"),
            id="min_frequency_below_reference_clock_floor",
        ),
    ],
)
def test_schema_validation(
    set_core_config: SetCoreConfigCallable,
    variant: str,
    config: ConfigType,
    expectation,
) -> None:
    _set_variant(set_core_config, variant)
    with expectation:
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    ("variant", "expected"),
    [
        pytest.param(
            VARIANT_ESP32, [10, 12, 13, 20, 24, 26, 40, 80, 160, 240], id="esp32"
        ),
        pytest.param(VARIANT_ESP32H2, [2, 4, 8, 16, 32, 48, 64, 96], id="esp32h2"),
        pytest.param(
            VARIANT_ESP32C6, [2, 4, 5, 8, 10, 20, 40, 80, 120, 160], id="esp32c6"
        ),
    ],
)
def test_valid_min_frequencies(variant: str, expected: list[int]) -> None:
    assert valid_min_frequencies(variant) == expected


def test_frequencies_are_kept_in_mhz(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32)
    config = CONFIG_SCHEMA({CONF_MAX_FREQUENCY: "160MHZ", CONF_MIN_FREQUENCY: 40e6})
    assert config[CONF_MAX_FREQUENCY] == 160
    assert config[CONF_MIN_FREQUENCY] == 40
    assert config[CONF_IDLE_TIME_BEFORE_SLEEP] == 3


@pytest.mark.parametrize(
    ("variant", "config", "expected"),
    [
        pytest.param(VARIANT_ESP32H2, LIGHT_SLEEP, True, id="top_pd_light_sleep"),
        pytest.param(VARIANT_ESP32, LIGHT_SLEEP, False, id="no_top_pd_light_sleep"),
        pytest.param(VARIANT_ESP32H2, {}, False, id="no_light_sleep"),
        pytest.param(
            VARIANT_ESP32H2,
            LIGHT_SLEEP | {CONF_POWER_DOWN_PERIPHERALS: False},
            False,
            id="explicit_value_kept",
        ),
    ],
)
def test_power_down_peripherals_default(
    set_core_config: SetCoreConfigCallable,
    variant: str,
    config: ConfigType,
    expected: bool,
) -> None:
    _set_variant(set_core_config, variant)
    assert CONFIG_SCHEMA(config)[CONF_POWER_DOWN_PERIPHERALS] is expected


def _final_validate_config(
    set_core_config: SetCoreConfigCallable,
    config: ConfigType,
    full_config: ConfigType | None = None,
    boot_frequency: str = "160MHZ",
) -> None:
    """Validate config, then run _final_validate against the given full config."""
    full_config = {"esp32": {"cpu_frequency": boot_frequency}} | (full_config or {})
    _set_variant(set_core_config, VARIANT_ESP32, full_config=full_config)
    _final_validate(CONFIG_SCHEMA(config))


def test_rejects_multiple_esp32_power_management_instances(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="Only one esp32_power_management instance"):
        _final_validate_config(
            set_core_config,
            {},
            {"power_management": [{"platform": "esp32_power_management"}] * 2},
        )


@pytest.mark.parametrize(
    ("max_frequency", "boot_frequency", "expected"),
    [
        pytest.param("160MHZ", "240MHZ", "downclocked to 160MHZ", id="boot_higher"),
        pytest.param("240MHZ", "80MHZ", "may run as high as 240MHZ", id="boot_lower"),
        pytest.param("160MHZ", "160MHZ", "", id="equal"),
    ],
)
def test_boot_frequency_log(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
    max_frequency: str,
    boot_frequency: str,
    expected: str,
) -> None:
    caplog.set_level(logging.INFO)
    _final_validate_config(
        set_core_config,
        {CONF_MAX_FREQUENCY: max_frequency},
        boot_frequency=boot_frequency,
    )
    assert expected in caplog.text
    assert (caplog.text == "") is (expected == "")


@pytest.mark.parametrize(
    ("config", "boot_frequency", "expectation"),
    [
        pytest.param(
            {CONF_MAX_FREQUENCY: "80MHZ", CONF_MIN_FREQUENCY: "160MHZ"},
            "240MHZ",
            pytest.raises(cv.Invalid, match="must not be greater than"),
            id="min_above_max",
        ),
        pytest.param(
            {CONF_MIN_FREQUENCY: "160MHZ"},
            "80MHZ",
            pytest.raises(cv.Invalid, match="must not be greater than"),
            id="min_above_boot_frequency",
        ),
        pytest.param(
            {CONF_MIN_FREQUENCY: "80MHZ"},
            "80MHZ",
            nullcontext(),
            id="min_equal_to_boot_frequency",
        ),
    ],
)
def test_min_frequency_against_effective_max(
    set_core_config: SetCoreConfigCallable,
    config: ConfigType,
    boot_frequency: str,
    expectation,
) -> None:
    with expectation:
        _final_validate_config(set_core_config, config, boot_frequency=boot_frequency)


def test_rejects_power_down_flash_with_psram(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="not allowed when the device has PSRAM"):
        _final_validate_config(
            set_core_config,
            LIGHT_SLEEP | {CONF_POWER_DOWN_FLASH: True},
            {"psram": {"mode": "quad"}},
        )


@pytest.fixture
def sdkconfig_calls() -> dict[str, object]:
    """Run to_code() without codegen and collect the sdkconfig options it sets."""
    calls: dict[str, object] = {}
    with (
        mock.patch(
            "esphome.components.esp32_power_management.power_management.cg.new_Pvariable",
            return_value=mock.MagicMock(),
        ),
        mock.patch(
            "esphome.components.esp32_power_management.power_management.power_management."
            "register_power_management",
            new_callable=mock.AsyncMock,
        ),
        mock.patch(
            "esphome.components.esp32_power_management.power_management.add_idf_sdkconfig_option",
            side_effect=calls.__setitem__,
        ),
    ):
        yield calls


SLEEPY_MTD = {
    CONF_OPENTHREAD: {
        CONF_DEVICE_TYPE: "MTD",
        CONF_POLL_PERIOD: TimePeriodMilliseconds(milliseconds=1000),
    }
}
NOT_SET = object()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("core_config", "light_sleep", "key", "expected"),
    [
        pytest.param(SLEEPY_MTD, True, "CONFIG_LWIP_ND6", False, id="nd6_sleepy_mtd"),
        pytest.param(
            {CONF_OPENTHREAD: {CONF_DEVICE_TYPE: "FTD"}},
            True,
            "CONFIG_LWIP_ND6",
            NOT_SET,
            id="nd6_ftd",
        ),
        pytest.param({}, True, "CONFIG_LWIP_ND6", NOT_SET, id="nd6_no_openthread"),
        pytest.param(
            {CONF_OPENTHREAD: {CONF_DEVICE_TYPE: "FTD"}},
            True,
            "CONFIG_IEEE802154_SLEEP_ENABLE",
            True,
            id="radio_sleep_openthread",
        ),
        pytest.param(
            {"zigbee": {"id": "zigbee_id"}},
            True,
            "CONFIG_IEEE802154_SLEEP_ENABLE",
            True,
            id="radio_sleep_zigbee",
        ),
        pytest.param(
            {}, True, "CONFIG_IEEE802154_SLEEP_ENABLE", NOT_SET, id="radio_sleep_none"
        ),
        pytest.param(
            SLEEPY_MTD, False, "CONFIG_LWIP_ND6", NOT_SET, id="nd6_light_sleep_off"
        ),
        pytest.param(
            SLEEPY_MTD,
            False,
            "CONFIG_IEEE802154_SLEEP_ENABLE",
            NOT_SET,
            id="radio_sleep_light_sleep_off",
        ),
    ],
)
async def test_to_code_sdkconfig(
    set_core_config: SetCoreConfigCallable,
    sdkconfig_calls: dict[str, object],
    core_config: ConfigType,
    light_sleep: bool,
    key: str,
    expected: object,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32H2)
    CORE.config = core_config
    await to_code(CONFIG_SCHEMA({CONF_ENABLE_LIGHT_SLEEP: light_sleep}))
    assert sdkconfig_calls.get(key, NOT_SET) is expected
