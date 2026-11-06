"""Tests for the uart helpers shared by components that own a UART."""

import pytest

from esphome import config_validation as cv
from esphome.components.uart import claim_exclusive, subtree_references_uart
from esphome.const import (
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.helpers import uart_full_config
from tests.component_tests.types import SetCoreConfigCallable


def _set(
    set_core_config: SetCoreConfigCallable,
    uarts: list[ConfigType] | None = None,
    **domains: list[ConfigType],
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=uart_full_config(uarts, **domains)
    )


def _entry(uart_id: str, conf_key: str = CONF_UART_ID) -> ConfigType:
    return {conf_key: ID(uart_id)}


def test_finds_a_nested_uart_id() -> None:
    config = {"modbus": [{"id": ID("hub"), "uart_id": ID("bus")}]}
    assert subtree_references_uart(config, "bus")


def test_ignores_other_uarts_and_other_keys() -> None:
    config = {"modbus": [{"uart_id": ID("other")}], "sensor": [{"id": ID("bus")}]}
    assert not subtree_references_uart(config, "bus")


def test_finds_any_of_the_keys() -> None:
    config = {"modbus_tcp_uart": [{"tcp_uart_id": ID("bus")}]}
    assert subtree_references_uart(config, "bus", "tcp_uart_id")
    assert subtree_references_uart(config, "bus", CONF_UART_ID, "tcp_uart_id")
    assert not subtree_references_uart(config, "bus")


def test_matches_whole_keys_only() -> None:
    # A key that is part of another key's name must not match it
    config = {"sensor": [{"id": ID("bus"), "uart_id": ID("bus")}]}
    assert not subtree_references_uart(config, "bus", "tcp_uart_id")


def test_handles_scalars() -> None:
    assert not subtree_references_uart("bus", "bus")


def test_claims_distinct_uarts(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config)
    claim_exclusive(_entry("uart_0"), "owner")
    claim_exclusive(_entry("uart_1"), "owner")
    claim_exclusive(_entry("uart_0"), "other_owner")


def test_rejects_one_uart_under_two_keys(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config)
    claim_exclusive(_entry("uart_0"), "owner")
    with pytest.raises(cv.Invalid, match="already used by another 'owner' entry"):
        claim_exclusive(_entry("uart_0", "link_id"), "owner", "link_id")


def test_claims_ignore_the_owner_data(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config)
    fv.full_config.get().data["owner"] = {CONF_UART_ID: {"uart_0"}}
    claim_exclusive(_entry("uart_0"), "owner")


def test_rejects_a_second_entry_of_the_owner(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(set_core_config)
    claim_exclusive(_entry("uart_0"), "owner")
    with pytest.raises(
        cv.Invalid, match="already used by another 'owner' entry. Each owner"
    ):
        claim_exclusive(_entry("uart_0"), "owner")


@pytest.mark.parametrize("conf_key", [CONF_UART_ID, "link_id"])
def test_rejects_another_domain_naming_the_uart(
    set_core_config: SetCoreConfigCallable, conf_key: str
) -> None:
    _set(set_core_config, modbus=[{CONF_ID: ID("hub"), conf_key: ID("uart_0")}])
    with pytest.raises(
        cv.Invalid, match="also used by 'modbus'. owner requires exclusive use"
    ):
        claim_exclusive(_entry("uart_0", "link_id"), "owner", "link_id")


def test_ignores_the_owner_other_uarts_and_other_keys(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set(
        set_core_config,
        owner=[_entry("uart_0")],
        modbus=[_entry("uart_1"), _entry("uart_0", "link_id")],
    )
    claim_exclusive(_entry("uart_0"), "owner")


def test_testing_mode_allows_a_shared_bus(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # Grouped CI builds put several components on one bus package.
    CORE.testing_mode = True
    _set(set_core_config, modbus=[_entry("uart_0")])
    claim_exclusive(_entry("uart_0"), "owner")
    with pytest.raises(cv.Invalid, match="already used by another 'owner'"):
        claim_exclusive(_entry("uart_0"), "owner")


def test_rejects_dummy_receiver(set_core_config: SetCoreConfigCallable) -> None:
    uart_conf = {CONF_ID: ID("uart_0"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: True}}
    _set(set_core_config, uarts=[uart_conf])
    with pytest.raises(cv.Invalid, match="drops the bytes owner should forward") as err:
        claim_exclusive(_entry("uart_0"), "owner")
    assert err.value.path[-2:] == [CONF_DEBUG, CONF_DUMMY_RECEIVER]


def test_accepts_debug_without_dummy_receiver(
    set_core_config: SetCoreConfigCallable,
) -> None:
    uart_conf = {CONF_ID: ID("uart_0"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: False}}
    _set(set_core_config, uarts=[uart_conf])
    claim_exclusive(_entry("uart_0"), "owner")
