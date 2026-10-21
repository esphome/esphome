"""Unit tests for esphome.mqtt module."""

from __future__ import annotations

import json
import threading
import time
from unittest.mock import MagicMock, patch

import pytest

from esphome.const import (
    CONF_BIRTH_MESSAGE,
    CONF_BROKER,
    CONF_ESPHOME,
    CONF_MQTT,
    CONF_NAME,
    CONF_PAYLOAD,
    CONF_PORT,
    CONF_RETAIN,
    CONF_TOPIC,
)
from esphome.core import EsphomeError
from esphome.mqtt import get_esphome_device_ip, prepare, show_logs


@pytest.mark.parametrize(
    "birth",
    [
        None,
        {CONF_TOPIC: "test-device/status", CONF_PAYLOAD: "online", CONF_RETAIN: False},
        {CONF_TOPIC: "test-device/status", CONF_PAYLOAD: "", CONF_RETAIN: True},
    ],
    ids=["no birth message", "unretained birth message", "empty birth payload"],
)
@patch("esphome.mqtt.initialize")
def test_show_logs_asks_on_connect(
    mock_initialize: MagicMock, birth: dict | None
) -> None:
    """Without a retained birth message nothing tells a later CLI the device is up, so it asks on connect."""
    config = _discovery_config()
    if birth is not None:
        config[CONF_MQTT][CONF_BIRTH_MESSAGE] = birth

    show_logs(config)

    on_connect = mock_initialize.call_args.args[3]
    client = MagicMock()
    on_connect(client, None, None, 0)
    client.publish.assert_called_once_with(
        "esphome/logs/test-device", None, retain=False
    )
    if birth is None or not birth[CONF_PAYLOAD]:
        assert mock_initialize.call_args.args[1] == ["test-device/debug"]


@patch("esphome.mqtt.safe_print")
@patch("esphome.mqtt.initialize")
def test_show_logs_requests_crash_log_when_device_reports_online(
    mock_initialize: MagicMock, mock_print: MagicMock
) -> None:
    """With a retained birth message the request follows the device coming online, not the CLI connecting."""
    config = _discovery_config()
    config[CONF_MQTT][CONF_BIRTH_MESSAGE] = {
        CONF_TOPIC: "test-device/status",
        CONF_PAYLOAD: "online",
        CONF_RETAIN: True,
    }

    show_logs(config)

    assert mock_initialize.call_args.args[1] == [
        "test-device/debug",
        "test-device/status",
    ]
    on_message = mock_initialize.call_args.args[2]
    on_connect = mock_initialize.call_args.args[3]
    client = MagicMock()
    on_connect(client, None, None, 0)
    client.publish.assert_not_called()

    on_message(client, None, MagicMock(topic="test-device/status", payload=b"offline"))
    client.publish.assert_not_called()
    on_message(client, None, MagicMock(topic="test-device/status", payload=b"online"))
    client.publish.assert_called_once_with(
        "esphome/logs/test-device", None, retain=False
    )
    mock_print.assert_not_called()

    on_message(
        client, None, MagicMock(topic="test-device/debug", payload=b"[I][app]: hi")
    )
    assert mock_print.call_args.args[0].endswith("[I][app]: hi")
    client.publish.assert_called_once()


@patch("esphome.mqtt.safe_print")
@patch("esphome.mqtt.initialize")
def test_show_logs_prints_a_birth_message_inside_the_log_subscription(
    mock_initialize: MagicMock, mock_print: MagicMock
) -> None:
    """A birth topic covered by the log topic is still printed, and still requests the crash log."""
    config = _discovery_config()
    config[CONF_MQTT][CONF_BIRTH_MESSAGE] = {
        CONF_TOPIC: "test-device/status",
        CONF_PAYLOAD: "online",
        CONF_RETAIN: True,
    }

    show_logs(config, topic="test-device/#")

    on_message = mock_initialize.call_args.args[2]
    client = MagicMock()
    on_message(client, None, MagicMock(topic="test-device/status", payload=b"online"))
    client.publish.assert_called_once_with(
        "esphome/logs/test-device", None, retain=False
    )
    assert mock_print.call_args.args[0].endswith("online")


def test_get_esphome_device_ip_empty_broker() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when broker is empty."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "",
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the broker is not configured",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_none_broker() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when broker is None."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: None,
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the broker is not configured",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_mqtt() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when mqtt config is missing."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the mqtt:",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_esphome() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when esphome config is missing."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the device name:",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_name() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when device name is missing."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
        CONF_ESPHOME: {},
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the device name:",
    ):
        get_esphome_device_ip(config)


