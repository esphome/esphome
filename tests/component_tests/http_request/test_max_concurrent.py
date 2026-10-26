"""max_concurrent_requests is set only when it differs from the default of 1."""

from collections.abc import Callable
from pathlib import Path


def test_max_concurrent_requests_default_is_not_set(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_esp32_stock.yaml"))
    assert "set_max_concurrent_requests" not in main_cpp


def test_max_concurrent_requests_is_set(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("max_concurrent.yaml"))
    assert "->set_max_concurrent_requests(3);" in main_cpp
