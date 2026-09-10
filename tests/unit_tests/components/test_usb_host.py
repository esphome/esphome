"""Tests for usb_host device matching validation."""

import pytest

from esphome.components.usb_host import validate_usb_clients
import esphome.config_validation as cv
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("first", "second"),
    [
        pytest.param(
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4002,
            },
            id="different_pid",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x1A86,
                "pid": 0x4001,
            },
            id="different_vid",
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
                "manufacturer": "Nabu Casa",
                "product": "ZWA-2",
            },
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
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0,
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Nabu Casa",
                "product": "ZWA-2",
            },
            id="wildcard_pid_separated_by_product",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4002,
            },
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
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
            },
            id="exact_duplicate",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            id="unfiltered_shadows_filtered",
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
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            id="identical_filters",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0,
                "pid": 0,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            id="zero_ids_match_every_device",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0x303A,
                "pid": 0,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0x4001,
                "manufacturer": "Nabu Casa",
                "product": "ZBT-2",
            },
            id="wildcard_pid_shadows_filtered",
        ),
        pytest.param(
            {
                "id": "a",
                "vid": 0,
                "pid": 0x4001,
            },
            {
                "id": "b",
                "vid": 0x303A,
                "pid": 0,
            },
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
            "pid": 0x4002,
        },
        {
            "id": "c",
            "vid": 0x303A,
            "pid": 0x4001,
            "manufacturer": "Nabu Casa",
            "product": "ZBT-2",
        },
    ]
    with pytest.raises(cv.Invalid, match="'a', 'c'"):
        validate_usb_clients(configs)


def test_incomplete_filter_is_rejected() -> None:
    configs = [
        {
            "id": "a",
            "vid": 0x303A,
            "pid": 0x4001,
            "product": "ZBT-2",
        }
    ]
    with pytest.raises(cv.Invalid, match="none or all"):
        validate_usb_clients(configs)


def test_single_client_is_always_valid() -> None:
    configs = [
        {
            "id": "a",
            "vid": 0x303A,
            "pid": 0x4001,
        }
    ]
    assert validate_usb_clients(configs) is configs
