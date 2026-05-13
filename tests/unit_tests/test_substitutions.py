import glob
import logging
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome import config as config_module, yaml_util
from esphome.components import substitutions
from esphome.components.packages import (
    MAX_INCLUDE_DEPTH,
    _PackageProcessor,
    do_packages_pass,
    merge_packages,
)
from esphome.components.substitutions.jinja import UndefinedError
from esphome.config import resolve_extend_remove
from esphome.config_helpers import Extend, merge_config
import esphome.config_validation as cv
from esphome.const import CONF_SUBSTITUTIONS
from esphome.core import CORE, EsphomeError, Lambda
from esphome.util import OrderedDict

_LOGGER = logging.getLogger(__name__)

# Set to True for dev mode behavior
# This will generate the expected version of the test files.

DEV_MODE = False


def sort_dicts(obj):
    """Recursively sort dictionaries for order-insensitive comparison."""
    if isinstance(obj, dict):
        return {k: sort_dicts(obj[k]) for k in sorted(obj)}
    if isinstance(obj, list):
        # Lists are not sorted; we preserve order
        return [sort_dicts(i) for i in obj]
    return obj


def dict_diff(a, b, path=""):
    """Recursively find differences between two dict/list structures."""
    diffs = []
    if isinstance(a, dict) and isinstance(b, dict):
        a_keys = set(a)
        b_keys = set(b)
        diffs.extend(f"{path}/{key} only in actual" for key in a_keys - b_keys)
        diffs.extend(f"{path}/{key} only in expected" for key in b_keys - a_keys)
        for key in a_keys & b_keys:
            diffs.extend(dict_diff(a[key], b[key], f"{path}/{key}"))
    elif isinstance(a, list) and isinstance(b, list):
        min_len = min(len(a), len(b))
        for i in range(min_len):
            diffs.extend(dict_diff(a[i], b[i], f"{path}[{i}]"))
        if len(a) > len(b):
            diffs.extend(
                f"{path}[{i}] only in actual: {a[i]!r}" for i in range(min_len, len(a))
            )
        elif len(b) > len(a):
            diffs.extend(
                f"{path}[{i}] only in expected: {b[i]!r}"
                for i in range(min_len, len(b))
            )
    elif a != b:
        diffs.append(f"\t{path}: actual={a!r} expected={b!r}")
    return diffs


def write_yaml(path: Path, data: dict) -> None:
    path.write_text(yaml_util.dump(data), encoding="utf-8")


def verify_database(value: Any, path: str = "") -> str | None:
    if isinstance(value, list):
        for i, v in enumerate(value):
            result = verify_database(v, f"{path}[{i}]")
            if result is not None:
                return result
        return None
    if isinstance(value, dict):
        for k, v in value.items():
            if path == "" and k == CONF_SUBSTITUTIONS:
                return None  # ignore substitutions key at top level since it is merged.
            key_result = verify_database(k, f"{path}/{k}")
            if key_result is not None:
                return key_result
            value_result = verify_database(v, f"{path}/{k}")
            if value_result is not None:
                return value_result
        return None
    if isinstance(value, str):
        if not isinstance(value, yaml_util.ESPHomeDataBase):
            return f"{path}: {value!r} is not ESPHomeDataBase"
        return None
    return None


# Mapping of (url, ref) to local test repository path under fixtures/substitutions
REMOTES = {
    ("https://github.com/esphome/repo1", "main"): "remotes/repo1/main",
    ("https://github.com/esphome/repo2", "main"): "remotes/repo2/main",
}

# Collect all input YAML files for test_substitutions_fixtures parametrized tests:
HERE = Path(__file__).parent
BASE_DIR = HERE / "fixtures" / "substitutions"
SOURCES = sorted(glob.glob(str(BASE_DIR / "*.input.yaml")))
assert SOURCES, f"test_substitutions_fixtures: No input YAML files found in {BASE_DIR}"