def _discovery_config() -> dict:
    return {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }


def _deliver_on_loop_start(mock_prepare, client, payload: bytes) -> None:
    """Deliver a discovery answer as soon as the network loop starts."""

    def deliver(*args, **kwargs):
        msg = MagicMock()
        msg.payload = payload
        mock_prepare.call_args.args[2](client, None, msg)

    client.loop_start.side_effect = deliver


def test_get_esphome_device_ip_success() -> None:
    """A device answer on the discovery topic returns its IPs."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        _deliver_on_loop_start(
            mock_prepare,
            client,
            json.dumps(
                {"name": "test-device", "ip": "10.0.0.5", "ip1": "10.0.0.6"}
            ).encode(),
        )

        result = get_esphome_device_ip(_discovery_config())

    assert result == ["10.0.0.5", "10.0.0.6"]
    client.loop_stop.assert_called_once_with()
    # Once from on_message on receiving the answer, once from the finally
    assert client.disconnect.call_count == 2


def test_get_esphome_device_ip_preset_stop_event_skips_lookup() -> None:
    """A stop event set before the call returns [] without touching the broker."""
    stop_event = threading.Event()
    stop_event.set()

    with patch("esphome.mqtt.prepare") as mock_prepare:
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    assert result == []
    mock_prepare.assert_not_called()


def test_get_esphome_device_ip_stop_event_aborts_wait() -> None:
    """A stop event set mid-wait exits quietly with no addresses."""
    stop_event = threading.Event()
    client = MagicMock()
    # Simulate teardown starting right after the network loop spins up
    client.loop_start.side_effect = stop_event.set

    start = time.monotonic()
    with patch("esphome.mqtt.prepare", return_value=client):
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    # An abort is not a failure and must be nowhere near the 25s timeout
    assert result == []
    assert time.monotonic() - start < 5
    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_timeout_raises() -> None:
    """No answer within the timeout raises EsphomeError (default stop event path)."""
    client = MagicMock()
    with (
        patch("esphome.mqtt.prepare", return_value=client),
        pytest.raises(EsphomeError, match="Failed to find IP via MQTT"),
    ):
        get_esphome_device_ip(_discovery_config(), timeout=0.25)

    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_stop_during_connect_skips_wait() -> None:
    """A stop event set while the broker connect is in flight still cleans up."""
    stop_event = threading.Event()
    client = MagicMock()

    def prepare_and_stop(*args):
        stop_event.set()
        return client

    with patch("esphome.mqtt.prepare", side_effect=prepare_and_stop):
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    assert result == []
    client.loop_start.assert_not_called()
    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_replaces_reconnect_handler(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The one-shot discovery client must not inherit the reconnect-forever
    handler, which would make loop_stop() join the network thread forever;
    its replacement still reports a broker-initiated disconnect."""
    client = MagicMock()
    prepare_handler = MagicMock()
    client.on_disconnect = prepare_handler

    with (
        patch("esphome.mqtt.prepare", return_value=client),
        pytest.raises(EsphomeError, match="Failed to find IP via MQTT"),
    ):
        get_esphome_device_ip(_discovery_config(), timeout=0.25)

    assert client.on_disconnect is not prepare_handler
    client.on_disconnect(client, None, 0)
    assert "Disconnected from MQTT broker" not in caplog.text
    client.on_disconnect(client, None, 5)
    assert "Disconnected from MQTT broker (5)" in caplog.text


def test_get_esphome_device_ip_answer_without_ip_fails_fast(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A device answer with no IP fields fails promptly, not at the timeout."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        _deliver_on_loop_start(
            mock_prepare, client, json.dumps({"name": "test-device"}).encode()
        )

        start = time.monotonic()
        with pytest.raises(EsphomeError, match="Failed to find IP via MQTT"):
            get_esphome_device_ip(_discovery_config(), timeout=5)

    assert time.monotonic() - start < 1
    assert "Device answer did not include an IP address" in caplog.text


