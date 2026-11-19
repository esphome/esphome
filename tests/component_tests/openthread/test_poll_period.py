"""poll_period limit and the core_options it replaces."""

from collections.abc import Callable
import logging

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C6
from esphome.components.openthread import (
    CONFIG_SCHEMA,
    MTD_POLL_PERIOD_OPTIONS,
    POLL_PERIOD_ACTION_SCHEMA,
)
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

TLV = "0e080000000000010000000300001435060004001fffe0"
OTHER_OPTION = {"OPENTHREAD_CONFIG_MLE_MAX_CHILDREN": "5"}


@pytest.fixture(autouse=True)
def _esp32_c6_idf(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32C6},
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
    )


def _config(poll_period: str | None, **extra: object) -> dict:
    config = {"device_type": "MTD", "force_dataset": False, "tlv": TLV, **extra}
    if poll_period is not None:
        config["poll_period"] = poll_period
    return config


@pytest.mark.parametrize("name", sorted(MTD_POLL_PERIOD_OPTIONS))
def test_poll_period_drops_derived_core_options(
    caplog: pytest.LogCaptureFixture, name: str
) -> None:
    config = _config("5s", core_options={name: "0", **OTHER_OPTION})
    with caplog.at_level(logging.WARNING):
        result = CONFIG_SCHEMA(config)
    assert result["core_options"] == OTHER_OPTION
    assert f"'{name}' in core_options is deprecated" in caplog.text


@pytest.mark.parametrize("poll_period", [None, "0s"])
def test_no_polling_keeps_derived_core_options(
    caplog: pytest.LogCaptureFixture, poll_period: str | None
) -> None:
    options = {"OPENTHREAD_CONFIG_MLE_CHILD_TIMEOUT_DEFAULT": "960"}
    with caplog.at_level(logging.WARNING):
        result = CONFIG_SCHEMA(_config(poll_period, core_options=options))
    assert result["core_options"] == options
    assert "deprecated" not in caplog.text


def test_ftd_accepts_derived_core_options() -> None:
    options = {"OPENTHREAD_CONFIG_CHILD_SUPERVISION_CHECK_TIMEOUT": "0"}
    config = _config(None, core_options=options)
    config["device_type"] = "FTD"
    assert CONFIG_SCHEMA(config)["core_options"] == options


@pytest.mark.parametrize(
    "validate",
    [
        lambda period: CONFIG_SCHEMA(_config(period)),
        lambda period: POLL_PERIOD_ACTION_SCHEMA({"poll_period": period, "id": "ot"}),
    ],
    ids=["config", "action"],
)
@pytest.mark.parametrize(
    ("poll_period", "deprecated"), [("7200s", False), ("7201s", True)]
)
def test_poll_period_limit(
    caplog: pytest.LogCaptureFixture,
    validate: Callable[[str], dict],
    poll_period: str,
    deprecated: bool,
) -> None:
    with caplog.at_level(logging.WARNING):
        result = validate(poll_period)
    assert result["poll_period"].total_milliseconds == 7200 * 1000
    assert ("poll_period above 7200s is deprecated" in caplog.text) is deprecated
