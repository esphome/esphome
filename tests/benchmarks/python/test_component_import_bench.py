"""CodSpeed benchmarks for importing entity components.

Every ``esphome config`` or ``esphome compile`` imports the Python module of
each component the YAML uses. A component that imports its optional
integrations (mqtt, web_server, zigbee) at module scope drags their whole
import chains in for every user, so the cost of a fresh import is what these
benchmarks pin down.

The interpreter is shared across iterations, so each run first drops the
esphome modules from ``sys.modules`` and imports the component again from
scratch; the import machinery and the module bodies run every time, as they
do in a new process. The modules loaded before the benchmark are put back
afterwards, so the rest of the test session keeps the objects it imported.

Skipped when ``pytest-codspeed`` isn't installed so the regular unit-test
suite keeps working unchanged.
"""

from __future__ import annotations

from collections.abc import Generator
import importlib
import sys

import pytest

pytest.importorskip("pytest_codspeed")

COMPONENTS = ("binary_sensor", "sensor", "logger")


def _esphome_modules() -> dict[str, object]:
    return {
        n: m
        for n, m in sys.modules.items()
        if n == "esphome" or n.startswith("esphome.")
    }


def _forget_esphome_modules() -> None:
    for name in _esphome_modules():
        del sys.modules[name]


def _import_component(name: str) -> object:
    _forget_esphome_modules()
    return importlib.import_module(f"esphome.components.{name}")


@pytest.fixture
def restore_esphome_modules() -> Generator[None]:
    saved = _esphome_modules()
    yield
    _forget_esphome_modules()
    sys.modules.update(saved)


@pytest.mark.usefixtures("restore_esphome_modules")
@pytest.mark.parametrize("component", COMPONENTS)
def test_import_component(benchmark, component: str) -> None:
    benchmark(_import_component, component)
