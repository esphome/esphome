"""Tests for the display external writer registry."""

from collections.abc import Callable, Generator
from pathlib import Path
import sys

import pytest

from esphome import loader
from esphome.components.display import (
    Display,
    get_external_writers,
    has_external_writer,
    register_external_writer,
)
from esphome.core import ID
from esphome.final_validate import full_config

TEST_CARD = "show_test_card()"
UPDATE_NEVER = "set_update_interval(4294967295UL)"


@pytest.fixture
def unload_fake_writer() -> Generator[None]:
    """Drop the fake external component so later tests start clean."""
    yield
    loader._COMPONENT_CACHE.pop("fake_display_writer", None)
    sys.modules.pop("esphome.components.fake_display_writer", None)
    loader.clear_component_meta_finders()


def test_unresolved_id_matches_after_resolution() -> None:
    """An auto-picked ID registered during schema validation matches once resolved."""
    pending = ID(None, is_declaration=False, type=Display)
    register_external_writer(pending, "owner")
    declared = ID("epd", is_declaration=True, type=Display)

    assert get_external_writers(declared) == []
    assert has_external_writer(declared) is False

    pending.id = "epd"
    assert get_external_writers(declared) == ["owner"]
    assert has_external_writer(declared) is True
    assert get_external_writers(ID("other", is_declaration=True, type=Display)) == []


def test_all_owners_returned() -> None:
    register_external_writer(ID("epd"), "first")
    register_external_writer(ID("epd"), "second")
    register_external_writer(ID("other"), "third")

    assert get_external_writers(ID("epd")) == ["first", "second"]


def test_lvgl_counts_as_external_writer() -> None:
    assert has_external_writer(ID("epd")) is False
    full_config.get()["lvgl"] = []
    assert has_external_writer(ID("epd")) is True


def test_drivers_show_test_card_without_owner(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("no_owner.yaml"))

    assert f"it8951_display->{TEST_CARD}" in main_cpp
    assert f"epaper_display->{TEST_CARD}" in main_cpp
    assert UPDATE_NEVER not in main_cpp


@pytest.mark.usefixtures("unload_fake_writer")
@pytest.mark.parametrize(
    "config_file", ["it8951_owner_auto_id.yaml", "epaper_spi_owner_auto_id.yaml"]
)
def test_owned_display_has_no_test_card(
    config_file: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The owner picks the display ID automatically, so it resolves after the schema runs."""
    main_cpp = generate_main(component_config_path(config_file))

    assert TEST_CARD not in main_cpp
    assert UPDATE_NEVER in main_cpp


@pytest.mark.usefixtures("unload_fake_writer")
def test_only_owned_display_loses_test_card(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("owner_by_id.yaml"))

    assert f"it8951_display->{TEST_CARD}" in main_cpp
    assert f"epaper_display->{TEST_CARD}" not in main_cpp
    assert f"epaper_display->{UPDATE_NEVER}" in main_cpp
    assert f"it8951_display->{UPDATE_NEVER}" not in main_cpp