@pytest.mark.parametrize(
    "source_path",
    [Path(p) for p in SOURCES],
    ids=lambda p: p.name,
)
@patch("esphome.git.clone_or_update")
def test_substitutions_fixtures(
    mock_clone_or_update: MagicMock, source_path: Path
) -> None:
    def fake_clone_or_update(
        *,
        url: str,
        ref: str | None = None,
        refresh=None,
        domain: str,
        username: str | None = None,
        password: str | None = None,
        submodules: list[str] | None = None,
        _recover_broken: bool = True,
    ) -> tuple[Path, None]:
        path = REMOTES.get((url, ref))
        if path is None:
            path = REMOTES.get((url.rstrip(".git"), ref))
            if path is None:
                raise RuntimeError(
                    f"Cannot find test repository for {url} @ {ref}. Check the REMOTES mapping in test_substitutions.py"
                )
        return BASE_DIR / path, None

    mock_clone_or_update.side_effect = fake_clone_or_update

    expected_path = source_path.with_suffix("").with_suffix(".approved.yaml")
    test_case = source_path.with_suffix("").stem

    # Load using ESPHome's YAML loader
    config = yaml_util.load_yaml(source_path)

    command_line_substitutions = config.pop("command_line_substitutions", None)

    config = do_packages_pass(
        config, command_line_substitutions=command_line_substitutions
    )

    config = substitutions.do_substitution_pass(config, command_line_substitutions)

    config = merge_packages(config)

    resolve_extend_remove(config)
    verify_database_result = verify_database(config)
    if verify_database_result is not None:
        raise AssertionError(verify_database_result)

    # Also load expected using ESPHome's loader, or use {} if missing and DEV_MODE
    if expected_path.is_file():
        expected = yaml_util.load_yaml(expected_path)
    elif DEV_MODE:
        expected = {}
    else:
        assert expected_path.is_file(), f"Expected file missing: {expected_path}"

    # Sort dicts only (not lists) for comparison
    got_sorted = sort_dicts(config)
    expected_sorted = sort_dicts(expected)

    if got_sorted != expected_sorted:
        diff = "\n".join(dict_diff(got_sorted, expected_sorted))
        msg = (
            f"Substitution result mismatch for {source_path.name}\n"
            f"Diff:\n{diff}\n\n"
            f"Got:      {got_sorted}\n"
            f"Expected: {expected_sorted}"
        )
        # Write out the received file when test fails
        if DEV_MODE:
            received_path = source_path.with_name(f"{test_case}.received.yaml")
            write_yaml(received_path, config)
            msg += f"\nWrote received file to {received_path}."
        raise AssertionError(msg)

    if DEV_MODE:
        _LOGGER.error("Tests passed, but Dev mode is enabled.")
    assert (
        not DEV_MODE  # make sure DEV_MODE is disabled after you are finished.
    ), (
        "Test passed but DEV_MODE must be disabled when running tests. Please set DEV_MODE=False."
    )


