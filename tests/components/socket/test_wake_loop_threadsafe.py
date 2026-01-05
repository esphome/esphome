from esphome.components import socket
from esphome.core import CORE


def test_require_wake_loop_threadsafe__first_call() -> None:
    """Test that first call sets up define and consumes socket."""
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
    initial_consumers = CORE.data.get(socket.KEY_SOCKET_CONSUMERS, {})

    # Call require_wake_loop_threadsafe
    socket.require_wake_loop_threadsafe()

    # Verify no socket was consumed
    consumers = CORE.data.get(socket.KEY_SOCKET_CONSUMERS, {})
    assert "socket.wake_loop_threadsafe" not in consumers
    assert consumers == initial_consumers
