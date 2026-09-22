"""Tests for the async_tcp component."""

from esphome.core import CORE


def test_libretiny_ignores_asynctcp_esphome_fork(generate_main):
    """MQTT loads async_tcp without a web server, so async_tcp adds the ignore."""
    generate_main("tests/component_tests/async_tcp/test_async_tcp_libretiny_mqtt.yaml")

    assert "AsyncTCP-esphome" in CORE.platformio_options["lib_ignore"]