@pytest.mark.parametrize("payload", [b"not json {", b"123", b"null"])
def test_get_esphome_device_ip_unparsable_payload_ignored(
    caplog: pytest.LogCaptureFixture,
    payload: bytes,
) -> None:
    """Garbage on the discovery topic must not kill paho's network thread."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        _deliver_on_loop_start(mock_prepare, client, payload)

        with pytest.raises(EsphomeError, match="Failed to find IP via MQTT"):
            get_esphome_device_ip(_discovery_config(), timeout=0)

    assert "Ignoring unparsable discovery payload" in caplog.text


def test_get_esphome_device_ip_broker_disconnect_fails_fast(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A broker-initiated disconnect aborts the wait instead of timing out."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client):

        def drop_connection(*args, **kwargs):
            client.on_disconnect(client, None, 5)

        client.loop_start.side_effect = drop_connection

        start = time.monotonic()
        with pytest.raises(EsphomeError, match="Failed to find IP via MQTT"):
            get_esphome_device_ip(_discovery_config(), timeout=5)

    assert time.monotonic() - start < 1
    assert "Disconnected from MQTT broker (5)" in caplog.text


def test_get_esphome_device_ip_sends_discovery_ping() -> None:
    """Connecting publishes the discovery ping for the device."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:

        def connect_then_answer(*args, **kwargs):
            on_connect = mock_prepare.call_args.args[3]
            on_connect(client, None, None, 0)
            msg = MagicMock()
            msg.payload = json.dumps({"name": "test-device", "ip": "10.0.0.5"}).encode()
            mock_prepare.call_args.args[2](client, None, msg)

        client.loop_start.side_effect = connect_then_answer

        result = get_esphome_device_ip(_discovery_config())

    assert result == ["10.0.0.5"]
    client.publish.assert_called_once_with(
        "esphome/ping/test-device", None, retain=False
    )


def test_get_esphome_device_ip_disconnect_error_does_not_mask_result(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A cleanup failure must not replace the discovery result."""
    client = MagicMock()
    # First disconnect (from on_message) succeeds; the finally's fails
    client.disconnect.side_effect = [None, OSError("socket already closed")]

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        _deliver_on_loop_start(
            mock_prepare,
            client,
            json.dumps({"name": "test-device", "ip": "10.0.0.5"}).encode(),
        )

        result = get_esphome_device_ip(_discovery_config())

    assert result == ["10.0.0.5"]
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_invalid_address_values_skipped(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Non-string or non-printable ip values are skipped, valid ones kept."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        _deliver_on_loop_start(
            mock_prepare,
            client,
            json.dumps(
                {
                    "name": "test-device",
                    "ip": 1234,
                    "ip1": "x\n[00:00:00][I][forged] fake line",
                    "ip2": "  10.0.0.5  ",
                }
            ).encode(),
        )

        result = get_esphome_device_ip(_discovery_config())

    assert result == ["10.0.0.5"]
    assert caplog.text.count("Ignoring invalid address in discovery answer") == 2
    assert "forged" not in "".join(
        r.getMessage() for r in caplog.records if "Found IP" in r.getMessage()
    )


@pytest.mark.parametrize(
    ("transport", "ws_path", "expect_tls", "expect_path"),
    [
        (None, None, False, None),
        ("ws", None, False, "/mqtt"),
        ("wss", "/ws", True, "/ws"),
    ],
    ids=["tcp", "ws with the default path", "wss with a path"],
)
@patch("esphome.mqtt.ssl.create_default_context")
@patch("esphome.mqtt.mqtt.Client")
def test_prepare_follows_the_firmware_transport(
    mock_client_class: MagicMock,
    mock_context: MagicMock,
    transport: str | None,
    ws_path: str | None,
    expect_tls: bool,
    expect_path: str | None,
) -> None:
    """The CLI client speaks WebSocket, and TLS over wss, when the firmware is configured to."""
    config = _discovery_config()
    config[CONF_MQTT][CONF_PORT] = 1883
    if transport is not None:
        config[CONF_MQTT]["transport"] = transport
    if ws_path is not None:
        config[CONF_MQTT]["ws_path"] = ws_path
    client = mock_client_class.return_value

    assert prepare(config, [], None, None, None, None, None) is client

    mock_client_class.assert_called_once_with(
        "", transport="websockets" if expect_path else "tcp"
    )
    if expect_path is None:
        client.ws_set_options.assert_not_called()
    else:
        client.ws_set_options.assert_called_once_with(path=expect_path)
    if expect_tls:
        mock_context.assert_called_once_with(cadata=None)
        client.tls_set_context.assert_called_once_with(mock_context.return_value)
    else:
        client.tls_set_context.assert_not_called()
    client.connect.assert_called_once_with("mqtt.local", 1883)