def test_substitutions_with_command_line_maintains_ordered_dict() -> None:
    """Test that substitutions remain an OrderedDict when command line substitutions are provided,
    and that move_to_end() can be called successfully.

    This is a regression test for https://github.com/esphome/esphome/issues/11182
    where the config would become a regular dict and fail when move_to_end() was called.
    """
    # Create an OrderedDict config with substitutions
    config = OrderedDict()
    config["esphome"] = {"name": "test"}
    config[CONF_SUBSTITUTIONS] = {"var1": "value1", "var2": "value2"}
    config["other_key"] = "other_value"

    # Command line substitutions that should override
    command_line_subs = {"var2": "override", "var3": "new_value"}

    # Call do_substitution_pass with command line substitutions
    config = substitutions.do_substitution_pass(config, command_line_subs)

    # Verify that config is still an OrderedDict
    assert isinstance(config, OrderedDict), "Config should remain an OrderedDict"

    # Verify substitutions are at the beginning (move_to_end with last=False)
    keys = list(config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"

    # Verify substitutions were properly merged
    assert config[CONF_SUBSTITUTIONS]["var1"] == "value1"
    assert config[CONF_SUBSTITUTIONS]["var2"] == "override"
    assert config[CONF_SUBSTITUTIONS]["var3"] == "new_value"

    # Verify config[CONF_SUBSTITUTIONS] is also an OrderedDict
    assert isinstance(config[CONF_SUBSTITUTIONS], OrderedDict), (
        "Substitutions should be an OrderedDict"
    )


def test_substitutions_without_command_line_maintains_ordered_dict() -> None:
    """Test that substitutions work correctly without command line substitutions."""
    config = OrderedDict()
    config["esphome"] = {"name": "test"}
    config[CONF_SUBSTITUTIONS] = {"var1": "value1"}
    config["other_key"] = "other_value"

    # Call without command line substitutions
    config = substitutions.do_substitution_pass(config, None)

    # Verify that config is still an OrderedDict
    assert isinstance(config, OrderedDict), "Config should remain an OrderedDict"

    # Verify substitutions are at the beginning
    keys = list(config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"


def test_substitutions_after_merge_config_maintains_ordered_dict() -> None:
    """Test that substitutions work after merge_config (packages scenario).

    This is a regression test for https://github.com/esphome/esphome/issues/11182
    where using packages would cause config to become a regular dict, breaking move_to_end().
    """
    # Simulate what happens with packages - merge two OrderedDict configs
    base_config = OrderedDict()
    base_config["esphome"] = {"name": "base"}
    base_config[CONF_SUBSTITUTIONS] = {"var1": "value1"}

    package_config = OrderedDict()
    package_config["sensor"] = [{"platform": "template"}]
    package_config[CONF_SUBSTITUTIONS] = {"var2": "value2"}

    # Merge configs (simulating package merge)
    merged_config = merge_config(base_config, package_config)

    # Verify merged config is still an OrderedDict
    assert isinstance(merged_config, OrderedDict), (
        "Merged config should be an OrderedDict"
    )

    # Now try to run substitution pass on the merged config
    merged_config = substitutions.do_substitution_pass(merged_config, None)

    # Should not raise AttributeError
    assert isinstance(merged_config, OrderedDict), (
        "Config should still be OrderedDict after substitution pass"
    )
    keys = list(merged_config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"


def test_validate_config_with_command_line_substitutions_maintains_ordered_dict(
    tmp_path: Path,
) -> None:
    """Test that validate_config preserves OrderedDict when merging command-line substitutions.

    This tests the code path in config.py where result[CONF_SUBSTITUTIONS] is set
    using merge_dicts_ordered() with command-line substitutions provided.
    """
    # Create a minimal valid config
    test_config = OrderedDict()
    test_config["esphome"] = {"name": "test_device"}
    test_config[CONF_SUBSTITUTIONS] = OrderedDict({"var1": "value1", "var2": "value2"})
    test_config["esp32"] = {"board": "esp32dev"}

    # Command line substitutions that should override
    command_line_subs = {"var2": "override", "var3": "new_value"}

    # Set up CORE for the test with a proper Path object
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("# test config")
    CORE.config_path = test_yaml

    # Call validate_config with command line substitutions
    result = config_module.validate_config(test_config, command_line_subs)

    # Verify that result[CONF_SUBSTITUTIONS] is an OrderedDict
    assert isinstance(result.get(CONF_SUBSTITUTIONS), OrderedDict), (
        "Result substitutions should be an OrderedDict"
    )

    # Verify substitutions were properly merged
    assert result[CONF_SUBSTITUTIONS]["var1"] == "value1"
    assert result[CONF_SUBSTITUTIONS]["var2"] == "override"
    assert result[CONF_SUBSTITUTIONS]["var3"] == "new_value"


def _get_test_minimal_valid_config(tmp_path: Path) -> OrderedDict:
    """Helper to create a minimal valid config for testing."""
    # Create a minimal valid config
    test_config = OrderedDict()
    test_config["esphome"] = {"name": "test_device"}
    test_config[CONF_SUBSTITUTIONS] = OrderedDict({"var1": "value1", "var2": "value2"})
    test_config["esp32"] = {"board": "esp32dev"}

    # Set up CORE for the test with a proper Path object
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("# test config")
    CORE.config_path = test_yaml
    return test_config


def test_validate_config_without_command_line_substitutions_maintains_ordered_dict(
    tmp_path: Path,
) -> None:
    """Test that validate_config preserves OrderedDict without command-line substitutions.

    This tests the code path in config.py where result[CONF_SUBSTITUTIONS] is set
    using merge_dicts_ordered() when command_line_substitutions is None.
    """

    test_config = _get_test_minimal_valid_config(tmp_path)

    # Call validate_config without command line substitutions
    result = config_module.validate_config(test_config, None)

    # Verify that result[CONF_SUBSTITUTIONS] is an OrderedDict
    assert isinstance(result.get(CONF_SUBSTITUTIONS), OrderedDict), (
        "Result substitutions should be an OrderedDict"
    )

    # Verify substitutions are unchanged
    assert result[CONF_SUBSTITUTIONS]["var1"] == "value1"
    assert result[CONF_SUBSTITUTIONS]["var2"] == "value2"


def test_merge_config_preserves_ordered_dict() -> None:
    """Test that merge_config preserves OrderedDict type.

    This is a regression test to ensure merge_config doesn't lose OrderedDict type
    when merging configs, which causes AttributeError on move_to_end().
    """
    # Test OrderedDict + dict = OrderedDict
    od = OrderedDict([("a", 1), ("b", 2)])
    d = {"b": 20, "c": 3}
    result = merge_config(od, d)
    assert isinstance(result, OrderedDict), (
        "OrderedDict + dict should return OrderedDict"
    )

    # Test dict + OrderedDict = OrderedDict
    d = {"a": 1, "b": 2}
    od = OrderedDict([("b", 20), ("c", 3)])
    result = merge_config(d, od)
    assert isinstance(result, OrderedDict), (
        "dict + OrderedDict should return OrderedDict"
    )

    # Test OrderedDict + OrderedDict = OrderedDict
    od1 = OrderedDict([("a", 1), ("b", 2)])
    od2 = OrderedDict([("b", 20), ("c", 3)])
    result = merge_config(od1, od2)
    assert isinstance(result, OrderedDict), (
        "OrderedDict + OrderedDict should return OrderedDict"
    )

    # Test that dict + dict still returns regular dict (no unnecessary conversion)
    d1 = {"a": 1, "b": 2}
    d2 = {"b": 20, "c": 3}
    result = merge_config(d1, d2)
    assert isinstance(result, dict), "dict + dict should return dict"
    assert not isinstance(result, OrderedDict), (
        "dict + dict should not return OrderedDict"
    )


def test_substitution_pass_error_gets_captured(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """vol.Invalid from do_substitution_pass is captured by validate_config."""

    # Patch the target: in config_module.do_substitution_pass (NOT where it's defined)
    def fake_do_substitution_pass(*args, **kwargs):
        raise cv.Invalid("Error in do_substitutions_pass!!")

    monkeypatch.setattr(
        config_module, "do_substitution_pass", fake_do_substitution_pass
    )

    # Prepare minimal config + no CLI substitutions
    config = _get_test_minimal_valid_config(tmp_path)

    # Call the function under test
    result = config_module.validate_config(config, None)

    # Now assert that add_error was called with the vol.Invalid

    assert "Error in do_substitutions_pass!!" in str(result.get_error_for_path([]))


@pytest.mark.parametrize(
    "value", ["", "   ", "1foo", "9VAR", "0abc", "$1foo", "$9VAR", "$0abc"]
)
def test_validate_substitution_key_empty_raises(value: str) -> None:
    """Empty (or all-whitespace) substitution keys are rejected."""
    with pytest.raises(cv.Invalid):
        substitutions.validate_substitution_key(value)


@pytest.mark.parametrize(
    "input_value, expected_output",
    [
        ("$FOO_bar9", "FOO_bar9"),  # Valid key with leading '$'
        ("Foo_bar9", "Foo_bar9"),  # Normal valid key
    ],
)
def test_validate_substitution_key_valid(
    input_value: str, expected_output: str
) -> None:
    """Valid substitution keys are accepted with optional leading '$'."""
    result = substitutions.validate_substitution_key(input_value)
    assert result == expected_output


def test_circular_dependency_warnings(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Circular substitution references produce warnings naming the cause."""
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: OrderedDict({"x": "${y}", "y": "${x}"}),
            "key": "value",
        }
    )
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert "Could not resolve substitution variable 'x'" in caplog.text
    assert "'y' is undefined" in caplog.text
    assert "Could not resolve substitution variable 'y'" in caplog.text
    assert "'x' is undefined" in caplog.text
    # Verify path includes location
    assert "substitutions->x" in caplog.text
    assert "substitutions->y" in caplog.text


def test_missing_dependency_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A substitution referencing an undefined variable warns with the cause."""
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: OrderedDict({"a": "${missing}"}),
            "key": "value",
        }
    )
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert "Could not resolve substitution variable 'a'" in caplog.text
    assert "'missing' is undefined" in caplog.text
    assert "substitutions->a" in caplog.text


def test_undefined_variable_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A reference to an undefined variable in config values produces a warning."""
    config = OrderedDict(
        {
            "key": "${undefined_var}",
        }
    )
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert "'undefined_var' is undefined" in caplog.text


def test_password_field_warnings_suppressed(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Undefined variables in password fields should not produce warnings."""
    config = OrderedDict(
        {
            "password": "${undefined_var}",
        }
    )
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert caplog.text == ""


def test_config_context_unresolvable_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Unresolvable vars in a ConfigContext produce warnings via push_context."""
    inner = OrderedDict({"key": "${a}"})
    yaml_util.add_context(inner, {"a": "${undefined}"})
    config = OrderedDict({"items": [inner]})
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert "Could not resolve substitution variable 'a'" in caplog.text
    assert "'undefined' is undefined" in caplog.text


def test_non_string_substitution_value_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Undefined vars in non-string contexts (e.g. dict keys) produce warnings."""
    config = OrderedDict(
        {
            "items": {"${undefined_key}": "value"},
        }
    )
    with caplog.at_level(logging.WARNING):
        substitutions.do_substitution_pass(config)

    assert "'undefined_key' is undefined" in caplog.text


def test_lambda_substitution() -> None:
    """Substitution inside a Lambda value should be expanded."""
    lam = Lambda("return ${var};")
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: OrderedDict({"var": "42"}),
            "lambda": lam,
        }
    )
    config = substitutions.do_substitution_pass(config)
    assert config["lambda"].value == "return 42;"


def test_lambda_no_substitution_unchanged() -> None:
    """A Lambda with no variable references should not be mutated."""
    lam = Lambda("return 1;")
    original_value = lam.value
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: OrderedDict({"var": "42"}),
            "lambda": lam,
        }
    )
    config = substitutions.do_substitution_pass(config)
    assert config["lambda"].value is original_value


