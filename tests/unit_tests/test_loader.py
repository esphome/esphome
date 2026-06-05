"""Unit tests for esphome.loader module."""

import ast
import logging
from pathlib import Path
import sys
import textwrap
from types import ModuleType
from unittest.mock import MagicMock, Mock, patch

import pytest
import voluptuous as vol

from esphome import config as esphome_config, config_validation as cv
from esphome.core import CORE
import esphome.loader as loader_mod
from esphome.loader import (
    AliasMeta,
    ComponentManifest,
    _AliasFinder,
    _build_alias_map,
    _read_aliases,
    _replace_component_manifest,
    get_component,
)
from tests.testing_helpers import ComponentManifestOverride

# ---------------------------------------------------------------------------
# ComponentManifestOverride
# ---------------------------------------------------------------------------


def _make_manifest(*, to_code=None, dependencies=None) -> ComponentManifest:
    """Return a ComponentManifest backed by a minimal mock module."""
    mod = MagicMock()
    mod.to_code = to_code
    mod.DEPENDENCIES = dependencies or []
    return ComponentManifest(mod)


def test_testing_manifest_delegates_to_wrapped() -> None:
    """Unoverridden attributes fall through to the wrapped manifest."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    assert tm.dependencies == ["wifi"]


def test_testing_manifest_override_shadows_wrapped() -> None:
    """An assigned attribute shadows the wrapped value."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.dependencies = ["ble"]
    assert tm.dependencies == ["ble"]
    # Wrapped value unchanged
    assert inner.dependencies == ["wifi"]


def test_testing_manifest_to_code_suppression() -> None:
    """Setting to_code=None suppresses code generation."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code)
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    assert tm.to_code is None


def test_testing_manifest_enable_codegen_removes_suppression() -> None:
    """enable_codegen() removes the to_code override, restoring the original."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code)
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    assert tm.to_code is None

    tm.enable_codegen()
    assert tm.to_code is real_to_code


def test_testing_manifest_enable_codegen_preserves_other_overrides() -> None:
    """enable_codegen() only removes to_code; other overrides survive."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    tm.dependencies = ["ble"]

    tm.enable_codegen()

    assert tm.to_code is inner.to_code
    assert tm.dependencies == ["ble"]


def test_testing_manifest_restore_clears_all_overrides() -> None:
    """restore() removes every override, reverting all attributes to wrapped values."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code, dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    tm.dependencies = ["ble"]

    tm.restore()

    assert tm.to_code is real_to_code
    assert tm.dependencies == ["wifi"]


def test_replace_component_manifest_installs_override() -> None:
    """_replace_component_manifest replaces the cached manifest for a domain."""
    inner = _make_manifest()
    override = ComponentManifestOverride(inner)

    _replace_component_manifest("_test_dummy_domain", override)

    assert get_component("_test_dummy_domain") is override


def test_component_manifest_resources_with_filter_source_files() -> None:
    """Test that ComponentManifest.resources correctly filters out excluded files."""
    # Create a mock module with FILTER_SOURCE_FILES function
    mock_module = MagicMock()
    mock_module.FILTER_SOURCE_FILES = lambda: [
        "platform_esp32.cpp",
        "platform_esp8266.cpp",
    ]
    mock_module.__package__ = "esphome.components.test_component"

    # Create ComponentManifest instance
    manifest = ComponentManifest(mock_module)

    # Mock the files in the package
    def create_mock_file(filename: str) -> MagicMock:
        mock_file = MagicMock()
        mock_file.name = filename
        mock_file.is_file.return_value = True
        return mock_file

    mock_files = [
        create_mock_file("test.cpp"),
        create_mock_file("test.h"),
        create_mock_file("platform_esp32.cpp"),
        create_mock_file("platform_esp8266.cpp"),
        create_mock_file("common.cpp"),
        create_mock_file("README.md"),  # Should be excluded by extension
    ]

    # Mock importlib.resources
    with patch("importlib.resources.files") as mock_files_func:
        mock_package_files = MagicMock()
        mock_package_files.iterdir.return_value = mock_files
        mock_package_files.joinpath = lambda name: MagicMock(is_file=lambda: True)
        mock_files_func.return_value = mock_package_files

        # Get resources
        resources = manifest.resources

        # Convert to list of filenames for easier testing
        resource_names = [r.resource for r in resources]

        # Check that platform files are excluded
        assert "platform_esp32.cpp" not in resource_names
        assert "platform_esp8266.cpp" not in resource_names

        # Check that other source files are included
        assert "test.cpp" in resource_names
        assert "test.h" in resource_names
        assert "common.cpp" in resource_names

        # Check that non-source files are excluded
        assert "README.md" not in resource_names

        # Verify the correct number of resources
        assert len(resources) == 3  # test.cpp, test.h, common.cpp


