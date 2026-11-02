"""Tests for the Mitsubishi CN105 vane direction selects."""

from collections.abc import Callable
from pathlib import Path

from esphome.config import load_config
from esphome.core import CORE


def test_one_select_entry_per_hub_is_accepted(
    component_fixture_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_fixture_path("select_one_entry.yaml")
    assert not load_config({}).errors


def test_second_select_entry_for_the_hub_is_rejected(
    component_fixture_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_fixture_path("select_two_entries.yaml")
    errors = load_config({}).errors
    assert len(errors) == 1
    assert "Only one mitsubishi_cn105 select entry is allowed per hub" in str(errors[0])
