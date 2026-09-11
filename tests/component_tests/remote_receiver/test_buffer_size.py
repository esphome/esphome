"""buffer_size reaches the receiver when set, and always on the pulse ring targets."""

from collections.abc import Callable
from pathlib import Path


def test_explicit_buffer_size_is_passed_through(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_buffer_size.yaml"))
    assert "rcvr->set_buffer_size(2000);" in main_cpp


def test_pulse_ring_target_keeps_a_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_esp8266.yaml"))
    assert "rcvr->set_buffer_size(1000);" in main_cpp
