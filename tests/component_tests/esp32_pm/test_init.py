"""Tests for esp32_pm configuration validation."""

from unittest import mock

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32, VARIANT_ESP32H2
from esphome.components.esp32_pm.const import (
    CONF_ENABLE_LIGHT_SLEEP,
    CONF_IDLE_TIME_BEFORE_SLEEP,
    CONF_MAX_FREQUENCY,
    CONF_MIN_FREQUENCY,
    CONF_POWER_DOWN_FLASH,
    CONF_POWER_DOWN_PERIPHERALS,
    CONF_ZIGBEE,
)
from esphome.components.esp32_pm.power_management import (
    CONFIG_SCHEMA,
    _pm_final_validate,
    to_code,
)
from esphome.components.openthread.const import CONF_DEVICE_TYPE, CONF_POLL_PERIOD
from esphome.const import (
    CONF_ID,
    CONF_OPENTHREAD,
    KEY_FRAMEWORK_VERSION,
    PlatformFramework,
)
from esphome.core import CORE, TimePeriodMilliseconds
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _set_variant(
    set_core_config: SetCoreConfigCallable, variant: str, /, *, framework=None
) -> None:
    set_core_config(
        framework or PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
        platform_data={KEY_VARIANT: variant},
    )


# --- _validate_frequencies ---------------------------------------------------


def test_rejects_max_frequency_invalid_for_variant(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32)
    with pytest.raises(cv.Invalid, match="is not a valid CPU frequency"):
        CONFIG_SCHEMA({CONF_MAX_FREQUENCY: "96MHZ"})


def test_accepts_max_frequency_valid_for_variant(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32)
    config = CONFIG_SCHEMA({CONF_MAX_FREQUENCY: "160MHZ"})
    assert config[CONF_MAX_FREQUENCY] == 160000000


def test_rejects_min_frequency_greater_than_max_frequency(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32)
    with pytest.raises(cv.Invalid, match="must not be greater than"):
        CONFIG_SCHEMA({CONF_MAX_FREQUENCY: "80MHZ", CONF_MIN_FREQUENCY: "160MHZ"})


# --- _validate_power_down -----------------------------------------------------


