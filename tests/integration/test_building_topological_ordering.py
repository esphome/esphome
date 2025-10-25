from __future__ import annotations

from pathlib import Path

import pytest

from .types import CompileOutput, ConfigWriter, OnlyCompileFunction


@pytest.mark.asyncio
async def test_building_missing_dependency_component(
    yaml_config: str, write_yaml_config: ConfigWriter, compile_only: OnlyCompileFunction
) -> None:
    """Compiles a config where a component DEPENDENCIES lists a missing component.

    Expectation: compilation fails with a clear message about the missing dependency.
    """

    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    config_path: Path = await write_yaml_config(yaml_config, None)
    result: CompileOutput = await compile_only(config_path)

    # Should fail because a declared dependency is missing
    assert result.returncode != 0

    # Expect a clear error about the missing dependency
    combined = result.stdout + "\n" + result.stderr
    assert (
        "Component building_missing_dependency_component requires component absent_component_xyz"
        in combined
    ), combined


@pytest.mark.asyncio
async def test_building_missing_auto_load_component(
    yaml_config: str, write_yaml_config: ConfigWriter, compile_only: OnlyCompileFunction
) -> None:
    """Compiles a config where a component AUTO_LOADs a missing target.

    Expectation: compilation fails with a clear message about the missing component.
    """

    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    config_path: Path = await write_yaml_config(yaml_config, None)
    result: CompileOutput = await compile_only(config_path)

    # Should fail because AUTO_LOAD points to a missing component
    assert result.returncode != 0

    combined = result.stdout + "\n" + result.stderr
    assert "Component not found: absent_component_xyz" in combined, combined


@pytest.mark.asyncio
async def test_building_topological_component_initialization_order(
    yaml_config: str, write_yaml_config: ConfigWriter, compile_only: OnlyCompileFunction
) -> None:
    """Verify that initialization order follows topological order constraints"""
    external_components_path: str = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    config_path: Path = await write_yaml_config(yaml_config, None)
    result: CompileOutput = await compile_only(config_path)

    assert result.returncode == 0, result.stderr or result.stdout

    # Look for the Initialization order line and check relative positions
    combined = result.stdout + "\n" + result.stderr
    # Find the last occurrence to be robust against multiple runs/log blocks
    marker = "Initialization order: "
    idx = combined.rfind(marker)
    assert idx != -1, f"Missing '{marker}' log line.\nLogs:\n{combined}"
    order_str = combined[idx + len(marker) :].splitlines()[0].strip()
    names = [s.strip() for s in order_str.split(",")]

    # Helper to get index with assertion
    def pos(name: str) -> int:
        assert name in names, f"{name} not found in order: {names}"
        return names.index(name)

    assert pos("host") < pos("esphome"), order_str
    assert pos("esphome") < pos("logger"), order_str

    for comp in (
        "building_topological_component_ordering",
        "network",
        "api",
        "socket",
        "mdns",
        "preferences",
    ):
        assert pos("esphome") < pos(comp), order_str
        assert pos("host") < pos(comp), order_str
        assert pos("logger") < pos(comp), order_str

    assert pos("logger") < pos("building_topological_component_ordering"), order_str
    assert pos("network") < pos("api"), order_str
    assert pos("api") < pos("socket"), order_str
    assert pos("api") < pos("mdns"), order_str
    assert pos("host") < pos("preferences"), order_str


@pytest.mark.asyncio
async def test_building_cyclic_dependency(
    yaml_config: str, write_yaml_config: ConfigWriter, compile_only: OnlyCompileFunction
) -> None:
    """Compiles the test fixture YAML and asserts cyclic dependency is reported."""
    external_components_path: str = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    config_path: Path = await write_yaml_config(yaml_config, None)
    result: CompileOutput = await compile_only(config_path)

    assert result.returncode != 0
    assert (
        "Circular dependency detected among components: building_cyclic_dependency"
        in result.stderr
    )
