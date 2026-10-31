"""transmit_raw and the rc_switch transmit actions as generated code."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

CONFIG = "raw_rc_switch_actions.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    # Without the #line directives a lambda body is the user's text alone
    main = generate_main(component_config_path(CONFIG))
    return re.sub(r"\n *(#line .*|// ESPHOME_LINE_DIRECTIVE_RESET)(?=\n)", "", main)


def _action(main_cpp: str, action_id: str) -> str:
    """The setup statements of one action object."""
    start = main_cpp.index(f"new({action_id}) ")
    return main_cpp[start : main_cpp.index("new(automation_id", start)]


def test_raw_static_code_in_flash(main_cpp: str) -> None:
    raw = _action(main_cpp, "remote_base_rawaction_id_2")
    assert (
        "static constexpr int32_t int32_t_id_2[] PROGMEM = {1000, -1000, 500, -500};"
        in raw
    )
    assert "->set_code_static(int32_t_id_2, 4);" in raw
    assert "->set_carrier_frequency([]() -> uint32_t {\n    return 0;\n});" in raw
    # Without repeat the call sends once
    assert "set_send_times" not in raw
    assert "set_send_wait" not in raw


def test_raw_repeat_and_carrier(main_cpp: str) -> None:
    raw = _action(main_cpp, "remote_base_rawaction_id_3")
    assert "->set_send_times([]() -> uint32_t {\n    return 2;\n});" in raw
    assert "->set_send_wait([]() -> uint32_t {\n    return 10000;\n});" in raw
    assert "->set_carrier_frequency([]() -> uint32_t {\n    return 38000;\n});" in raw


def test_raw_lambda_code(main_cpp: str) -> None:
    raw = _action(main_cpp, "remote_base_rawaction_id_4")
    assert (
        "->set_code_template([]() -> std::vector<int32_t> {\n"
        "    return {2000, -1000};\n"
        "});"
    ) in raw


def test_rc_switch_raw(main_cpp: str) -> None:
    constant = _action(main_cpp, "remote_base_rcswitchrawaction_id")
    assert "return remote_base::rc_switch_protocol(2);" in constant
    assert '->set_code(ESPHOME_F("00101001100111110101xxxx"));' in constant
    # The rc_switch actions also send once without repeat
    assert "set_send_times" not in constant
    lambda_ = _action(main_cpp, "remote_base_rcswitchrawaction_id_2")
    assert "return remote_base::rc_switch_protocol(1);" in lambda_
    assert '    return std::string("0010100110");' in lambda_
    assert "->set_send_times([]() -> uint32_t {\n    return 2;\n});" in lambda_
    assert "->set_send_wait([]() -> uint32_t {\n    return 10000;\n});" in lambda_


def test_rc_switch_type_a(main_cpp: str) -> None:
    constant = _action(main_cpp, "remote_base_rcswitchtypeaaction_id")
    assert (
        "return remote_base::rc_switch_protocol_copy(rc_switch_custom_protocol);"
        in constant
    )
    assert '->set_group(ESPHOME_F("11001"));' in constant
    assert '->set_device(ESPHOME_F("01000"));' in constant
    assert "->set_state([]() -> bool {\n    return true;\n});" in constant
    lambda_ = _action(main_cpp, "remote_base_rcswitchtypeaaction_id_2")
    assert '->set_group([]() -> std::string {\n    return std::string("11001");' in (
        lambda_
    )
    assert '->set_device([]() -> std::string {\n    return std::string("01000");' in (
        lambda_
    )
    assert "->set_state([]() -> bool {\n    return false;" in lambda_


def test_rc_switch_type_b(main_cpp: str) -> None:
    constant = _action(main_cpp, "remote_base_rcswitchtypebaction_id_2")
    assert "->set_address([]() -> uint8_t {\n    return 4;\n});" in constant
    assert "->set_channel([]() -> uint8_t {\n    return 2;\n});" in constant
    assert "->set_state([]() -> bool {\n    return true;\n});" in constant


def test_rc_switch_type_c(main_cpp: str) -> None:
    constant = _action(main_cpp, "remote_base_rcswitchtypecaction_id")
    assert '->set_family(ESPHOME_F("c"));' in constant
    assert "->set_group([]() -> uint8_t {\n    return 1;\n});" in constant
    assert "->set_device([]() -> uint8_t {\n    return 2;\n});" in constant
    lambda_ = _action(main_cpp, "remote_base_rcswitchtypecaction_id_2")
    assert '->set_family([]() -> std::string {\n    return std::string("C");' in (
        lambda_
    )


def test_rc_switch_type_d(main_cpp: str) -> None:
    constant = _action(main_cpp, "remote_base_rcswitchtypedaction_id")
    assert '->set_group(ESPHOME_F("b"));' in constant
    assert "->set_device([]() -> uint8_t {\n    return 3;\n});" in constant
    assert "->set_state([]() -> bool {\n    return false;\n});" in constant
    lambda_ = _action(main_cpp, "remote_base_rcswitchtypedaction_id_2")
    assert '->set_group([]() -> std::string {\n    return std::string("b");' in lambda_


def test_lambdas_take_args_by_value(main_cpp: str) -> None:
    """User lambdas get the script argument by value, so they may modify it."""
    raw = _action(main_cpp, "remote_base_rawaction_id")
    assert (
        "[](int x) -> std::vector<int32_t> {\n    x *= 2;\n    return {x, -1000};\n}"
        in raw
    )
    assert "[](int x) -> uint32_t {\n    return x *= 1000;\n}" in raw
    assert "[](int x) -> uint32_t {\n    return x *= 1;\n}" in raw
    assert "[](int x) -> uint32_t {\n    x *= 3; return x;\n}" in raw
    type_b = _action(main_cpp, "remote_base_rcswitchtypebaction_id")
    assert "[](int x) -> uint8_t {\n    x *= 2; return x;\n}" in type_b
    assert "[](int x) -> uint8_t {\n    return x *= 1;\n}" in type_b
    assert (
        "[](int x) -> remote_base::RCSwitchBase {\n"
        "    x *= 1; return remote_base::rc_switch_protocol(x);\n}"
    ) in type_b
