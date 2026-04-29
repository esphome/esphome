"""Unit tests for script/build_helpers.py manifest override and build helpers."""

import os
from pathlib import Path
import sys
import textwrap
from unittest.mock import MagicMock, patch

import pytest

# Add the script directory to Python path so we can import build_helpers
sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "script"))
)

import build_helpers  # noqa: E402

from esphome.loader import ComponentManifest  # noqa: E402
from tests.testing_helpers import ComponentManifestOverride  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_component_manifest(*, to_code=None, dependencies=None) -> ComponentManifest:
    mod = MagicMock()
    mod.to_code = to_code
    mod.DEPENDENCIES = dependencies or []
    return ComponentManifest(mod)


# ---------------------------------------------------------------------------
# filter_components_with_files
# ---------------------------------------------------------------------------


def test_filter_keeps_components_with_cpp_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "mycomp_test.cpp").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == ["mycomp"]


def test_filter_keeps_components_with_h_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "helpers.h").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == ["mycomp"]


def test_filter_drops_components_without_test_dir(tmp_path: Path) -> None:
    result = build_helpers.filter_components_with_files(["nodir"], tmp_path)

    assert result == []


def test_filter_drops_components_with_no_cpp_or_h(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "README.md").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == []


# ---------------------------------------------------------------------------
# get_platform_components
# ---------------------------------------------------------------------------


def test_get_platform_components_discovers_subdirectory(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "sensor").mkdir(parents=True)

    sensor_mod = MagicMock()
    sensor_mod.IS_PLATFORM_COMPONENT = True

    with patch(
        "build_helpers.get_component", return_value=ComponentManifest(sensor_mod)
    ):
        result = build_helpers.get_platform_components(["bthome"], tmp_path)

    assert result == ["sensor.bthome"]


def test_get_platform_components_skips_pycache(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "__pycache__").mkdir(parents=True)

    result = build_helpers.get_platform_components(["bthome"], tmp_path)

    assert result == []


def test_get_platform_components_raises_for_invalid_domain(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "notadomain").mkdir(parents=True)

    with (
        patch("build_helpers.get_component", return_value=None),
        pytest.raises(ValueError, match="notadomain"),
    ):
        build_helpers.get_platform_components(["bthome"], tmp_path)


# ---------------------------------------------------------------------------
# load_test_manifest_overrides
# ---------------------------------------------------------------------------


def test_load_suppresses_to_code(tmp_path: Path) -> None:
    """to_code is always set to None before the override is called."""

    async def real_to_code(config):
        pass

    inner = _make_component_manifest(to_code=real_to_code)

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.to_code is None


def test_load_calls_override_fn(tmp_path: Path) -> None:
    """override_manifest() in test_init is called with the ComponentManifestOverride."""
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    init_py = comp_dir / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.dependencies = ["injected"]
    """)
    )

    inner = _make_component_manifest()
    override = ComponentManifestOverride(inner)
    override.to_code = None

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.dependencies == ["injected"]


def test_load_enable_codegen_in_override(tmp_path: Path) -> None:
    """An override_manifest that calls enable_codegen() restores to_code."""

    async def real_to_code(config):
        pass

    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    init_py = comp_dir / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.enable_codegen()
    """)
    )

    inner = _make_component_manifest(to_code=real_to_code)

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.to_code is real_to_code


def test_load_no_override_file(tmp_path: Path) -> None:
    """No override file: manifest is wrapped and to_code suppressed, nothing else."""
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "mycomp"
    assert isinstance(installed, ComponentManifestOverride)


