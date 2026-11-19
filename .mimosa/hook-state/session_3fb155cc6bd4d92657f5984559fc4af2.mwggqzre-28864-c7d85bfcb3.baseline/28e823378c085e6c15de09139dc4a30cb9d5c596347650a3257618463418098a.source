"""The Hisense ARG code is compiled in only when that model is configured."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

DEFINE = "USE_WHIRLPOOL_HISENSE_ARG"
HERE = Path(__file__).parent


def _define_names() -> set[str]:
    return {define.name for define in CORE.defines}


def test_hisense_arg_model_emits_the_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(HERE / "test_model_define.yaml")
    assert DEFINE in _define_names()


def test_dg11j1_model_does_not_emit_the_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(HERE / "test_model_define_dg11j1.yaml")
    assert DEFINE not in _define_names()
