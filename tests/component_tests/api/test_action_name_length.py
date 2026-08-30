"""Tests for the user-defined action name length limit."""

import pytest

from esphome.components.api import ACTION_NAME_MAX_LENGTH, validate_action_name
from esphome.config_validation import Invalid


def test_action_name_at_limit_is_accepted() -> None:
    name = "a" * ACTION_NAME_MAX_LENGTH
    assert validate_action_name(name) == name


def test_action_name_over_limit_is_rejected() -> None:
    with pytest.raises(Invalid):
        validate_action_name("a" * (ACTION_NAME_MAX_LENGTH + 1))
