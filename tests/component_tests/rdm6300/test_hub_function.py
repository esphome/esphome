"""Tests for the function that publishes the rdm6300 card binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_cards_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    # A bool function: every card is checked, and it returns whether one matched
    assert (
        "static bool esphome__reader__cards_fn(uint32_t esphome__arg_tag) {\n"
        "  bool esphome__found = false;\n"
        "  (esphome__found |= rdm6300::publish_card(::card_a, esphome__arg_tag, 7616525));\n"
        "  (esphome__found |= rdm6300::publish_card(::card_b, esphome__arg_tag, 4294967295UL));\n"
        "  return esphome__found;\n"
        "}"
    ) in main_cpp
    assert "reader->set_cards_publisher(esphome__reader__cards_fn);" in main_cpp
