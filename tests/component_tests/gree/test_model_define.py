"""GREE code paths are compiled in only for configs that use them."""

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


def test_receiver_and_switch_defines_follow_the_config(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("yx1ff_switch.yaml"))
    defines = {define.name for define in CORE.defines}
    assert {"USE_GREE_RECEIVER", "USE_GREE_FEATURE_SWITCH"} <= defines
    # YX1FF receives the light state over IR, so the switch never restores its own
    assert (
        "yx1ff_light->set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);" in main_cpp
    )


def test_transmit_only_config_leaves_the_receiver_out(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("yan.yaml"))
    defines = {define.name for define in CORE.defines}
    assert "USE_GREE_RECEIVER" not in defines
    assert "USE_GREE_FEATURE_SWITCH" not in defines