def test_extend_substitution() -> None:
    """Substitution inside an Extend value should be expanded."""
    ext = Extend("${component_id}")
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: OrderedDict({"component_id": "my_sensor"}),
            "sensor": ext,
        }
    )
    config = substitutions.do_substitution_pass(config)
    assert config["sensor"].value == "my_sensor"


def test_substitute_does_not_mutate_input() -> None:
    """substitute() must return a new tree without modifying the original."""
    inner_list = ["${var}", "static"]
    inner_dict = OrderedDict({"key": "${var}"})
    lam = Lambda("return ${var};")
    config = OrderedDict(
        {
            "a_list": inner_list,
            "a_dict": inner_dict,
            "a_lambda": lam,
            "plain": "${var}",
        }
    )
    context = substitutions.ContextVars({"var": "replaced"})
    result = substitutions.substitute(config, [], context, strict_undefined=True)

    # Result has substitutions applied
    assert result["plain"] == "replaced"
    assert result["a_list"] == ["replaced", "static"]
    assert result["a_dict"]["key"] == "replaced"
    assert result["a_lambda"].value == "return replaced;"

    # Original input is untouched
    assert config["plain"] == "${var}"
    assert inner_list == ["${var}", "static"]
    assert inner_dict["key"] == "${var}"
    assert lam.value == "return ${var};"

    # Containers are new objects, not the originals
    assert result["a_list"] is not inner_list
    assert result["a_dict"] is not inner_dict
    assert result["a_lambda"] is not lam


