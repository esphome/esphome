from esphome.components import socket
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE


def _setup_platform(platform=PLATFORM_ESP8266) -> None:
    """Set up CORE.data with a platform for testing."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}


def test_require_wake_loop_threadsafe__first_call() -> None:
    """Test that first call sets up define and consumes socket."""
    _setup_platform()
    CORE.config = {"wifi": True}
    socket.require_wake_loop_threadsafe()

    # Verify CORE.data was updated
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__idempotent() -> None:
    """Test that subsequent calls are idempotent."""
    # Set up initial state as if already called
    CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] = True
    CORE.config = {"ethernet": True}

    # Call again - should not raise or fail
    socket.require_wake_loop_threadsafe()

    # Verify state is still True
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Define should not be added since flag was already True
    assert not any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__multiple_calls() -> None:
    """Test that multiple calls only set up once."""
    _setup_platform()
    # Call three times
    CORE.config = {"openthread": True}
    socket.require_wake_loop_threadsafe()
    socket.require_wake_loop_threadsafe()
    socket.require_wake_loop_threadsafe()

    # Verify CORE.data was set
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added (only once, but we can just check it exists)
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__no_networking() -> None:
    """Test that wake loop is NOT configured when no networking is configured."""
    # Set up config without any networking components
    CORE.config = {"esphome": {"name": "test"}, "logger": {}}

    # Call require_wake_loop_threadsafe
    socket.require_wake_loop_threadsafe()

    # Verify CORE.data flag was NOT set (since has_networking returns False)
    assert socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED not in CORE.data

    # Verify the define was NOT added
    assert not any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__no_networking_does_not_consume_socket() -> None:
    """Test that no socket is consumed when no networking is configured."""
    # Set up config without any networking components
    CORE.config = {"logger": {}}

    # Track initial socket consumer state
    initial_udp = CORE.data.get(socket.KEY_SOCKET_CONSUMERS_UDP, {})

    # Call require_wake_loop_threadsafe
    socket.require_wake_loop_threadsafe()

    # Verify no socket was consumed
    udp_consumers = CORE.data.get(socket.KEY_SOCKET_CONSUMERS_UDP, {})
    assert "socket.wake_loop_threadsafe" not in udp_consumers
    assert udp_consumers == initial_udp


def test_require_wake_loop_threadsafe__esp32_no_udp_socket() -> None:
    """Test that ESP32 uses task notifications instead of UDP socket."""
    _setup_platform(PLATFORM_ESP32)
    CORE.config = {"wifi": True}
    socket.require_wake_loop_threadsafe()

    # Verify the define was added
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)

    # Verify no UDP socket was consumed (ESP32 uses FreeRTOS task notifications)
    udp_consumers = CORE.data.get(socket.KEY_SOCKET_CONSUMERS_UDP, {})
    assert "socket.wake_loop_threadsafe" not in udp_consumers


def test_require_wake_loop_threadsafe__non_esp32_consumes_udp_socket() -> None:
    """Test that non-ESP32 platforms consume a UDP socket for wake notifications."""
    _setup_platform(PLATFORM_ESP8266)
    CORE.config = {"wifi": True}
    socket.require_wake_loop_threadsafe()

    # Verify UDP socket was consumed
    udp_consumers = CORE.data.get(socket.KEY_SOCKET_CONSUMERS_UDP, {})
    assert udp_consumers.get("socket.wake_loop_threadsafe") == 1
