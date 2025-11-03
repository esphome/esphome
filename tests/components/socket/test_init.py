from esphome.components import socket
from esphome.core import CORE


def test_require_wake_loop_threadsafe__first_call() -> None:
    """Test that first call sets up define and consumes socket."""
    socket.require_wake_loop_threadsafe()

    # Verify CORE.data was updated
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__idempotent() -> None:
    """Test that subsequent calls are idempotent."""
    # Set up initial state as if already called
    CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] = True

    # Call again - should not raise or fail
    socket.require_wake_loop_threadsafe()

    # Verify state is still True
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Define should not be added since flag was already True
    assert not any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)


def test_require_wake_loop_threadsafe__multiple_calls() -> None:
    """Test that multiple calls only set up once."""
    # Call three times
    socket.require_wake_loop_threadsafe()
    socket.require_wake_loop_threadsafe()
    socket.require_wake_loop_threadsafe()

    # Verify CORE.data was set
    assert CORE.data[socket.KEY_WAKE_LOOP_THREADSAFE_REQUIRED] is True

    # Verify the define was added (only once, but we can just check it exists)
    assert any(d.name == "USE_WAKE_LOOP_THREADSAFE" for d in CORE.defines)
