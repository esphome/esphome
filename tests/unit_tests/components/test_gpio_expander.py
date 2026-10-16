"""Tests for the shared GPIO expander pin mode validators."""

import pytest

from esphome.components import gpio_expander
import esphome.config_validation as cv


def _mode(input_: bool, output: bool, pullup: bool = False) -> dict[str, bool]:
    return {"input": input_, "output": output, "pullup": pullup}


@pytest.mark.parametrize(("input_", "output"), [(True, False), (False, True)])
def test_input_xor_output_accepts_one(input_: bool, output: bool) -> None:
    mode = _mode(input_, output)
    assert gpio_expander.validate_input_xor_output(mode) is mode


@pytest.mark.parametrize(("input_", "output"), [(True, True), (False, False)])
def test_input_xor_output_rejects_both_or_neither(input_: bool, output: bool) -> None:
    with pytest.raises(cv.Invalid, match="either input or output"):
        gpio_expander.validate_input_xor_output(_mode(input_, output))


def test_pullup_allowed_on_input() -> None:
    mode = _mode(True, False, pullup=True)
    assert gpio_expander.validate_input_xor_output_pullup(mode) is mode


def test_pullup_rejected_on_output() -> None:
    with pytest.raises(cv.Invalid, match="Pullup only available with input"):
        gpio_expander.validate_input_xor_output_pullup(_mode(False, True, pullup=True))


def test_pullup_variant_still_requires_one_mode() -> None:
    with pytest.raises(cv.Invalid, match="either input or output"):
        gpio_expander.validate_input_xor_output_pullup(_mode(True, True))
