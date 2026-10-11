"""Tests for usb_host device matching validation."""

import logging

import pytest

from esphome.components.usb_host import _final_validate, validate_usb_clients
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("first", "second"),
    [
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001},
            {"id": "b", "vid": 0x303A, "pid": 0x4002},
            id="different_pid",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001},
            {"id": "b", "vid": 0x1A86, "pid": 0x4001},
            id="different_vid",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZWA-2"},
            id="different_product",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Espressif",
                "product": "ZBT-2",
            },
            id="different_manufacturer",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0, "product": "ZBT-2"},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZWA-2"},
            id="wildcard_pid_separated_by_product",
        ),
        pytest.param(
            {"id": "a", "vid": 0, "pid": 0x4001},
            {"id": "b", "vid": 0x303A, "pid": 0x4002},
            id="wildcard_vid_different_pid",
        ),
    ],
)
def test_disjoint_clients_are_accepted(first: ConfigType, second: ConfigType) -> None:
    configs = [first, second]
    assert validate_usb_clients(configs) is configs


@pytest.mark.parametrize(
    ("first", "second"),
    [
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001},
            {"id": "b", "vid": 0x303A, "pid": 0x4001},
            id="exact_duplicate",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            id="unfiltered_shadows_filtered",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            id="identical_filters",
        ),
        pytest.param(
            {"id": "a", "vid": 0, "pid": 0},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            id="zero_ids_match_every_device",
        ),
        pytest.param(
            {"id": "a", "vid": 0x303A, "pid": 0},
            {"id": "b", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
            id="wildcard_pid_shadows_filtered",
        ),
        pytest.param(
            {"id": "a", "vid": 0, "pid": 0x4001},
            {"id": "b", "vid": 0x303A, "pid": 0},
            id="wildcards_on_different_fields",
        ),
    ],
)
def test_overlapping_clients_are_rejected(
    first: ConfigType, second: ConfigType
) -> None:
    with pytest.raises(cv.Invalid, match="overlap"):
        validate_usb_clients([first, second])


def test_every_pair_is_compared_not_just_neighbours() -> None:
    configs = [
        {"id": "a", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
        {"id": "b", "vid": 0x303A, "pid": 0x4002},
        {"id": "c", "vid": 0x303A, "pid": 0x4001, "product": "ZBT-2"},
    ]
    with pytest.raises(cv.Invalid, match="a, c"):
        validate_usb_clients(configs)


def _run_final_validate(
    devices: list[ConfigType], uarts: list[ConfigType]
) -> ConfigType:
    config = {"id": "usb_host", "devices": devices}
    token = fv.full_config.set({"usb_host": config, "usb_uart": uarts})
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_device_duplicating_usb_uart_warns(caplog: pytest.LogCaptureFixture) -> None:
    devices = [{"id": "device_0", "vid": 0x303A, "pid": 0x4001}]
    uarts = [{"id": "uart_0", "vid": 0x303A, "pid": 0x4001}]
    with caplog.at_level(logging.WARNING):
        _run_final_validate(devices, uarts)
    assert "'device_0'" in caplog.text
    assert "'uart_0'" in caplog.text
    assert "remove it from usb_host devices" in caplog.text


def test_wildcard_device_covering_usb_uart_suggests_narrowing(
    caplog: pytest.LogCaptureFixture,
) -> None:
    devices = [{"id": "device_0", "vid": 0x303A, "pid": 0}]
    uarts = [{"id": "uart_0", "vid": 0x303A, "pid": 0x4001}]
    with caplog.at_level(logging.WARNING):
        _run_final_validate(devices, uarts)
    assert "narrow its filter" in caplog.text
    assert "remove it" not in caplog.text


def test_disjoint_device_and_usb_uart_do_not_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    devices = [{"id": "device_0", "vid": 0x303A, "pid": 0x4002}]
    uarts = [{"id": "uart_0", "vid": 0x303A, "pid": 0x4001}]
    with caplog.at_level(logging.WARNING):
        _run_final_validate(devices, uarts)
    assert caplog.text == ""


_DUPLICATE_CLIENTS = [
    {"id": "a", "vid": 0x303A, "pid": 0x4001},
    {"id": "b", "vid": 0x303A, "pid": 0x4001},
]


@pytest.mark.parametrize(
    ("devices", "uarts"),
    [([*_DUPLICATE_CLIENTS], []), ([], [*_DUPLICATE_CLIENTS])],
    ids=["devices", "uarts"],
)
def test_overlap_within_one_component_is_rejected(
    devices: list[ConfigType], uarts: list[ConfigType]
) -> None:
    with pytest.raises(cv.Invalid, match="a, b"):
        _run_final_validate(devices, uarts)


def test_device_inside_broader_usb_uart_suggests_removing(
    caplog: pytest.LogCaptureFixture,
) -> None:
    devices = [{"id": "device_0", "vid": 0x303A, "pid": 0x4001}]
    uarts = [{"id": "uart_0", "vid": 0x303A, "pid": 0}]
    with caplog.at_level(logging.WARNING):
        _run_final_validate(devices, uarts)
    assert "remove it from usb_host devices" in caplog.text