def test_do_substitution_pass_substitutions_must_be_mapping_from_config() -> None:
    """Non-mapping substitutions raises cv.Invalid."""
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: ["not", "a", "mapping"],
            "other": "value",
        }
    )

    with pytest.raises(
        cv.Invalid, match="Substitutions must be a key to value mapping"
    ):
        substitutions.do_substitution_pass(config)


# ── IncludeFile / package loading tests ────────────────────────────────────


def test_resolve_package_max_depth_exceeded(tmp_path: Path) -> None:
    """A yaml_loader that always returns another IncludeFile triggers the depth guard."""
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    # Each call to the loader returns a fresh IncludeFile pointing at itself,
    # so PACKAGE_SCHEMA always sees an IncludeFile and never a dict.
    def always_returns_include(path: Path) -> yaml_util.IncludeFile:
        return yaml_util.IncludeFile(parent, path.name, None, always_returns_include)

    package_config = yaml_util.IncludeFile(
        parent, "test.yaml", None, always_returns_include
    )
    processor = _PackageProcessor({}, None)
    with pytest.raises(
        cv.Invalid,
        match=f"Maximum include nesting depth \\({MAX_INCLUDE_DEPTH}\\) exceeded",
    ):
        processor.resolve_package(package_config, substitutions.ContextVars(), [])


