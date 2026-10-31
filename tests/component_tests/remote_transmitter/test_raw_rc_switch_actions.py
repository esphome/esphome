"""transmit_raw and the rc_switch transmit actions as generated code."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.remote_base import _rc_switch_bits, _rc_switch_letter_index

CONFIG = "raw_rc_switch_actions.yaml"

_TRANSMIT_BODY = re.compile(
    r"auto esphome__call = ::tx->transmit\(\);\n(.*?)esphome__call\.perform\(\);",
    re.DOTALL,
)


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    # Without the #line directives a lambda body is the user's text alone
    main = generate_main(component_config_path(CONFIG))
    return re.sub(r"\n *(#line .*|// ESPHOME_LINE_DIRECTIVE_RESET)(?=\n)", "", main)


@pytest.fixture
def bodies(main_cpp: str) -> list[str]:
    """The statements of each transmit: the script's two, then one per button."""
    found = _TRANSMIT_BODY.findall(main_cpp)
    assert len(found) == 14
    return found


def _in_order(body: str, *parts: str) -> None:
    positions = [body.index(part) for part in parts]
    assert positions == sorted(positions)


def test_no_action_objects(main_cpp: str) -> None:
    assert "RawAction" not in main_cpp
    assert "RCSwitch" not in main_cpp.replace("RCSwitchBase", "")
    # The script's two transmits merge into one action; the buttons call functions directly
    assert main_cpp.count("new(applyaction_id") == 1
    assert main_cpp.count("new(automation_id") == 1


def test_raw_static_code_in_flash(main_cpp: str, bodies: list[str]) -> None:
    # Both buttons send the same code from one flash table
    assert (
        main_cpp.count(
            "static constexpr int32_t remote_base_raw_code[] PROGMEM = "
            "{1000, -1000, 500, -500};"
        )
        == 1
    )
    raw = bodies[2]
    assert (
        "remote_base::encode_raw_timings(esphome__call.get_data(), "
        "::remote_base_raw_code, 4);"
    ) in raw
    # transmit() already resets the carrier frequency to 0
    assert "set_carrier_frequency" not in raw
    # Without repeat the call sends once
    assert "set_send_times" not in raw
    assert "set_send_wait" not in raw


def test_raw_repeat_and_carrier(bodies: list[str]) -> None:
    _in_order(
        bodies[3],
        "encode_raw_timings(",
        "set_carrier_frequency(38000);",
        "esphome__call.set_send_times(2);",
        "esphome__call.set_send_wait(10000);",
    )


def test_raw_lambda_code(bodies: list[str]) -> None:
    assert (
        "esphome__call.get_data()->set_data([]() -> std::vector<int32_t> {\n"
        "    return {2000, -1000};\n"
        "  }());"
    ) in bodies[4]


def test_rc_switch_raw(bodies: list[str]) -> None:
    # The code is decoded at build time; every character but 0 is a one
    assert (
        "remote_base::rc_switch_protocol(2).transmit(esphome__call.get_data(), "
        "2727775, 24);"
    ) in bodies[5]
    # The rc_switch actions also send once without repeat
    assert "set_send_times" not in bodies[5]
    lambda_ = bodies[6]
    _in_order(
        lambda_,
        "const std::string esphome__code = []() -> std::string {\n"
        '    return std::string("0010100110");',
        "remote_base::rc_switch_protocol(1).transmit(esphome__call.get_data(), "
        "remote_base::decode_binary_string(esphome__code), esphome__code.size());",
        "esphome__call.set_send_times(2);",
        "esphome__call.set_send_wait(10000);",
    )


@pytest.mark.parametrize(
    ("code", "value"),
    [("0", 0), ("1", 1), ("x", 1), ("10x0", 0b1010), ("1" * 64, 2**64 - 1)],
)
def test_rc_switch_bits_match_decode_binary_string(code: str, value: int) -> None:
    assert _rc_switch_bits(code) == value


