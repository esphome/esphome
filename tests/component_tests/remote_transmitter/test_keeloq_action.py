"""transmit_keeloq sets the suffix only when suffix bits are sent."""

from collections.abc import Callable
from pathlib import Path


def test_keeloq_suffix_is_set_only_with_suffix_bits(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("keeloq_suffix.yaml"))

    # The plain word gets the same setters as before the suffix existed.
    assert "remote_base_keeloqaction_id->set_suffix" not in main_cpp
    # A constant and a lambda width both set the suffix.
    for action in ("remote_base_keeloqaction_id_2", "remote_base_keeloqaction_id_3"):
        assert f"{action}->set_suffix([]() -> uint16_t" in main_cpp
        assert f"{action}->set_suffix_bits([]() -> uint8_t" in main_cpp
