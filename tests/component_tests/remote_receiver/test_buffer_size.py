"""An explicit buffer_size still reaches the RMT receiver."""

from collections.abc import Callable
from pathlib import Path


def test_explicit_buffer_size_is_passed_through(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_buffer_size.yaml"))
    assert "rcvr->set_buffer_size(2000);" in main_cpp