@pytest.mark.parametrize(("letter", "index"), [("a", 0), ("C", 2), ("p", 15)])
def test_rc_switch_letter_index_matches_tolower(letter: str, index: int) -> None:
    assert _rc_switch_letter_index(letter) == index


def test_rc_switch_type_a(bodies: list[str]) -> None:
    assert (
        "remote_base::RCSwitchBase::type_a_code(25, 8, true, "
        "&esphome__code, &esphome__nbits);"
    ) in bodies[7]
    assert (
        "remote_base::rc_switch_protocol_copy(rc_switch_custom_protocol).transmit("
        "esphome__call.get_data(), esphome__code, esphome__nbits);"
    ) in bodies[7]
    _in_order(
        bodies[8],
        "const auto esphome__group = remote_base::decode_binary_string("
        '[]() -> std::string {\n    return std::string("11001");',
        "const auto esphome__device = remote_base::decode_binary_string(",
        "const auto esphome__state = []() -> bool {\n    return false;",
        "remote_base::RCSwitchBase::type_a_code(esphome__group, esphome__device, "
        "esphome__state, &esphome__code, &esphome__nbits);",
        "remote_base::rc_switch_protocol(1).transmit(",
    )


def test_rc_switch_type_b(bodies: list[str]) -> None:
    assert (
        "remote_base::RCSwitchBase::type_b_code(4, 2, true, "
        "&esphome__code, &esphome__nbits);"
    ) in bodies[9]


def test_rc_switch_type_c(bodies: list[str]) -> None:
    assert (
        "remote_base::RCSwitchBase::type_c_code(2, 1, 2, true, "
        "&esphome__code, &esphome__nbits);"
    ) in bodies[10]
    _in_order(
        bodies[11],
        "const auto esphome__family = static_cast<uint8_t>(tolower("
        "[]() -> std::string {\n    return std::string(\"C\");\n  }()[0]) - 'a');",
        "const auto esphome__group = []() -> uint8_t {",
        "const auto esphome__device = []() -> uint8_t {",
        "const auto esphome__state = []() -> bool {",
        "remote_base::RCSwitchBase::type_c_code(esphome__family, esphome__group, "
        "esphome__device, esphome__state, &esphome__code, &esphome__nbits);",
    )


def test_rc_switch_type_d(bodies: list[str]) -> None:
    assert (
        "remote_base::RCSwitchBase::type_d_code(1, 3, false, "
        "&esphome__code, &esphome__nbits);"
    ) in bodies[12]
    assert (
        "const auto esphome__group = static_cast<uint8_t>(tolower("
        "[]() -> std::string {\n    return std::string(\"b\");\n  }()[0]) - 'a');"
    ) in bodies[13]


def test_lambdas_take_args_by_value(bodies: list[str]) -> None:
    """User lambdas get the script argument by value, so they may modify it.

    They are called rather than reduced, so even a single return modifies only its own copy.
    """
    raw, type_b = bodies[0], bodies[1]
    _in_order(
        raw,
        "set_data([](int x) -> std::vector<int32_t> {\n"
        "    x *= 2;\n    return {x, -1000};\n  }(x));",
        "set_carrier_frequency([](int x) -> uint32_t {\n"
        "    return x *= 1000;\n  }(x));",
        "set_send_times([](int x) -> uint32_t {\n    return x *= 1;\n  }(x));",
        "set_send_wait([](int x) -> uint32_t {\n    x *= 3; return x;\n  }(x));",
    )
    _in_order(
        type_b,
        "const auto esphome__address = [](int x) -> uint8_t {\n"
        "    x *= 2; return x;\n  }(x);",
        "const auto esphome__channel = [](int x) -> uint8_t {\n"
        "    return x *= 1;\n  }(x);",
        "remote_base::RCSwitchBase::type_b_code(",
        # The protocol is read last, as before
        "[](int x) -> remote_base::RCSwitchBase {\n"
        "    x *= 1; return remote_base::rc_switch_protocol(x);\n"
        "  }(x).transmit(esphome__call.get_data(), esphome__code, esphome__nbits);",
    )
