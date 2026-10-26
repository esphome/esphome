"""Code generation tests for the enabled switch.

The client stays stopped until the hub has an enabled state: codegen sets it when
no enabled switch is configured, and the switch answers from its restore otherwise.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

GenerateMain = Callable[[str | Path], str]
ConfigPath = Callable[[str], Path]


def test_without_switch_codegen_enables_the_client(
    generate_main: GenerateMain, component_config_path: ConfigPath
) -> None:
    """With no enabled switch, nothing else would start the client."""
    main_cpp = generate_main(component_config_path("device_info_default.yaml"))

    assert "->set_enabled(true);" in main_cpp


@pytest.mark.parametrize(
    ("config", "restore_mode"),
    [
        ("switch_enabled.yaml", "SWITCH_RESTORE_DEFAULT_ON"),
        ("switch_enabled_disabled_restore.yaml", "SWITCH_RESTORE_DISABLED"),
    ],
)
def test_switch_answers_the_hub(
    generate_main: GenerateMain,
    component_config_path: ConfigPath,
    config: str,
    restore_mode: str,
) -> None:
    """The hub waits for the switch's restore instead of being enabled by codegen."""
    main_cpp = generate_main(component_config_path(config))

    assert "->set_enabled(true);" not in main_cpp
    assert f"sendspin_enabled->set_restore_mode(switch_::{restore_mode});" in main_cpp
    assert "App.register_switch(sendspin_enabled," in main_cpp


def test_switch_sets_the_hub_then_publishes(
    generate_main: GenerateMain, component_config_path: ConfigPath
) -> None:
    """Turning the switch sets the hub first, then publishes."""
    main_cpp = generate_main(component_config_path("switch_enabled.yaml"))

    set_enabled = main_cpp.index("->set_enabled(state);")
    assert main_cpp.index("entity->publish_state(state);", set_enabled) > set_enabled
    assert "->set_enabled_switch(sendspin_enabled);" in main_cpp
