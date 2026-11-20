from collections.abc import Callable
import logging
from pathlib import Path
from typing import Any

import pytest

from esphome.components.const import CONF_BORDER_ROUTER, CONF_DEVICE_TYPE
from esphome.components.esp32 import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32S3,
)
from esphome.components.esp32.const import (
    KEY_BOARD,
    KEY_ESP32,
    KEY_IDF_VERSION,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
)
from esphome.components.openthread import (
    _RCP_SCHEMA,
    BORDER_ROUTER_SDKCONFIG_OPTIONS,
    CONFIG_SCHEMA,
    MTD_POLL_PERIOD_OPTIONS,
    POLL_PERIOD_ACTION_SCHEMA,
    RCP_BORDER_ROUTER_VARIANTS,
    _final_validate,
    _validate,
    _validate_platform,
)
from esphome.components.openthread.const import (
    CONF_CORE_OPTIONS,
    CONF_POLL_PERIOD,
    CONF_RCP,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_BAUD_RATE,
    CONF_ENABLE_IPV6,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_INVERTED,
    CONF_LOGGER,
    CONF_NETWORKS,
    CONF_NUMBER,
    CONF_OPENTHREAD,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_USE_ADDRESS,
    CONF_WIFI,
    KEY_FRAMEWORK_VERSION,
    PlatformFramework,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.helpers import get_define_value
from tests.component_tests.types import SetCoreConfigCallable

CONFIG_DIR = Path(__file__).parent / "config"
CONF_NETWORK = "network"
DEVICE_TYPE_FTD = "FTD"
DEVICE_TYPE_MTD = "MTD"
RCP_PINS = {CONF_RX_PIN: 18, CONF_TX_PIN: 17}


@pytest.fixture
def otbr_config() -> ConfigType:
    return {
        CONF_ID: "openthread_component",
        CONF_BORDER_ROUTER: {},
        CONF_DEVICE_TYPE: DEVICE_TYPE_FTD,
    }


@pytest.fixture
def full_config(otbr_config: ConfigType) -> ConfigType:
    return {
        CONF_OPENTHREAD: otbr_config,
        CONF_NETWORK: {CONF_ENABLE_IPV6: True},
        CONF_WIFI: {
            CONF_ENABLE_ON_BOOT: True,
            CONF_NETWORKS: [{"ssid": "test"}],
        },
    }


def _set_esp32_idf_core(
    set_core_config: SetCoreConfigCallable,
    variant: str = VARIANT_ESP32C6,
    idf_version: cv.Version | None = None,
) -> None:
    if idf_version is None:
        idf_version = cv.Version(5, 5, 5)
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: idf_version},
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_IDF_VERSION: idf_version,
            KEY_VARIANT: variant,
            KEY_SDKCONFIG_OPTIONS: {},
        },
    )


