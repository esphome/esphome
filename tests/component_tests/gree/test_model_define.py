"""The YAW1F follow-up message is compiled in only for configs that use that model."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_yaw1f_model_emits_its_define(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("yaw1f.yaml"))
    assert "USE_GREE_YAW1F" in {define.name for define in CORE.defines}


def test_other_models_leave_the_define_out(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("yan.yaml"))
    assert "USE_GREE_YAW1F" not in {define.name for define in CORE.defines}