def test_load_skips_already_wrapped(tmp_path: Path) -> None:
    """Components already wrapped as ComponentManifestOverride are not double-wrapped."""
    inner = _make_component_manifest()
    already_wrapped = ComponentManifestOverride(inner)

    with (
        patch("build_helpers.get_component", return_value=already_wrapped),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_not_called()


def test_load_skips_platform_component_already_wrapped(tmp_path: Path) -> None:
    inner = _make_component_manifest()
    already_wrapped = ComponentManifestOverride(inner)

    with (
        patch("build_helpers.get_platform", return_value=already_wrapped),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["sensor.bthome"], tmp_path)

    mock_set.assert_not_called()


def test_load_wraps_top_level_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "mycomp"
    assert isinstance(installed, ComponentManifestOverride)
    assert installed.to_code is None


def test_load_wraps_platform_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_platform", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["sensor.bthome"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "bthome.sensor"
    assert isinstance(installed, ComponentManifestOverride)
    assert installed.to_code is None


# ---------------------------------------------------------------------------
# populate_dependency_config
# ---------------------------------------------------------------------------


def _make_component_stub(
    *,
    multi_conf: bool = False,
    is_platform_component: bool = False,
    config_schema=None,
) -> MagicMock:
    stub = MagicMock()
    stub.multi_conf = multi_conf
    stub.is_platform_component = is_platform_component
    stub.config_schema = config_schema
    return stub


def test_populate_platform_component_listed_alone_uses_list() -> None:
    """Regression: a platform component (sensor) with no `sensor.x` siblings
    must land as `[]` in config. Previously it was populated as a dict via
    `schema({})`, which then crashed the sibling `domain.platform` branch
    when later dependencies tried `config.setdefault('sensor', []).append(...)`.
    """
    sensor = _make_component_stub(is_platform_component=True)
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["sensor"],
        get_component_fn=lambda name: sensor if name == "sensor" else None,
        register_platform_fn=lambda _: None,
    )

    assert config["sensor"] == []


def test_populate_platform_component_then_platform_entry() -> None:
    """When `sensor` is processed before `sensor.gpio` (sorted order),
    the bare-component branch must leave `config['sensor']` as a list so
    the platform-entry branch can append into it.
    """
    sensor = _make_component_stub(is_platform_component=True)
    gpio = _make_component_stub()  # the bare `gpio` component
    components: dict[str, object] = {"sensor": sensor, "gpio": gpio}
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["gpio", "sensor", "sensor.gpio"],
        get_component_fn=components.get,
        register_platform_fn=lambda _: None,
    )

    assert config["sensor"] == [{"platform": "gpio"}]


def test_populate_multi_conf_component_uses_list() -> None:
    multi = _make_component_stub(multi_conf=True)
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["multi"],
        get_component_fn=lambda name: multi if name == "multi" else None,
        register_platform_fn=lambda _: None,
    )

    assert config["multi"] == []


def test_populate_plain_component_uses_schema_defaults() -> None:
    schema = MagicMock(return_value={"default_key": 42})
    plain = _make_component_stub(config_schema=schema)
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["plain"],
        get_component_fn=lambda name: plain if name == "plain" else None,
        register_platform_fn=lambda _: None,
    )

    schema.assert_called_once_with({})
    assert config["plain"] == {"default_key": 42}


def test_populate_plain_component_falls_back_when_schema_raises() -> None:
    def picky_schema(_):
        raise ValueError("required field missing")

    plain = _make_component_stub(config_schema=picky_schema)
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["plain"],
        get_component_fn=lambda name: plain if name == "plain" else None,
        register_platform_fn=lambda _: None,
    )

    assert config["plain"] == {}


def test_populate_skips_unresolvable_pseudo_components() -> None:
    """`core` and other names that get_component returns None for are skipped
    silently without inserting anything into the config.
    """
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["core"],
        get_component_fn=lambda _: None,
        register_platform_fn=lambda _: None,
    )

    assert config == {}


def test_populate_preserves_existing_plain_component_config() -> None:
    """If a plain component already has a config entry (e.g. from the user's
    YAML), the schema-defaults branch must not overwrite it.
    """
    schema = MagicMock()
    plain = _make_component_stub(config_schema=schema)
    config: dict = {"plain": {"user_key": "set_by_user"}}

    build_helpers.populate_dependency_config(
        config,
        ["plain"],
        get_component_fn=lambda name: plain if name == "plain" else None,
        register_platform_fn=lambda _: None,
    )

    schema.assert_not_called()
    assert config["plain"] == {"user_key": "set_by_user"}


def test_populate_registers_platform_for_platform_entry() -> None:
    """Each `domain.platform` entry triggers register_platform_fn(domain) so
    USE_<DOMAIN> defines get emitted later in the build pipeline.
    """
    registered: list[str] = []
    config: dict = {}

    build_helpers.populate_dependency_config(
        config,
        ["sensor.gpio", "binary_sensor.gpio"],
        get_component_fn=lambda _: None,
        register_platform_fn=registered.append,
    )

    assert registered == ["sensor", "binary_sensor"]
    assert config["sensor"] == [{"platform": "gpio"}]
    assert config["binary_sensor"] == [{"platform": "gpio"}]
