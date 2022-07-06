from collections import OrderedDict
import yaml
from yaml import SafeLoader
from esphome import yaml_util
from esphome.helpers import read_file, write_file


def clean(x):
    if isinstance(x, dict):
        return {clean(key): clean(value) for key, value in x.items()}
    if isinstance(x, list):
        return [clean(i) for i in x]
    if isinstance(x, str):
        return str(x)
    if isinstance(x, int):
        return int(x)
    if isinstance(x, float):
        return float(x)
    return x


def compare_to_expected(fixture_path, actual, fname, generate=False):
    fname = fixture_path / "yaml_util" / "expected" / fname
    actual = clean(actual)
    if generate:
        write_file(fname, yaml.dump(actual))
        assert "generate" == "Generate is set to True"

    content = read_file(fname)
    loader = SafeLoader(content)
    expected = loader.get_single_data() or OrderedDict()
    loader.dispose()
    assert expected == actual, f"Expected result to be equal to contents of {fname}"


def _test_yaml_file(fixture_path, fname, generate=False):
    yaml_file = fixture_path / "yaml_util" / fname
    vars = yaml_util.load_vars(yaml_file)
    actual = yaml_util.load_yaml(yaml_file, vars=vars)
    compare_to_expected(fixture_path, actual, fname, generate)


def test_include_with_vars(fixture_path):
    _test_yaml_file(fixture_path, "includetest.yaml")


def test_for(fixture_path):
    _test_yaml_file(fixture_path, "fortest.yaml", False)


def test_if(fixture_path):
    _test_yaml_file(fixture_path, "iftest.yaml", False)