def test_include_filename_substitution_undefined_var(tmp_path: Path) -> None:
    """!include with an undefined substitution variable raises cv.Invalid.

    The error message must reference the unresolved filename template so the
    user knows which include failed, rather than seeing a bare file-not-found.
    """
    main_file = tmp_path / "main.yaml"
    main_file.write_text("result: !include ${undefined_var}.yaml\n")

    config = yaml_util.load_yaml(main_file)
    with pytest.raises(cv.Invalid, match=r"\$\{undefined_var\}"):
        substitutions.do_substitution_pass(config)


def test_raise_first_undefined_logs_extras_at_debug(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Only the first undefined error is raised; extras are logged at debug."""
    errors: substitutions.ErrList = [
        (UndefinedError("'a' is undefined"), ["url"], None),
        (UndefinedError("'b' is undefined"), ["ref"], None),
        (UndefinedError("'c' is undefined"), ["path"], None),
    ]

    with (
        caplog.at_level(logging.DEBUG, logger="esphome.components.substitutions"),
        pytest.raises(cv.Invalid) as exc_info,
    ):
        substitutions.raise_first_undefined(errors, "package definition")

    # First error is surfaced as the cv.Invalid message.
    raised = str(exc_info.value)
    assert "'a' is undefined" in raised
    assert "'b' is undefined" not in raised
    assert "'c' is undefined" not in raised

    # Remaining errors are captured via debug logging for troubleshooting.
    assert "Additional undefined variables in package definition" in caplog.text
    assert "'b' is undefined at 'ref'" in caplog.text
    assert "'c' is undefined at 'path'" in caplog.text


def test_raise_first_undefined_noop_on_empty() -> None:
    """An empty errors list is a no-op — no exception, no log."""
    substitutions.raise_first_undefined([], "package definition")


def test_do_substitution_pass_included_substitutions_must_be_mapping(
    tmp_path: Path,
) -> None:
    """`substitutions: !include list.yaml` where the file holds a list raises cv.Invalid.

    Locks in the shape check that runs after the deferred IncludeFile has been
    resolved.
    """
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    def loader(path: Path):
        return ["not", "a", "mapping"]

    include = yaml_util.IncludeFile(parent, "subs.yaml", None, loader)
    config = OrderedDict({CONF_SUBSTITUTIONS: include})

    with pytest.raises(
        cv.Invalid, match="Substitutions must be a key to value mapping"
    ):
        substitutions.do_substitution_pass(config)


def test_do_packages_pass_included_substitutions_must_be_mapping(
    tmp_path: Path,
) -> None:
    """`substitutions: !include list.yaml` alongside `packages:` raises cv.Invalid.

    Without the shape check, ``UserDict(...)`` would surface a low-level
    ``TypeError``; the explicit ``cv.Invalid`` points at the substitutions path.
    """
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    def loader(path: Path):
        return ["not", "a", "mapping"]

    include = yaml_util.IncludeFile(parent, "subs.yaml", None, loader)
    config = OrderedDict(
        {
            CONF_SUBSTITUTIONS: include,
            "packages": {"noop": {"wifi": {"ssid": "main"}}},
        }
    )

    with pytest.raises(
        cv.Invalid, match="Substitutions must be a key to value mapping"
    ):
        do_packages_pass(config)


def test_resolve_package_undefined_var_in_include_filename(tmp_path: Path) -> None:
    """An undefined substitution in a package include filename raises cv.Invalid.

    Previously this would raise an unhandled UndefinedError. With
    strict_undefined=False, the unresolved filename passes through to
    file loading which produces a clean cv.Invalid error.
    """
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    def loader(path: Path):
        raise EsphomeError(f"Error reading file {path}: No such file")

    package_config = yaml_util.IncludeFile(
        parent, "${undefined_var}.yaml", None, loader
    )
    processor = _PackageProcessor({}, None)
    with pytest.raises(cv.Invalid, match="unresolved substitutions"):
        processor.resolve_package(package_config, substitutions.ContextVars(), [])


def test_resolve_include_error_shows_expanded_from_when_substituted(
    tmp_path: Path,
) -> None:
    """When a substituted filename fails to load, the error includes '(expanded from ...)'."""
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    def failing_loader(_path: Path) -> None:
        raise EsphomeError("File not found")

    include = yaml_util.IncludeFile(parent, "${device}.yaml", None, failing_loader)
    context = substitutions.ContextVars({"device": "my_device"})

    with pytest.raises(cv.Invalid) as exc_info:
        substitutions.resolve_include(include, [], context)

    msg = str(exc_info.value)
    assert "my_device.yaml" in msg
    assert "expanded from '${device}.yaml'" in msg


def test_resolve_include_error_no_expanded_from_for_literal_filename(
    tmp_path: Path,
) -> None:
    """When a literal filename fails to load, the error has no 'expanded from' clause."""
    parent = tmp_path / "main.yaml"
    parent.write_text("")

    def failing_loader(_path: Path) -> None:
        raise EsphomeError("File not found")

    include = yaml_util.IncludeFile(parent, "literal.yaml", None, failing_loader)

    with pytest.raises(cv.Invalid) as exc_info:
        substitutions.resolve_include(include, [], substitutions.ContextVars())

    assert "expanded from" not in str(exc_info.value)


def test_include_vars_applied_to_lambda_value(tmp_path: Path) -> None:
    """!include vars: must substitute into a top-level !lambda value in the included file.

    Regression test for the case where the included file's root is a Lambda;
    add_context() previously only tagged dict/list/str, so the include's vars
    never reached the substitution pass for Lambda content.
    """
    included = tmp_path / "lambda.yaml"
    included.write_text('!lambda |-\n  return "${foo}";\n')

    include = yaml_util.IncludeFile(
        tmp_path / "main.yaml", "lambda.yaml", {"foo": "bar"}, yaml_util.load_yaml
    )
    config = OrderedDict({"value": include.load()})
    result = substitutions.do_substitution_pass(config)

    assert isinstance(result["value"], Lambda)
    assert result["value"].value == 'return "bar";'
