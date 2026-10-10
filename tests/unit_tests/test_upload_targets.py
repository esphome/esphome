"""Tests for the stable upload-targets classification helpers."""

import pytest

from esphome.upload_targets import PortType, get_port_type


@pytest.mark.parametrize(
    "port",
    [
        "/dev/ttyUSB0",
        "/dev/ttyACM0",
        "/dev/cu.usbserial-1410",
        "/dev/tty.usbmodem1101",
        "COM1",
        "COM23",
    ],
)
def test_get_port_type_serial(port: str) -> None:
    """Local serial devices classify as SERIAL."""
    assert get_port_type(port) is PortType.SERIAL


def test_get_port_type_bootsel() -> None:
    """``BOOTSEL`` magic string classifies as BOOTSEL."""
    assert get_port_type("BOOTSEL") is PortType.BOOTSEL


def test_get_port_type_mqtt() -> None:
    """``MQTT`` magic string classifies as MQTT."""
    assert get_port_type("MQTT") is PortType.MQTT


def test_get_port_type_mqttip() -> None:
    """``MQTTIP`` magic string classifies as MQTTIP."""
    assert get_port_type("MQTTIP") is PortType.MQTTIP


@pytest.mark.parametrize(
    "port",
    [
        "192.168.1.10",
        "fe80::1",
        "device.local",
        "my-esp.example.com",
    ],
)
def test_get_port_type_network(port: str) -> None:
    """IP addresses, mDNS, and hostnames classify as NETWORK."""
    assert get_port_type(port) is PortType.NETWORK


def test_port_type_values_are_stable() -> None:
    """Member values are part of the stable surface.

    External tooling (device-builder, etc.) may compare against the
    string values directly. Renaming or changing these breaks
    downstream consumers — guard against accidental edits.
    """
    assert PortType.SERIAL.value == "SERIAL"
    assert PortType.NETWORK.value == "NETWORK"
    assert PortType.MQTT.value == "MQTT"
    assert PortType.MQTTIP.value == "MQTTIP"
    assert PortType.BOOTSEL.value == "BOOTSEL"


def test_main_re_exports_for_backwards_compat() -> None:
    """``esphome.__main__`` re-exports the stable surface.

    The CLI entry point pre-dated the stable module and existing
    internal callers (and any third-party code that snuck in via
    ``__main__``) still import from there. The re-export must
    resolve to the same objects.
    """
    from esphome.__main__ import (
        PortType as MainPortType,
        get_port_type as main_get_port_type,
    )

    assert MainPortType is PortType
    assert main_get_port_type is get_port_type