def _run_final_validation(
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    token = fv.full_config.set(full_config)
    try:
        _final_validate(otbr_config)
    finally:
        fv.full_config.reset(token)


def test_plain_openthread_uses_the_srp_client(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(CONFIG_DIR / "no_custom_header.yaml")

    defines = {define.name for define in CORE.defines}
    assert "USE_OPENTHREAD_SRP" in defines
    assert "USE_OPENTHREAD_BORDER_ROUTER" not in defines
    assert (
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_OPENTHREAD_SRP_CLIENT"]
        is True
    )


def test_border_router_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(CONFIG_DIR / "border_router.yaml")

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    for option, value in BORDER_ROUTER_SDKCONFIG_OPTIONS.items():
        assert sdkconfig[option] == value
    assert sdkconfig["CONFIG_ESP_COEX_SW_COEXIST_ENABLE"] is True
    assert sdkconfig["CONFIG_OPENTHREAD_SRP_CLIENT"] is False
    assert sdkconfig["CONFIG_OPENTHREAD_DNS64_CLIENT"] is False
    assert (
        sdkconfig["CONFIG_OPENTHREAD_NETWORK_MASTERKEY"]
        == "00112233445566778899aabbccddeeff"
    )
    assert sdkconfig["CONFIG_OPENTHREAD_NETWORK_EXTPANID"] == "00000000000000ab"
    assert (
        sdkconfig["CONFIG_OPENTHREAD_NETWORK_PSKC"]
        == "000000000000000000000000000000cd"
    )
    defines = {define.name for define in CORE.defines}
    assert "USE_OPENTHREAD_BORDER_ROUTER" in defines
    assert "USE_OPENTHREAD_SRP" not in defines


def test_border_router_rcp_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    cpp_main = generate_main(CONFIG_DIR / "border_router_rcp.yaml")

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_OPENTHREAD_RADIO_NATIVE"] is False
    assert sdkconfig["CONFIG_OPENTHREAD_RADIO_SPINEL_UART"] is True
    assert sdkconfig["CONFIG_VFS_SUPPORT_DIR"] is True
    assert sdkconfig.get("CONFIG_ESP_COEX_SW_COEXIST_ENABLE") is not True
    assert any(define.name == "USE_OPENTHREAD_RCP_UART" for define in CORE.defines)
    assert "OpenThreadComponent(460800, 18, 17, nullptr)" in cpp_main


@pytest.mark.parametrize(
    ("mutate", "message"),
    [
        (
            lambda config, _: config.pop(CONF_WIFI),
            "requires a Wi-Fi STA backbone",
        ),
        (
            lambda config, _: config[CONF_WIFI].update({CONF_NETWORKS: []}),
            "requires at least one configured",
        ),
        (
            lambda config, _: config[CONF_WIFI].update({CONF_ENABLE_ON_BOOT: False}),
            "enable_on_boot: true",
        ),
        (
            lambda config, _: config[CONF_WIFI].update({CONF_AP: {}}),
            "does not support Wi-Fi AP",
        ),
    ],
)
def test_border_router_rejects_incompatible_config(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
    mutate: Any,
    message: str,
) -> None:
    _set_esp32_idf_core(set_core_config)
    mutate(full_config, otbr_config)

    with pytest.raises(cv.Invalid, match=message):
        _run_final_validation(otbr_config, full_config)


def test_border_router_requires_ftd(
    set_core_config: SetCoreConfigCallable, otbr_config: ConfigType
) -> None:
    _set_esp32_idf_core(set_core_config)
    otbr_config[CONF_DEVICE_TYPE] = DEVICE_TYPE_MTD
    otbr_config[CONF_USE_ADDRESS] = "test.local"

    with pytest.raises(cv.Invalid, match="device_type: FTD"):
        _validate(otbr_config)


def test_native_border_router_rejects_variant_without_ieee802154(
    set_core_config: SetCoreConfigCallable, otbr_config: ConfigType
) -> None:
    _set_esp32_idf_core(set_core_config, VARIANT_ESP32S3)

    with pytest.raises(cv.Invalid, match="both Wi-Fi and IEEE 802.15.4"):
        _validate_platform(otbr_config)


def test_requires_idf_5_5_5(set_core_config: SetCoreConfigCallable) -> None:
    _set_esp32_idf_core(set_core_config, idf_version=cv.Version(5, 5, 2))

    with pytest.raises(cv.Invalid, match="at least framework version 5.5.5"):
        _validate_platform({CONF_DEVICE_TYPE: DEVICE_TYPE_FTD})


def test_rejects_arduino(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_ARDUINO)

    with pytest.raises(cv.Invalid, match="incompatible with ESP32 using arduino"):
        _validate_platform({CONF_DEVICE_TYPE: DEVICE_TYPE_FTD})


@pytest.mark.parametrize("variant", sorted(RCP_BORDER_ROUTER_VARIANTS))
def test_border_router_rcp_accepts_wifi_variants(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    variant: str,
) -> None:
    _set_esp32_idf_core(set_core_config, variant)
    otbr_config[CONF_BORDER_ROUTER][CONF_RCP] = RCP_PINS

    assert _validate_platform(otbr_config) is otbr_config


@pytest.mark.parametrize("variant", [VARIANT_ESP32H2, VARIANT_ESP32H4])
def test_border_router_rcp_rejects_variants_without_wifi(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    variant: str,
) -> None:
    _set_esp32_idf_core(set_core_config, variant)
    otbr_config[CONF_BORDER_ROUTER][CONF_RCP] = RCP_PINS

    with pytest.raises(cv.Invalid, match="requires a Wi-Fi-capable ESP32 variant"):
        _validate_platform(otbr_config)


@pytest.mark.parametrize(
    ("extra_config", "message"),
    [
        ({"uart": [{}]}, "cannot currently be combined"),
        (
            {CONF_LOGGER: {"hardware_uart": "UART1", "baud_rate": 115200}},
            "reserves UART1",
        ),
    ],
)
def test_border_router_rcp_rejects_uart_conflicts(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
    extra_config: ConfigType,
    message: str,
) -> None:
    _set_esp32_idf_core(set_core_config, VARIANT_ESP32S3)
    otbr_config[CONF_BORDER_ROUTER][CONF_RCP] = RCP_PINS
    full_config.update(extra_config)

    with pytest.raises(cv.Invalid, match=message):
        _run_final_validation(otbr_config, full_config)


@pytest.mark.parametrize(
    "rcp_config",
    [
        {**RCP_PINS, CONF_BAUD_RATE: 0},
        {**RCP_PINS, CONF_BAUD_RATE: 9599},
        {CONF_RX_PIN: {CONF_NUMBER: 18, CONF_INVERTED: True}, CONF_TX_PIN: 17},
    ],
)
def test_rcp_schema_rejects_bad_options(
    set_core_config: SetCoreConfigCallable, rcp_config: ConfigType
) -> None:
    _set_esp32_idf_core(set_core_config, VARIANT_ESP32S3)

    with pytest.raises(cv.Invalid):
        _RCP_SCHEMA(rcp_config)


@pytest.mark.parametrize("variant", [VARIANT_ESP32C5, VARIANT_ESP32C6])
def test_native_border_router_accepts_wifi_ieee802154_variants(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    variant: str,
) -> None:
    _set_esp32_idf_core(set_core_config, variant)

    assert _validate_platform(otbr_config) is otbr_config


def test_border_router_accepts_supported_config(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    _set_esp32_idf_core(set_core_config)

    _run_final_validation(otbr_config, full_config)


SHORT_TLV = "0e080000000000010000000300001435060004001fffe0"
OTHER_CORE_OPTION = {"OPENTHREAD_CONFIG_MLE_MAX_CHILDREN": "5"}


def _poll_config(
    poll_period: str | None, device_type: str = DEVICE_TYPE_MTD, **extra: Any
) -> ConfigType:
    config = {CONF_DEVICE_TYPE: device_type, "tlv": SHORT_TLV, **extra}
    if poll_period is not None:
        config[CONF_POLL_PERIOD] = poll_period
    return config


@pytest.mark.parametrize(
    "validate",
    [
        lambda period: CONFIG_SCHEMA(_poll_config(period)),
        lambda period: POLL_PERIOD_ACTION_SCHEMA(
            {CONF_POLL_PERIOD: period, CONF_ID: "ot"}
        ),
    ],
    ids=["config", "action"],
)
@pytest.mark.parametrize(
    ("poll_period", "deprecated"), [("7200s", False), ("7201s", True)]
)
def test_poll_period_limit(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
    validate: Callable[[str], ConfigType],
    poll_period: str,
    deprecated: bool,
) -> None:
    _set_esp32_idf_core(set_core_config)
    with caplog.at_level(logging.WARNING):
        result = validate(poll_period)
    assert result[CONF_POLL_PERIOD].total_milliseconds == 7200 * 1000
    assert ("poll_period above 7200s is deprecated" in caplog.text) is deprecated


@pytest.mark.parametrize(
    ("device_type", "poll_period"),
    [
        (DEVICE_TYPE_MTD, None),
        (DEVICE_TYPE_MTD, "0s"),
        (DEVICE_TYPE_MTD, "5s"),
        (DEVICE_TYPE_FTD, None),
    ],
)
def test_derived_core_options_are_kept(
    set_core_config: SetCoreConfigCallable,
    device_type: str,
    poll_period: str | None,
) -> None:
    _set_esp32_idf_core(set_core_config)
    options = {**dict.fromkeys(MTD_POLL_PERIOD_OPTIONS, "960"), **OTHER_CORE_OPTION}
    config = _poll_config(poll_period, device_type, **{CONF_CORE_OPTIONS: options})
    assert CONFIG_SCHEMA(config)[CONF_CORE_OPTIONS] == options


@pytest.mark.parametrize("name", sorted(MTD_POLL_PERIOD_OPTIONS))
def test_mtd_rejects_derived_core_option_expressions(
    set_core_config: SetCoreConfigCallable, name: str
) -> None:
    _set_esp32_idf_core(set_core_config)
    config = _poll_config(None, **{CONF_CORE_OPTIONS: {name: "(240 + 10)"}})
    with pytest.raises(cv.Invalid, match="must be a whole number of seconds"):
        CONFIG_SCHEMA(config)


def test_mtd_timeout_minimums_follow_core_options(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(CONFIG_DIR / "mtd_core_options.yaml")

    # The configured value, else the OpenThread default; polling never goes below it
    assert get_define_value("ESPHOME_OPENTHREAD_CHILD_TIMEOUT_MIN") == "960"
    assert (
        get_define_value("ESPHOME_OPENTHREAD_CHILD_SUPERVISION_INTERVAL_MIN") == "129"
    )
