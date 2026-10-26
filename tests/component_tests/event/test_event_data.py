"""An event can carry typed values, declared as variables."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.event import check_variables, validate_variables
from esphome.core import CORE, EsphomeError


def test_variables_reach_the_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("event_data.yaml"))

    assert "USE_EVENT_DATA" in {define.name for define in CORE.defines}
    # Two ints, a float and a bool: 4 + 4 + 4 + 1 bytes
    assert "static event::EventDataBlock<13> dimmer__data;" in main_cpp
    assert "dimmer->set_data(&dimmer__data, event_variables, 4);" in main_cpp
    # 1 byte of bits each: number_of_presses goes with event type 0 (press), steps with 1 and 2
    assert "event_variable_types[] PROGMEM = {1, 1};" in main_cpp
    assert "event_variable_types_2[] PROGMEM = {1, 6};" in main_cpp
    # Each value goes with the index of its variable
    assert 'dimmer->trigger("rotate_left", {{1, 3}, {2, 0.5f}});' in main_cpp
    assert "{3, true}" in main_cpp
    # on_event reads each variable from the event, as the callback carries only the type
    assert "::dimmer->get_last_event_data().get_int(1)" in main_cpp


def test_text_variables_reach_the_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("event_data_string.yaml"))

    # An int, then 7 + 1 and 64 + 1 bytes of text (64 is the default max_length)
    assert "static event::EventDataBlock<77> dial__data;" in main_cpp
    assert '{"action", nullptr, 4, event::EVENT_VARIABLE_TYPE_STRING, 7}' in main_cpp
    assert '{"code", nullptr, 12, event::EVENT_VARIABLE_TYPE_STRING, 64}' in main_cpp
    assert (
        'dial->trigger("clock_wise", {{0, 3}, {1, "start"}, {2, "a1b2"}});' in main_cpp
    )
    # A text is a view into the data block, unless the automation waits
    assert "Automation<StringRef, int32_t, StringRef, StringRef>" in main_cpp
    assert "Automation<StringRef, int32_t, std::string, std::string>" in main_cpp


def test_event_without_variables_has_no_data(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("event_types.yaml"))

    assert "USE_EVENT_DATA" not in {define.name for define in CORE.defines}
    assert "set_data" not in main_cpp


def _trigger_config(
    tmp_path: Path,
    data: str,
    event_type: str = "rotate_left",
    event_id: str = "dimmer",
) -> Path:
    path = tmp_path / "test.yaml"
    path.write_text(f"""
esphome:
  name: test
esp8266:
  board: d1_mini
event:
  - platform: template
    id: dimmer
    name: Dimmer
    event_types: [press, rotate_left]
    variables:
      number_of_presses:
        type: int
        event_types: [press]
      steps: int
      speed: float
      action:
        type: string
        max_length: 7
  - platform: template
    id: doorbell
    name: Doorbell
    event_types: [ring]
button:
  - platform: template
    name: Turn
    on_press:
      - event.trigger:
          id: {event_id}
          event_type: {event_type}
          data:
            {data}
""")
    return path


@pytest.mark.parametrize(
    ("data", "error"),
    [
        ("stepz: 3", "event 'dimmer' has no variable 'stepz'"),
        ("number_of_presses: 2", "event type 'rotate_left' does not carry"),
        ("steps: 1.5", "steps: This option only accepts integers"),
        ("steps: true", "steps: Expected a number, got a boolean"),
        ("speed: true", "speed: Expected a number, got a boolean"),
        # The smallest int32_t, -2147483648, is a 64-bit literal in C++ and would not build
        ("steps: -2147483648", "steps: value must be at least -2147483647"),
        ("action: released", "action takes at most 7 bytes"),
        ("action: 7", "action: Must be string"),
    ],
)
def test_trigger_data_is_checked(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    data: str,
    error: str,
) -> None:
    with pytest.raises(EsphomeError, match=error):
        generate_main(_trigger_config(tmp_path, data))


def test_trigger_data_on_an_event_without_variables(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    path = _trigger_config(tmp_path, "steps: 3", event_type="ring", event_id="doorbell")
    with pytest.raises(EsphomeError, match="event 'doorbell' has no variables"):
        generate_main(path)


def test_lambda_event_type_is_checked_on_the_device(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    main_cpp = generate_main(
        _trigger_config(
            tmp_path, "number_of_presses: 2", event_type='!lambda return "press";'
        )
    )
    assert '"press", {{0, 2}}' in main_cpp


def test_short_and_full_form() -> None:
    assert validate_variables(
        {
            "steps": "int",
            "number_of_presses": {"type": "int", "event_types": "press"},
            "code": "string",
        }
    ) == {
        "steps": {"type": "int"},
        "number_of_presses": {"type": "int", "event_types": ["press"]},
        "code": {"type": "string", "max_length": 64},
    }


@pytest.mark.parametrize(
    ("variables", "error"),
    [
        ({"steps": "text"}, "Unknown value 'text'"),
        ({"step count": "int"}, "The character ' ' cannot be used"),
        ({"event_type": "int"}, "Home Assistant uses it"),
        ({"friendly_name": "int"}, "Home Assistant uses it"),
        ({"restored": "int"}, "Home Assistant uses it"),
        ({f"value_{i}": "int" for i in range(17)}, "at most 16 variables"),
        ({"steps": {"type": "int", "max_length": 7}}, "max_length is only for string"),
        # The API sends the length of a text in one byte
        ({"code": {"type": "string", "max_length": 128}}, "must be at most 127"),
        # A variable no event type carries could never be set
        ({"steps": {"type": "int", "event_types": []}}, "at least 1"),
    ],
)
def test_invalid_variables_are_rejected(variables: dict[str, str], error: str) -> None:
    with pytest.raises(cv.Invalid, match=error):
        validate_variables(variables)


def test_restriction_must_name_an_event_type() -> None:
    variables = validate_variables({"steps": {"type": "int", "event_types": ["turn"]}})
    with pytest.raises(cv.Invalid, match="'turn' is not one of the event types"):
        check_variables(variables, ["press", "rotate_left"])


def test_at_most_127_event_types_with_variables() -> None:
    # The API keeps the type index of a queued event in 7 bits
    check_variables(validate_variables({"steps": "int"}), ["type"] * 127)
    with pytest.raises(cv.Invalid, match="at most 127 event types"):
        check_variables(validate_variables({"steps": "int"}), ["type"] * 128)
