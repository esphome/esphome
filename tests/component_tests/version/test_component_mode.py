"""Component mode emits a separate class, and only when the component exists."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import version as version_component
from esphome.components.version import text_sensor as version_text_sensor


def test_component_mode_uses_its_own_class(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("component_mode.yaml"))

    assert "version::ComponentVersionTextSensor" in main_cpp
    assert "version::VersionTextSensor" not in main_cpp
    assert "set_component_name" in main_cpp


def test_esphome_mode_is_untouched(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    # The whole point of the split: a plain version sensor gains neither the
    # component fields nor the component class.
    main_cpp = generate_main(component_config_path("esphome_mode.yaml"))

    assert "version::VersionTextSensor" in main_cpp
    assert "ComponentVersionTextSensor" not in main_cpp
    assert "set_component_name" not in main_cpp


def test_declared_version_is_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(version_component, "COMPONENT_VERSION", "1.2.3", raising=False)

    main_cpp = generate_main(component_config_path("component_mode.yaml"))

    assert 'set_version("1.2.3")' in main_cpp


def test_undeclared_version_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    # version declares no COMPONENT_VERSION, which is the common case: it must
    # compile, warn, and leave the state unpublished rather than publish "".
    main_cpp = generate_main(component_config_path("component_mode.yaml"))

    assert "set_version" not in main_cpp
    assert "does not report a version" in caplog.text


def test_unknown_component_is_a_config_error() -> None:
    # Otherwise a typo is indistinguishable from a component that declares no
    # version: both leave the sensor reading unknown forever.
    config = version_text_sensor.CONFIG_SCHEMA(
        {"component": "not_a_component", "name": "Test"}
    )
    with pytest.raises(cv.Invalid, match="not found"):
        version_text_sensor.FINAL_VALIDATE_SCHEMA(config)


def test_hide_options_are_rejected_in_component_mode() -> None:
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        version_text_sensor.CONFIG_SCHEMA(
            {
                "component": "version",
                "name": "Test",
                "hide_hash": True,
            }
        )


@pytest.mark.parametrize("name", ["Version", "my-component", "1component", ""])
def test_invalid_component_names_are_rejected(name: str) -> None:
    with pytest.raises(cv.Invalid, match="not a valid component name"):
        version_text_sensor.CONFIG_SCHEMA({"component": name, "name": "Test"})