# ---------------------------------------------------------------------------
# recursive_sources — used only by the core "esphome" manifest so that files
# in esphome/core/<subdir>/*.cpp (e.g. esphome/core/wake/wake_host.cpp) are
# discovered without promoting <subdir>/ to a Python subpackage.
# ---------------------------------------------------------------------------


def _mock_file(filename: str) -> MagicMock:
    m = MagicMock()
    m.name = filename
    m.is_file.return_value = True
    m.is_dir.return_value = False
    return m


def _mock_dir(dirname: str, children: list, has_init: bool = False) -> MagicMock:
    """Mock a directory entry with an iterdir() and joinpath('__init__.py')."""
    d = MagicMock()
    d.name = dirname
    d.is_file.return_value = False
    d.is_dir.return_value = True
    d.iterdir.return_value = children
    init_marker = MagicMock()
    init_marker.is_file.return_value = has_init
    d.joinpath.return_value = init_marker
    return d


def test_component_manifest_resources_non_recursive_skips_subdirs() -> None:
    """Default (recursive_sources=False) does not descend into subdirectories."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.components.test_component"
    # No FILTER_SOURCE_FILES.
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module)  # recursive_sources defaults to False

    top_level = [
        _mock_file("top.cpp"),
        _mock_dir("subdir", [_mock_file("nested.cpp")]),
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["top.cpp"]


def test_component_manifest_resources_recursive_walks_non_subpackage_subdirs() -> None:
    """With recursive_sources=True, a subdir without __init__.py is walked."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    wake_dir = _mock_dir(
        "wake",
        [
            _mock_file("wake_host.cpp"),
            _mock_file("wake_host.h"),
            _mock_file("README.md"),  # wrong suffix, excluded
        ],
        has_init=False,
    )
    top_level = [
        _mock_file("wake.h"),
        wake_dir,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = sorted(r.resource for r in manifest.resources)

    assert names == ["wake.h", "wake/wake_host.cpp", "wake/wake_host.h"]


def test_component_manifest_resources_recursive_skips_subpackages() -> None:
    """Subdirectories that ARE Python subpackages (contain __init__.py) are
    skipped even with recursive_sources=True — those load as their own
    ComponentManifest and would otherwise be double-counted."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.components.haier"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    button_pkg = _mock_dir(
        "button",
        [_mock_file("self_cleaning.cpp")],
        has_init=True,  # Python subpackage — must be skipped.
    )
    top_level = [
        _mock_file("haier.cpp"),
        button_pkg,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["haier.cpp"]


def test_component_manifest_resources_recursive_skips_pycache() -> None:
    """__pycache__ inside a recursive walk must never be descended into."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    # __pycache__ is_dir=True but must be skipped without checking __init__.py
    # or calling iterdir (would yield compiled artifacts).
    pycache = _mock_dir("__pycache__", [_mock_file("wake.cpython-314.pyc")])
    top_level = [
        _mock_file("wake.h"),
        pycache,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["wake.h"]


def test_component_manifest_resources_recursive_filter_source_files_supports_subpaths() -> (
    None
):
    """FILTER_SOURCE_FILES entries using '/'-joined subpaths exclude files
    inside a recursively-walked subdir."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    mock_module.FILTER_SOURCE_FILES = lambda: ["wake/wake_host.cpp"]

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    wake_dir = _mock_dir(
        "wake",
        [
            _mock_file("wake_host.cpp"),  # excluded
            _mock_file("wake_freertos.cpp"),  # kept
        ],
    )
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = [wake_dir]
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["wake/wake_freertos.cpp"]


# ---------------------------------------------------------------------------
# Component aliases (renamed-platform back-compat)
# ---------------------------------------------------------------------------
#
# These tests pin down the substrate behind `ALIASES = [...]` on component
# `__init__.py` files: the AST scanner, the resulting global alias map, the
# Python-import `sys.meta_path` finder, the `get_component` integration, and
# the YAML pre-pass that rewrites legacy top-level keys.
#
# The framework is component-agnostic, so the integration tests inject a
# synthetic alias map (pointing a fake legacy name at the real `esp32`
# component) rather than depending on any specific renamed component.

# A legacy name that is NOT a real component, used as a synthetic alias.
_FAKE_ALIAS = "esp32_legacy_alias"


def _write_component(root: Path, name: str, body: str) -> None:
    """Write a fake component package at ``root/<name>/__init__.py``."""
    pkg = root / name
    pkg.mkdir()
    (pkg / "__init__.py").write_text(body)


def test_read_aliases_extracts_list_literal(tmp_path: Path) -> None:
    """AST scan should pick up ``ALIASES = ["legacy"]`` without executing."""
    init = tmp_path / "__init__.py"
    init.write_text("ALIASES = ['legacy_name']\n")
    aliases, removal = _read_aliases(init, ast)
    assert aliases == ["legacy_name"]
    assert removal is None


def test_read_aliases_extracts_removal_version(tmp_path: Path) -> None:
    """``ALIAS_REMOVAL_VERSION`` should be paired with the alias list."""
    init = tmp_path / "__init__.py"
    init.write_text(
        textwrap.dedent("""\
            ALIASES = ['old']
            ALIAS_REMOVAL_VERSION = "2027.6.0"
            """)
    )
    aliases, removal = _read_aliases(init, ast)
    assert aliases == ["old"]
    assert removal == "2027.6.0"


def test_read_aliases_skips_dynamic_forms(tmp_path: Path) -> None:
    """A call-expression / non-literal ALIASES shouldn't surface — the
    scanner deliberately ignores anything non-static to keep behavior
    predictable (and avoid executing component code)."""
    init = tmp_path / "__init__.py"
    init.write_text("ALIASES = list_helper()\nALIASES = ['caught'] if False else []\n")
    aliases, _ = _read_aliases(init, ast)
    assert aliases == []


def test_read_aliases_returns_empty_for_missing_declaration(tmp_path: Path) -> None:
    init = tmp_path / "__init__.py"
    init.write_text("CODEOWNERS = ['@me']\n")
    aliases, removal = _read_aliases(init, ast)
    assert aliases == []
    assert removal is None


def test_read_aliases_handles_syntax_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A broken __init__.py shouldn't crash the alias scanner — it'll
    surface as an ImportError elsewhere, but the scanner logs a warning and
    yields nothing so other components keep working. The substring pre-filter
    only skips files with no ``ALIASES`` token, so this file (which has one)
    still reaches the parse."""
    init = tmp_path / "__init__.py"
    init.write_text("ALIASES = ['x']\ndef broken( :\n")
    assert _read_aliases(init, ast) == ([], None)
    assert "Could not parse" in caplog.text


def test_read_aliases_handles_read_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unreadable __init__.py logs a warning and yields nothing rather
    than aborting the whole component scan."""
    missing = tmp_path / "nope" / "__init__.py"
    assert _read_aliases(missing, ast) == ([], None)
    assert "Could not read" in caplog.text


def test_build_alias_map_aggregates_components(tmp_path: Path) -> None:
    """End-to-end map build over a fake components dir."""
    _write_component(tmp_path, "newcomp", "ALIASES = ['oldcomp']\n")
    _write_component(tmp_path, "other", "")

    with patch("esphome.loader.CORE_COMPONENTS_PATH", tmp_path):
        alias_map, meta_map = _build_alias_map()

    assert alias_map == {"oldcomp": "newcomp"}
    assert meta_map == {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)}


def test_build_alias_map_carries_removal_version(tmp_path: Path) -> None:
    _write_component(
        tmp_path,
        "newcomp",
        "ALIASES = ['oldcomp']\nALIAS_REMOVAL_VERSION = '2028.1.0'\n",
    )

    with patch("esphome.loader.CORE_COMPONENTS_PATH", tmp_path):
        _, meta_map = _build_alias_map()

    assert meta_map["oldcomp"].removal_version == "2028.1.0"


def test_build_alias_map_rejects_duplicate_alias(tmp_path: Path) -> None:
    """If two canonical components both claim the same legacy alias,
    routing becomes ambiguous — the build must refuse to start so the
    conflict surfaces immediately at import time, not later as a
    'mysterious wrong component' bug."""
    _write_component(tmp_path, "comp_a", "ALIASES = ['shared']\n")
    _write_component(tmp_path, "comp_b", "ALIASES = ['shared']\n")

    from esphome.core import EsphomeError

    with (
        patch("esphome.loader.CORE_COMPONENTS_PATH", tmp_path),
        pytest.raises(EsphomeError, match="shared"),
    ):
        _build_alias_map()


def test_build_alias_map_handles_missing_dir(tmp_path: Path) -> None:
    """If the components directory doesn't exist (unlikely in production,
    but possible in some test contexts), we want an empty map rather than
    a crash — the rest of the loader can still function."""
    fake = tmp_path / "does-not-exist"
    with patch("esphome.loader.CORE_COMPONENTS_PATH", fake):
        alias_map, meta_map = _build_alias_map()
    assert alias_map == {}
    assert meta_map == {}


def test_build_alias_map_rejects_alias_shadowing_component(tmp_path: Path) -> None:
    """An alias that names an existing component package is refused: it would
    hijack a live domain, and a self-alias (alias == canonical) would send
    ``_lookup_module`` into infinite recursion."""
    # `newcomp` declares itself as an alias — its own package already exists.
    _write_component(tmp_path, "newcomp", "ALIASES = ['newcomp']\n")

    from esphome.core import EsphomeError

    with (
        patch("esphome.loader.CORE_COMPONENTS_PATH", tmp_path),
        pytest.raises(EsphomeError, match="shadows an existing component"),
    ):
        _build_alias_map()


# ---- Integration against a synthetic alias map (fake legacy -> esp32) ----


def _patch_alias_map(monkeypatch: pytest.MonkeyPatch, mapping: dict[str, str]) -> None:
    """Force the loader's alias map (used by the finder and get_component).

    Patches the lazily-built caches so both ``_get_alias_map`` and the
    installed meta-path finder resolve against ``mapping`` regardless of
    what the real on-disk scan would produce.
    """
    monkeypatch.setattr("esphome.loader._get_alias_map", lambda: mapping)


def test_get_component_resolves_alias(monkeypatch: pytest.MonkeyPatch) -> None:
    """``get_component(<alias>)`` should return the canonical manifest — every
    caller of the loader (dep checker, schema validator, codegen) hits
    the canonical component without knowing about the alias."""
    import esphome.loader as loader_mod

    _patch_alias_map(monkeypatch, {_FAKE_ALIAS: "esp32"})
    loader_mod._COMPONENT_CACHE.pop(_FAKE_ALIAS, None)

    canonical = get_component("esp32")
    aliased = get_component(_FAKE_ALIAS)
    assert canonical is not None
    assert aliased is canonical


def test_alias_finder_resolves_top_level_import(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """``import esphome.components.<alias>`` resolves to the canonical
    module via the meta-path finder. ``_FAKE_ALIAS`` == ``esp32_legacy_alias``."""
    _patch_alias_map(monkeypatch, {_FAKE_ALIAS: "esp32"})
    sys.modules.pop(f"esphome.components.{_FAKE_ALIAS}", None)

    finder = _AliasFinder()
    spec = finder.find_spec(f"esphome.components.{_FAKE_ALIAS}", None)
    assert spec is not None

    import esphome.components.esp32
    import esphome.components.esp32_legacy_alias

    assert esphome.components.esp32_legacy_alias is esphome.components.esp32


def test_alias_finder_resolves_submodule_import(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """``from esphome.components.<alias> import boards`` routes through to
    ``esphome.components.esp32.boards`` — same submodule object on both paths.

    The canonical submodule is imported first so its parent module carries
    the ``boards`` attribute; ``from <alias> import boards`` then resolves
    the aliased parent (via the finder) and reads that same attribute,
    rather than triggering a fresh file load under the alias name.
    ``_FAKE_ALIAS`` == ``esp32_legacy_alias``."""
    _patch_alias_map(monkeypatch, {_FAKE_ALIAS: "esp32"})
    sys.modules.pop(f"esphome.components.{_FAKE_ALIAS}", None)

    finder = _AliasFinder()
    spec = finder.find_spec(f"esphome.components.{_FAKE_ALIAS}.boards", None)
    assert spec is not None

    from esphome.components.esp32 import boards as canonical_boards
    from esphome.components.esp32_legacy_alias import boards as aliased_boards

    assert aliased_boards is canonical_boards


def test_alias_finder_ignores_non_components_path() -> None:
    """The finder must scope itself to ``esphome.components.<X>`` —
    everything else (other esphome submodules, third-party packages) is
    left for the normal import machinery."""
    finder = _AliasFinder()
    assert finder.find_spec("esphome.core", None) is None
    assert finder.find_spec("os.path", None) is None
    # `esphome.components` itself (no domain segment) is not a candidate.
    assert finder.find_spec("esphome.components", None) is None
    # A real, non-aliased component domain defers to normal import machinery
    # (no component declares an alias in this repo, so the live map is empty).
    assert finder.find_spec("esphome.components.logger", None) is None


# ---------------------------------------------------------------------------
# YAML pre-pass: top-level key rename + centralized deprecation warning
# ---------------------------------------------------------------------------
#
# The companion to the loader-side alias map: ``esphome.config`` runs a
# pre-pass over the user's parsed YAML that rewrites legacy top-level keys
# to their canonical names, surfacing a one-shot deprecation warning. These
# tests inject a synthetic alias-metadata map so the rewrite behavior, the
# warning text, and the both-keys-present conflict can be tested in isolation.


def _patch_alias_metadata(
    monkeypatch: pytest.MonkeyPatch, mapping: dict[str, AliasMeta]
) -> None:
    monkeypatch.setattr("esphome.loader.get_alias_metadata", lambda: mapping)


def test_resolve_component_aliases_renames_legacy_key(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A legacy alias key should be renamed to the canonical key and a
    deprecation warning citing the removal version logged."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version="2027.6.0")},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)  # ensure the warning fires
    config = {"esphome": {"name": "test"}, "oldcomp": {"board": "x"}}

    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        _resolve_component_aliases(config)

    assert "oldcomp" not in config
    assert config["newcomp"] == {"board": "x"}
    assert any(
        "'oldcomp:' top-level key is deprecated" in record.message
        and "rename it to 'newcomp:'" in record.message
        and "2027.6.0" in record.message
        for record in caplog.records
    )


def test_resolve_component_aliases_dedupes_warning_within_a_run(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Schema validators can run twice (auto-load discovery + final pass)
    so the rename pass must emit the warning only once per alias per run.
    Deduped via ``CORE.data``; cleared between runs."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        _resolve_component_aliases({"oldcomp": {"board": "a"}})
        _resolve_component_aliases({"oldcomp": {"board": "b"}})

    matches = [
        r
        for r in caplog.records
        if "'oldcomp:' top-level key is deprecated" in r.message
    ]
    assert len(matches) == 1


def test_resolve_component_aliases_rejects_both_keys_present(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """If the user has BOTH legacy and canonical keys, silently dropping
    one would hide a real misconfiguration. Raise instead."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"newcomp": {"board": "x"}, "oldcomp": {"board": "x"}}
    with pytest.raises(vol.Invalid, match="Both 'oldcomp:'"):
        _resolve_component_aliases(config)


def test_resolve_component_aliases_rejects_canonical_key_after_legacy(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The both-keys conflict must be detected even when the canonical key
    appears *after* the legacy key in the config (the up-front conflict
    scan, not a position-dependent check)."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"oldcomp": {"board": "x"}, "newcomp": {"board": "x"}}
    with pytest.raises(vol.Invalid, match="Both 'oldcomp:'"):
        _resolve_component_aliases(config)


def test_resolve_component_aliases_rejects_multiple_aliases_of_one_component(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Two different deprecated aliases of the same canonical component is
    ambiguous — silently keeping one would hide a misconfiguration."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {
            "oldcomp": AliasMeta(canonical="newcomp", removal_version=None),
            "legacycomp": AliasMeta(canonical="newcomp", removal_version=None),
        },
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"oldcomp": {"board": "x"}, "legacycomp": {"board": "y"}}
    with pytest.raises(vol.Invalid, match=r"Multiple deprecated aliases of 'newcomp:'"):
        _resolve_component_aliases(config)


def test_resolve_component_aliases_preserves_key_position(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The renamed canonical key keeps the legacy key's original position
    rather than being moved to the end of the config."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"esphome": {"name": "t"}, "oldcomp": {"board": "x"}, "logger": {}}

    _resolve_component_aliases(config)

    assert list(config) == ["esphome", "newcomp", "logger"]


def test_resolve_component_aliases_no_op_when_no_legacy_keys(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """The pre-pass must be a no-op (no warning, no mutation) for configs
    that already use canonical keys."""
    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    _patch_alias_metadata(
        monkeypatch,
        {"oldcomp": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"esphome": {"name": "test"}, "newcomp": {"board": "x"}}
    original = dict(config)

    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        _resolve_component_aliases(config)

    assert config == original
    assert not any("deprecated" in r.message for r in caplog.records)


# ---------------------------------------------------------------------------
# ComponentManifest alias properties
# ---------------------------------------------------------------------------


def test_component_manifest_alias_properties_default_empty() -> None:
    """``aliases`` / ``alias_removal_version`` fall back to ``[]`` / ``None``
    when the component module declares neither.

    Uses a real ``ModuleType`` rather than a ``MagicMock`` so that the
    ``getattr(..., default)`` fallback is actually exercised — a bare mock
    auto-creates any attribute on access and would never hit the default."""
    mod = ModuleType("fake_component")
    manifest = ComponentManifest(mod)
    assert manifest.aliases == []
    assert manifest.alias_removal_version is None


def test_component_manifest_alias_properties_read_module_values() -> None:
    """The properties surface the module's declared values verbatim."""
    mod = MagicMock()
    mod.ALIASES = ["legacy"]
    mod.ALIAS_REMOVAL_VERSION = "2027.6.0"
    manifest = ComponentManifest(mod)
    assert manifest.aliases == ["legacy"]
    assert manifest.alias_removal_version == "2027.6.0"


# ---------------------------------------------------------------------------
# Real (unpatched) lazy build + cache and remaining scanner branches
# ---------------------------------------------------------------------------


def test_get_alias_map_real_build_and_caches(monkeypatch: pytest.MonkeyPatch) -> None:
    """Exercise the real lazy build over the actual components dir (no patch):
    the first call scans and caches, the second returns the cached object."""
    monkeypatch.setattr(loader_mod, "_ALIAS_MAP_CACHE", None)
    first = loader_mod._get_alias_map()
    second = loader_mod._get_alias_map()
    assert isinstance(first, dict)
    assert first is second  # cached, not rebuilt on the second call


def test_get_alias_metadata_real_build_and_caches(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(loader_mod, "_ALIAS_META_CACHE", None)
    first = loader_mod.get_alias_metadata()
    second = loader_mod.get_alias_metadata()
    assert isinstance(first, dict)
    assert first is second


def test_build_alias_map_skips_files_and_initless_dirs(tmp_path: Path) -> None:
    """Loose files and directories without an ``__init__.py`` are ignored;
    only real component packages contribute to the map."""
    (tmp_path / "loose_file.py").write_text("ALIASES = ['ignored']\n")
    (tmp_path / "initless").mkdir()  # a dir, but no __init__.py
    _write_component(tmp_path, "realcomp", "ALIASES = ['legacy']\n")

    with patch("esphome.loader.CORE_COMPONENTS_PATH", tmp_path):
        alias_map, _ = _build_alias_map()

    assert alias_map == {"legacy": "realcomp"}


def test_read_aliases_ignores_non_assignment_and_complex_targets(
    tmp_path: Path,
) -> None:
    """Non-assignment statements and assignments to non-Name targets are
    skipped; only simple ``NAME = ...`` assignments are read."""
    init = tmp_path / "__init__.py"
    init.write_text(
        "import os\n"  # non-Assign (Import) node -> skipped
        "obj.attr = 'v'\n"  # Assign with an Attribute target -> skipped
        "ALIASES = ['legacy']\n"
    )
    aliases, _ = _read_aliases(init, ast)
    assert aliases == ["legacy"]


# ---------------------------------------------------------------------------
# Finder / loader edge branches
# ---------------------------------------------------------------------------


def test_alias_finder_returns_none_when_canonical_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """If an alias points at a canonical *target* that doesn't exist, the
    finder declines (returns None) and lets normal import machinery report
    the missing module."""
    _patch_alias_map(monkeypatch, {"broken_alias": "definitely_not_a_real_component"})
    finder = _AliasFinder()
    assert finder.find_spec("esphome.components.broken_alias", None) is None


def test_alias_finder_reraises_when_canonical_dependency_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """If the canonical module exists but fails to import one of its own
    dependencies, the finder surfaces that real error instead of masking it
    as an unresolved alias (which would silently fall through to a confusing
    'no module named <alias>')."""
    _patch_alias_map(monkeypatch, {"some_alias": "real_canonical"})

    def boom(name: str) -> None:
        raise ModuleNotFoundError("No module named 'missing_dep'", name="missing_dep")

    monkeypatch.setattr("esphome.loader.importlib.import_module", boom)
    finder = _AliasFinder()
    with pytest.raises(ModuleNotFoundError, match="missing_dep"):
        finder.find_spec("esphome.components.some_alias", None)


def test_install_alias_finder_is_idempotent() -> None:
    """The finder is installed once at import; calling the installer again is
    a no-op (no duplicate ``_AliasFinder`` on ``sys.meta_path``)."""
    before = [e for e in sys.meta_path if isinstance(e, _AliasFinder)]
    assert len(before) == 1  # installed at module import time
    loader_mod._install_alias_finder()
    after = [e for e in sys.meta_path if isinstance(e, _AliasFinder)]
    assert len(after) == 1


def test_get_component_alias_to_missing_canonical_returns_none(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """If an alias resolves to a canonical component that can't be loaded,
    ``get_component`` returns None and caches no bogus manifest."""
    _patch_alias_map(monkeypatch, {"ghost_alias": "definitely_not_a_real_component"})
    loader_mod._COMPONENT_CACHE.pop("ghost_alias", None)

    assert get_component("ghost_alias") is None
    assert "ghost_alias" not in loader_mod._COMPONENT_CACHE


# ---------------------------------------------------------------------------
# YAML pre-pass: empty-map fast path + validate_config integration
# ---------------------------------------------------------------------------


def test_resolve_component_aliases_noop_when_no_aliases_declared(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """When no component declares an alias, the pre-pass returns immediately
    without inspecting or mutating the config."""
    from esphome.config import _resolve_component_aliases

    monkeypatch.setattr("esphome.loader.get_alias_metadata", dict)  # empty map
    config = {"esphome": {"name": "t"}, "rp2040": {"board": "x"}}
    original = dict(config)
    _resolve_component_aliases(config)
    assert config == original


def _default_component_mock() -> Mock:
    """A permissive component mock that validates any config (ALLOW_EXTRA)."""
    return Mock(
        auto_load=[],
        is_platform_component=False,
        is_platform=False,
        multi_conf=False,
        multi_conf_no_default=False,
        dependencies=[],
        conflicts_with=[],
        config_schema=cv.Schema({}, extra=cv.ALLOW_EXTRA),
    )


@pytest.mark.usefixtures("setup_core")
def test_validate_config_renames_alias_key(
    mock_get_component: Mock, monkeypatch: pytest.MonkeyPatch
) -> None:
    """End-to-end: a legacy top-level key is renamed to its canonical name
    before the rest of ``validate_config`` runs, and validation succeeds.

    A real ``esp32`` target platform is included so ``preload_core_config``
    is satisfied and validation runs to completion (the renamed canonical
    key is loaded via the mocked, permissive component)."""
    mock_get_component.side_effect = lambda name: _default_component_mock()
    monkeypatch.setattr(
        "esphome.loader.get_alias_metadata",
        lambda: {
            "legacyfoo": AliasMeta(canonical="newcomp", removal_version="2027.6.0")
        },
    )
    CORE.data.pop("_component_aliases_warned", None)

    raw_config = {
        "esphome": {"name": "test"},
        "esp32": {"board": "esp32dev"},
        "legacyfoo": {"opt": 1},
    }
    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"unexpected errors: {result.errors}"
    assert "newcomp" in result
    assert "legacyfoo" not in result


@pytest.mark.usefixtures("setup_core")
def test_validate_config_reports_alias_conflict_as_error(
    mock_get_component: Mock, monkeypatch: pytest.MonkeyPatch
) -> None:
    """If both the legacy and canonical keys are present, ``validate_config``
    surfaces the conflict as a config error (the ``vol.Invalid`` path)."""
    mock_get_component.return_value = _default_component_mock()
    monkeypatch.setattr(
        "esphome.loader.get_alias_metadata",
        lambda: {"legacyfoo": AliasMeta(canonical="newcomp", removal_version=None)},
    )
    CORE.data.pop("_component_aliases_warned", None)

    raw_config = {
        "esphome": {"name": "test"},
        "newcomp": {"opt": 1},
        "legacyfoo": {"opt": 2},
    }
    result = esphome_config.validate_config(raw_config, {})

    assert result.errors
    assert "Both 'legacyfoo:'" in str(result.errors)
