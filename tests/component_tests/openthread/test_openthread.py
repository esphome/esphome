from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.esp32 import VARIANT_ESP32C6, VARIANT_ESP32S3
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_IDF_VERSION,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
)
from esphome.components.openthread import _final_validate, _validate_rcp
from esphome.components.openthread.const import (
    CONF_BORDER_ROUTER,
    CONF_DEVICE_TYPE,
    CONF_RCP,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_ENABLE_IPV6,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_INVERTED,
    CONF_LOGGER,
    CONF_NETWORKS,
    CONF_NUMBER,
    CONF_OPENTHREAD,
    CONF_RESET_PIN,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_WIFI,
    KEY_FRAMEWORK_VERSION,
    PlatformFramework,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

CONFIG_DIR = Path(__file__).parent / "config"
CONF_NETWORK = "network"
DEVICE_TYPE_FTD = "FTD"
DEVICE_TYPE_MTD = "MTD"
OTBR_SDKCONFIG_OPTIONS: dict[str, bool | int | str] = {
    "CONFIG_OPENTHREAD_BORDER_ROUTER": True,
    "CONFIG_OPENTHREAD_DNS64_CLIENT": False,
    "CONFIG_OPENTHREAD_SRP_CLIENT": False,
    "CONFIG_OPENTHREAD_PLATFORM_NETIF": True,
    "CONFIG_ESP_COEX_SW_COEXIST_ENABLE": True,
    "CONFIG_LWIP_IPV6_FORWARD": True,
    "CONFIG_LWIP_IPV6_NUM_ADDRESSES": 12,
    "CONFIG_LWIP_MULTICAST_PING": True,
    "CONFIG_LWIP_NETIF_STATUS_CALLBACK": True,
    "CONFIG_LWIP_HOOK_IP6_ROUTE_DEFAULT": True,
    "CONFIG_LWIP_HOOK_ND6_GET_GW_DEFAULT": True,
    "CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM": True,
    "CONFIG_LWIP_HOOK_IP6_SELECT_SRC_ADDR_CUSTOM": True,
    "CONFIG_MDNS_MULTIPLE_INSTANCE": True,
    "CONFIG_MBEDTLS_CMAC_C": True,
    "CONFIG_MBEDTLS_SSL_PROTO_DTLS": True,
    "CONFIG_MBEDTLS_KEY_EXCHANGE_ECJPAKE": True,
    "CONFIG_MBEDTLS_ECJPAKE_C": True,
}


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


def test_border_router_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(CONFIG_DIR / "border_router.yaml")

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    for option, value in OTBR_SDKCONFIG_OPTIONS.items():
        assert sdkconfig[option] == value
    assert any(define.name == "USE_OPENTHREAD_BORDER_ROUTER" for define in CORE.defines)


def test_border_router_rcp_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    cpp_main = generate_main(CONFIG_DIR / "border_router_rcp.yaml")

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_OPENTHREAD_RADIO_NATIVE"] is False
    assert sdkconfig["CONFIG_OPENTHREAD_RADIO_SPINEL_UART"] is True
    assert sdkconfig["CONFIG_VFS_SUPPORT_DIR"] is True
    assert "CONFIG_ESP_COEX_SW_COEXIST_ENABLE" not in sdkconfig
    assert any(define.name == "USE_OPENTHREAD_RCP_UART" for define in CORE.defines)
    assert "OpenThreadComponent(460800, 18, 17," in cpp_main


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
        (
            lambda config, _: config[CONF_NETWORK].update({CONF_ENABLE_IPV6: False}),
            "requires IPv6 to be enabled",
        ),
        (
            lambda _, otbr: otbr.update({CONF_DEVICE_TYPE: DEVICE_TYPE_MTD}),
            "device_type: FTD",
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


def test_border_router_requires_esp32c6(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    _set_esp32_idf_core(set_core_config, "ESP32S3")

    with pytest.raises(cv.Invalid, match="currently requires ESP32-C6"):
        _run_final_validation(otbr_config, full_config)


def test_border_router_requires_idf_5_5(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    _set_esp32_idf_core(set_core_config, idf_version=cv.Version(5, 4, 0))

    with pytest.raises(cv.Invalid, match="ESP-IDF 5.5.0 or newer"):
        _run_final_validation(otbr_config, full_config)


def test_border_router_rcp_accepts_esp32s3(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    _set_esp32_idf_core(set_core_config, VARIANT_ESP32S3)
    otbr_config[CONF_BORDER_ROUTER][CONF_RCP] = {
        CONF_RX_PIN: {CONF_NUMBER: 18},
        CONF_TX_PIN: {CONF_NUMBER: 17},
    }

    _run_final_validation(otbr_config, full_config)


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
    otbr_config[CONF_BORDER_ROUTER][CONF_RCP] = {
        CONF_RX_PIN: {CONF_NUMBER: 18},
        CONF_TX_PIN: {CONF_NUMBER: 17},
    }
    full_config.update(extra_config)

    with pytest.raises(cv.Invalid, match=message):
        _run_final_validation(otbr_config, full_config)


def test_rcp_rejects_same_rx_tx_pin() -> None:
    with pytest.raises(cv.Invalid, match="RX and TX pins must be different"):
        _validate_rcp(
            {
                CONF_RX_PIN: {CONF_NUMBER: 17},
                CONF_TX_PIN: {CONF_NUMBER: 17},
            }
        )


def test_rcp_rejects_reset_pin_collision() -> None:
    with pytest.raises(cv.Invalid, match="must be different from RX and TX pins"):
        _validate_rcp(
            {
                CONF_RX_PIN: {CONF_NUMBER: 18},
                CONF_TX_PIN: {CONF_NUMBER: 17},
                CONF_RESET_PIN: {CONF_NUMBER: 17, CONF_INVERTED: True},
            }
        )


def test_border_router_accepts_supported_config(
    set_core_config: SetCoreConfigCallable,
    otbr_config: ConfigType,
    full_config: ConfigType,
) -> None:
    _set_esp32_idf_core(set_core_config)

    _run_final_validation(otbr_config, full_config)
