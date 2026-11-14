"""CodSpeed benchmarks for naming the globals that code generation emits.

Every action, condition and filter built with the apply helpers emits a static
function, and many components emit PROGMEM tables, each named by
``cg.unique_global_id`` so the name cannot clash with a config id. A large
config emits hundreds of them, so the cost of one name must not grow with the
size of the config.

Skipped when ``pytest-codspeed`` isn't installed so the regular unit-test
suite keeps working unchanged.
"""

from __future__ import annotations

import pytest

from esphome import codegen as cg
from esphome.core import CORE, ID

pytest.importorskip("pytest_codspeed")

CONFIG_IDS = 1000
GLOBALS = 300


def _large_config() -> dict[str, list[dict[str, ID]]]:
    return {
        "sensor": [
            {"id": ID(f"sensor_{i}", is_declaration=True)} for i in range(CONFIG_IDS)
        ]
    }


def _setup() -> tuple[tuple, dict]:
    CORE.reset()
    CORE.config = _large_config()
    return (), {}


def _emit_functions() -> None:
    for i in range(GLOBALS):
        cg.shared_static_function(
            f"esphome__action_{i}__fn", cg.void, [], ["return;"], share=False
        )


def test_static_function_names_large_config(benchmark) -> None:
    """Unshared static functions, as for actions with lambdas, in a 1000 id config."""
    benchmark.pedantic(_emit_functions, setup=_setup, rounds=3, iterations=1)
