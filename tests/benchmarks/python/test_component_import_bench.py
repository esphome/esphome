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
process, while a change to the core moves no number here.

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
COMPONENT_PREFIX = "esphome.components."


@pytest.fixture
def restore_modules() -> Generator[None]:
    """Put the module objects that were loaded before the case back afterwards.

    The case replaces every component module with a fresh copy; anything
    that imported the originals keeps using them, so they must be the ones
    left in ``sys.modules`` for later tests.
    """
    saved = dict(sys.modules)
    yield
    sys.modules.clear()
    sys.modules.update(saved)


def _import_component(name: str) -> object:
    for module in [m for m in sys.modules if m.startswith(COMPONENT_PREFIX)]:
        del sys.modules[module]
    return importlib.import_module(COMPONENT_PREFIX + name)


@pytest.mark.usefixtures("restore_modules")
@pytest.mark.parametrize("component", COMPONENTS)
def test_import_component(benchmark, component: str) -> None:
    benchmark(_import_component, component)
