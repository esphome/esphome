"""on_raw passes a view of the frame, and a vector copy only to an automation that defers."""

from collections.abc import Callable
from pathlib import Path


def test_on_raw_owns_x_only_when_deferring(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_on_raw.yaml"))
    assert "remote_base::RawTrigger<std::span<const int32_t>>" in main_cpp
    assert "remote_base::RawTrigger<std::vector<int32_t>>" in main_cpp
