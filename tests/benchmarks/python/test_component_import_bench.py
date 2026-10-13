"""CodSpeed benchmarks for importing entity components.

Every ``esphome config`` or ``esphome compile`` imports the Python module of
each component the YAML uses. A component that imports its optional
integrations (mqtt, web_server, zigbee) at module scope drags their whole
import chains in for every user, so the cost of a fresh import is what these
benchmarks pin down.

The core of esphome is imported once up front and stays loaded; each run
drops only the component modules from ``sys.modules`` and imports the
component again, so the import machinery and the module bodies of the
component and everything it pulls in run every time, as they do in a new
process, while a change to the core moves no number here. The modules the
session had before a case are put back afterwards, so later tests keep the
objects they already hold.

Skipped when ``pytest-codspeed`` isn't installed so the regular unit-test
suite keeps working unchanged.
"""

from __future__ import annotations

from collections.abc import Generator
import importlib
import sys

import pytest

pytest.importorskip("pytest_codspeed")

# Load the core before the first case so no case pays for it.
import esphome.codegen  # noqa: E402, F401
import esphome.config_validation  # noqa: E402, F401

COMPONENTS = ("binary_sensor", "sensor", "logger")
COMPONENT_PACKAGE = "esphome.components"
COMPONENT_PREFIX = COMPONENT_PACKAGE + "."


def _component_modules() -> dict[str, object]:
    # The package itself goes too: ``from esphome.components import x`` is
    # answered from its attributes, so a stale package would hide the import.
    return {
        n: m
        for n, m in sys.modules.items()
        if n == COMPONENT_PACKAGE or n.startswith(COMPONENT_PREFIX)
    }


def _forget_component_modules() -> None:
    for name in _component_modules():
        del sys.modules[name]


def _import_component(name: str) -> object:
    _forget_component_modules()
    return importlib.import_module(COMPONENT_PREFIX + name)


@pytest.fixture
def restore_component_modules() -> Generator[None]:
    saved = _component_modules()
    yield
    _forget_component_modules()
    sys.modules.update(saved)


@pytest.mark.usefixtures("restore_component_modules")
@pytest.mark.parametrize("component", COMPONENTS)
def test_import_component(benchmark, component: str) -> None:
    benchmark(_import_component, component)
