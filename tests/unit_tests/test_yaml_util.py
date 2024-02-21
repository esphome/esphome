from esphome import yaml_util
from esphome.components import substitutions
from esphome.core import EsphomeError


def test_include_with_vars(fixture_path):
    yaml_file = fixture_path / "yaml_util" / "includetest.yaml"

    actual = yaml_util.load_yaml(yaml_file)
    substitutions.do_substitution_pass(actual, None)
    assert actual["esphome"]["name"] == "original"
    assert actual["esphome"]["libraries"][0] == "Wire"
    assert actual["esphome"]["board"] == "nodemcu"
    assert actual["wifi"]["ssid"] == "my_custom_ssid"


def test_loading_a_broken_yaml_file(fixture_path):
    """Ensure we fallback to pure python to give good errors."""
    yaml_file = fixture_path / "yaml_util" / "broken_includetest.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "broken_included.yaml" in str(err)


def test_loading_a_yaml_file_with_a_missing_component(fixture_path):
    """Ensure we show the filename for a yaml file with a missing component."""
    yaml_file = fixture_path / "yaml_util" / "missing_comp.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "missing_comp.yaml" in str(err)


def test_loading_a_missing_file(fixture_path):
    """We throw EsphomeError when loading a missing file."""
    yaml_file = fixture_path / "yaml_util" / "missing.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "missing.yaml" in str(err)
