"""Tests for iter_ids() in config.py."""

from esphome.config import iter_ids
from esphome.const import CONF_SUBSTITUTIONS
from esphome.core import ID, Lambda


def test_iter_ids_paths() -> None:
    """IDs in values, lists and lambdas are found with the path to them."""
    sensor_id = ID("my_sensor", is_declaration=True)
    first = ID("first", is_declaration=True)
    second = ID("second", is_declaration=True)
    config = {
        "sensor": {"id": sensor_id},
        "items": [{"id": first}, {"id": second}],
        "lambda": Lambda("return id(other).state;"),
    }

    found = list(iter_ids(config))

    assert [(id_.id, path) for id_, path in found] == [
        ("my_sensor", ["sensor", "id"]),
        ("first", ["items", 0, "id"]),
        ("second", ["items", 1, "id"]),
        ("other", ["lambda"]),
    ]
    assert found[0][0] is sensor_id


def test_iter_ids_id_as_key() -> None:
    """An ID used as a mapping key is found at the path of that mapping."""
    key_id = ID("key_id", is_declaration=True)
    value_id = ID("value_id", is_declaration=True)
    config = {"outer": {key_id: {"id": value_id}}}

    found = [(id_, path) for id_, path in iter_ids(config)]

    assert found == [(key_id, ["outer"]), (value_id, ["outer", key_id, "id"])]
    assert found[0][0] is key_id


def test_iter_ids_skips_top_level_substitutions() -> None:
    """IDs under top-level substitutions are ignored, but not deeper ones."""
    config = {
        CONF_SUBSTITUTIONS: {"name": ID("skipped", is_declaration=True)},
        "component": {CONF_SUBSTITUTIONS: {"name": ID("kept", is_declaration=True)}},
    }

    found = [(id_.id, path) for id_, path in iter_ids(config)]

    assert found == [("kept", ["component", CONF_SUBSTITUTIONS, "name"])]


def test_iter_ids_paths_are_independent() -> None:
    """Each path is its own list, and the caller's starting path is not changed."""
    start = ["root"]
    config = {"a": {"id": ID("a_id")}, "b": [ID("b_id")]}

    found = list(iter_ids(config, start))
    found[0][1].append("changed")

    assert start == ["root"]
    assert found[0][1] == ["root", "a", "id", "changed"]
    assert found[1][1] == ["root", "b", 0]