@pytest.mark.parametrize(
    ("variant", "config", "expected"),
    [
        pytest.param(
            VARIANT_ESP32H2,
            {CONF_ENABLE_LIGHT_SLEEP: True},
            True,
            id="top_pd_variant_with_light_sleep",
        ),
        pytest.param(
            VARIANT_ESP32,
            {CONF_ENABLE_LIGHT_SLEEP: True},
            False,
            id="non_top_pd_variant_with_light_sleep",
        ),
        pytest.param(
            VARIANT_ESP32H2,
            {},
            False,
            id="top_pd_variant_without_light_sleep",
        ),
        pytest.param(
            VARIANT_ESP32H2,
            {CONF_ENABLE_LIGHT_SLEEP: True, CONF_POWER_DOWN_PERIPHERALS: False},
            False,
            id="explicit_value_not_overridden",
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
    result = CONFIG_SCHEMA(config)
    assert result[CONF_POWER_DOWN_PERIPHERALS] is expected


# --- CONFIG_SCHEMA framework guard --------------------------------------------


def test_rejects_arduino_framework(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(
        set_core_config, VARIANT_ESP32, framework=PlatformFramework.ESP32_ARDUINO
    )
    with pytest.raises(cv.Invalid, match="arduino framework"):
        CONFIG_SCHEMA({})


# --- _pm_final_validate --------------------------------------------------------


def _final_validate_config(
    set_core_config: SetCoreConfigCallable,
    config: ConfigType,
    /,
    *,
    variant: str = VARIANT_ESP32,
    full_config: ConfigType | None = None,
) -> ConfigType:
    """Run a config through CONFIG_SCHEMA, then _pm_final_validate."""
    _set_variant(set_core_config, variant)
    validated = CONFIG_SCHEMA(config)
    fv.full_config.set(full_config or {})
    _pm_final_validate(validated)
    return validated


def test_rejects_multiple_esp32_pm_instances(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32)
    validated = CONFIG_SCHEMA({})
    fv.full_config.set(
        {
            "power_management": [
                {"platform": "esp32_pm"},
                {"platform": "esp32_pm"},
            ]
        }
    )
    with pytest.raises(cv.Invalid, match="Only one esp32_pm instance"):
        _pm_final_validate(validated)


def test_allows_single_esp32_pm_instance(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _final_validate_config(
        set_core_config,
        {},
        full_config={"power_management": [{"platform": "esp32_pm"}]},
    )


def test_warns_when_boot_frequency_higher_than_max_frequency(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    _final_validate_config(
        set_core_config,
        {CONF_MAX_FREQUENCY: "160MHZ"},
        full_config={"esp32": {"cpu_frequency": "240MHZ"}},
    )
    assert "is higher than" in caplog.text
    assert "downclocked to 160MHZ" in caplog.text


def test_warns_when_boot_frequency_lower_than_max_frequency(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    _final_validate_config(
        set_core_config,
        {CONF_MAX_FREQUENCY: "240MHZ"},
        full_config={"esp32": {"cpu_frequency": "80MHZ"}},
    )
    assert "is lower than" in caplog.text
    assert "may run as high as 240MHZ" in caplog.text


def test_no_warning_when_boot_frequency_equals_max_frequency(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    _final_validate_config(
        set_core_config,
        {CONF_MAX_FREQUENCY: "160MHZ"},
        full_config={"esp32": {"cpu_frequency": "160MHZ"}},
    )
    assert caplog.text == ""


def test_rejects_power_down_flash_with_light_sleep_and_psram(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="not allowed when device has PSRAM"):
        _final_validate_config(
            set_core_config,
            {CONF_ENABLE_LIGHT_SLEEP: True, CONF_POWER_DOWN_FLASH: True},
            full_config={"psram": {"mode": "quad"}},
        )


@pytest.mark.parametrize(
    ("config", "match"),
    [
        pytest.param(
            {CONF_POWER_DOWN_PERIPHERALS: True},
            f"{CONF_POWER_DOWN_PERIPHERALS}: True not allowed when "
            f"{CONF_ENABLE_LIGHT_SLEEP} not set to True",
            id="power_down_peripherals",
        ),
        pytest.param(
            {CONF_POWER_DOWN_FLASH: True},
            f"{CONF_POWER_DOWN_FLASH}: True not allowed when "
            f"{CONF_ENABLE_LIGHT_SLEEP} not set to True",
            id="power_down_flash",
        ),
        pytest.param(
            {CONF_IDLE_TIME_BEFORE_SLEEP: 5},
            f"{CONF_IDLE_TIME_BEFORE_SLEEP} not allowed when "
            f"{CONF_ENABLE_LIGHT_SLEEP} not set to True",
            id="idle_time_before_sleep",
        ),
    ],
)
def test_rejects_sleep_only_options_without_light_sleep(
    set_core_config: SetCoreConfigCallable, config: ConfigType, match: str
) -> None:
    with pytest.raises(cv.Invalid, match=match):
        _final_validate_config(set_core_config, config)


@pytest.mark.parametrize(
    ("variant", "raises"),
    [
        pytest.param(VARIANT_ESP32, True, id="unsupported_variant"),
        pytest.param(VARIANT_ESP32H2, False, id="top_pd_variant"),
    ],
)
def test_power_down_peripherals_variant_support(
    set_core_config: SetCoreConfigCallable, variant: str, raises: bool
) -> None:
    config = {CONF_ENABLE_LIGHT_SLEEP: True, CONF_POWER_DOWN_PERIPHERALS: True}
    if raises:
        with pytest.raises(
            cv.Invalid, match="Power Down Peripherals is only available"
        ):
            _final_validate_config(set_core_config, config, variant=variant)
    else:
        _final_validate_config(set_core_config, config, variant=variant)


# --- to_code: openthread/zigbee sdkconfig wiring ------------------------------


@pytest.fixture
def mock_pm_cg():
    """Mock the codegen calls to_code() makes, without a full compile."""
    with (
        mock.patch(
            "esphome.components.esp32_pm.power_management.cg.new_Pvariable",
            return_value=mock.MagicMock(),
        ),
        mock.patch(
            "esphome.components.esp32_pm.power_management.power_management."
            "register_power_management",
            new_callable=mock.AsyncMock,
        ),
        mock.patch(
            "esphome.components.esp32_pm.power_management.add_idf_sdkconfig_option",
        ) as mock_sdkconfig,
    ):
        yield mock_sdkconfig


def _sdkconfig_calls(mock_sdkconfig: mock.MagicMock) -> dict[str, object]:
    return {call.args[0]: call.args[1] for call in mock_sdkconfig.call_args_list}


@pytest.mark.asyncio
async def test_to_code_sets_lwip_nd6_false_for_openthread_sleepy_mtd(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {
        CONF_OPENTHREAD: {
            CONF_DEVICE_TYPE: "MTD",
            CONF_POLL_PERIOD: TimePeriodMilliseconds(milliseconds=1000),
        }
    }
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert _sdkconfig_calls(mock_pm_cg)["CONFIG_LWIP_ND6"] is False


@pytest.mark.asyncio
async def test_to_code_leaves_lwip_nd6_alone_for_openthread_ftd(
    mock_pm_cg: mock.MagicMock,
) -> None:
    # An FTD (router) isn't a sleepy MTD, so the ND6 workaround must not apply.
    CORE.config = {
        CONF_OPENTHREAD: {
            CONF_DEVICE_TYPE: "FTD",
            CONF_POLL_PERIOD: TimePeriodMilliseconds(milliseconds=1000),
        }
    }
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert "CONFIG_LWIP_ND6" not in _sdkconfig_calls(mock_pm_cg)


@pytest.mark.asyncio
async def test_to_code_leaves_lwip_nd6_alone_without_openthread(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {}
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert "CONFIG_LWIP_ND6" not in _sdkconfig_calls(mock_pm_cg)


@pytest.mark.asyncio
async def test_to_code_sets_ieee802154_sleep_for_openthread(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {CONF_OPENTHREAD: {CONF_DEVICE_TYPE: "FTD"}}
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert _sdkconfig_calls(mock_pm_cg)["CONFIG_IEEE802154_SLEEP_ENABLE"] is True


@pytest.mark.asyncio
async def test_to_code_sets_ieee802154_sleep_for_zigbee(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {CONF_ZIGBEE: {"id": "zigbee_id"}}
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert _sdkconfig_calls(mock_pm_cg)["CONFIG_IEEE802154_SLEEP_ENABLE"] is True


@pytest.mark.asyncio
async def test_to_code_leaves_ieee802154_sleep_alone_without_radio(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {}
    await to_code({CONF_ID: mock.MagicMock(), CONF_ENABLE_LIGHT_SLEEP: True})
    assert "CONFIG_IEEE802154_SLEEP_ENABLE" not in _sdkconfig_calls(mock_pm_cg)


@pytest.mark.asyncio
async def test_to_code_skips_sleep_options_when_light_sleep_disabled(
    mock_pm_cg: mock.MagicMock,
) -> None:
    CORE.config = {CONF_OPENTHREAD: {}, CONF_ZIGBEE: {}}
    await to_code({CONF_ID: mock.MagicMock()})
    calls = _sdkconfig_calls(mock_pm_cg)
    assert "CONFIG_LWIP_ND6" not in calls
    assert "CONFIG_IEEE802154_SLEEP_ENABLE" not in calls
