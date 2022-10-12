from esphome import yaml_util
from esphome.components import substitutions


def test_include_with_vars(fixture_path):
    yaml_file = fixture_path / "yaml_util" / "includetest.yaml"

    actual = yaml_util.load_yaml(yaml_file)
    substitutions.do_substitution_pass(actual, None)
    assert actual["esphome"]["name"] == "original"
    assert actual["esphome"]["libraries"][0] == "Wire"
    assert actual["esphome"]["board"] == "nodemcu"
    assert actual["wifi"]["ssid"] == "my_custom_ssid"
