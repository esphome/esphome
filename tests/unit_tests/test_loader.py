"""Unit tests for esphome.loader module."""

import ast
from pathlib import Path
import sys
import textwrap
from unittest.mock import MagicMock, patch

import pytest

from esphome.loader import (
    AliasMeta,
    ComponentManifest,
    _AliasFinder,
    _build_alias_map,
    _read_aliases,
    _replace_component_manifest,
    get_alias_metadata,
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
# The framework here is the substrate behind `ALIASES = [...]` on component
# `__init__.py` files. These tests pin down the AST scanner, the resulting
# global alias map, the Python-import `sys.meta_path` finder, and the
# integration with `get_component`. The rp2 → rp2040 actual mapping in this
# repo is used as a real-world fixture; other cases use temp dirs / mocks so
# the framework's behavior is testable in isolation.


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
            ALIAS_REMOVAL_VERSION = "2027.7.0"
            """)
    )
    aliases, removal = _read_aliases(init, ast)
    assert aliases == ["old"]
    assert removal == "2027.7.0"


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


def test_read_aliases_handles_syntax_error(tmp_path: Path) -> None:
    """A broken __init__.py shouldn't crash the alias scanner — it'll
    surface as an ImportError elsewhere, but the scanner just yields
    nothing so other components keep working.

    The source must contain the substring ``ALIASES`` so the scanner
    actually attempts to parse the file; otherwise the early-return
    optimization would short-circuit before reaching the parser and
    this test would not exercise the syntax-error branch.
    """
    init = tmp_path / "__init__.py"
    init.write_text("ALIASES = ['oops'\ndef broken( :\n")
    assert _read_aliases(init, ast) == ([], None)


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
    assert not fake.exists()
    with patch("esphome.loader.CORE_COMPONENTS_PATH", fake):
        alias_map, meta_map = _build_alias_map()
    assert alias_map == {}
    assert meta_map == {}


# ---- Live integration against the real rp2/rp2040 mapping in this repo ----


def test_real_alias_map_includes_rp2040() -> None:
    """The rp2 component declares ``ALIASES = ['rp2040']`` in this repo;
    the live alias map should surface it. This guards against future
    refactors silently dropping the declaration."""
    meta = get_alias_metadata()
    assert "rp2040" in meta
    assert meta["rp2040"].canonical == "rp2"
    assert meta["rp2040"].removal_version == "2027.7.0"


def test_get_component_resolves_alias() -> None:
    """``get_component('rp2040')`` should return the rp2 manifest — every
    caller of the loader (dep checker, schema validator, codegen) hits
    the canonical component without knowing about the alias."""
    rp2 = get_component("rp2")
    rp2040 = get_component("rp2040")
    assert rp2 is not None
    assert rp2040 is rp2


def test_alias_finder_resolves_top_level_import() -> None:
    """``import esphome.components.rp2040`` resolves to the canonical
    module via the meta-path finder."""
    # Remove any cached entry so we exercise the finder, not sys.modules cache.
    sys.modules.pop("esphome.components.rp2040", None)
    finder = _AliasFinder()
    spec = finder.find_spec("esphome.components.rp2040", None)
    assert spec is not None

    import esphome.components.rp2
    import esphome.components.rp2040

    assert esphome.components.rp2040 is esphome.components.rp2


def test_alias_finder_resolves_submodule_import() -> None:
    """``from esphome.components.rp2040 import boards`` routes through to
    ``esphome.components.rp2.boards`` — same submodule object on both
    paths."""
    sys.modules.pop("esphome.components.rp2040.boards", None)
    finder = _AliasFinder()
    spec = finder.find_spec("esphome.components.rp2040.boards", None)
    assert spec is not None

    from esphome.components.rp2 import boards as rp2_boards
    from esphome.components.rp2040 import boards as rp2040_boards

    assert rp2040_boards is rp2_boards


def test_alias_finder_ignores_non_components_path() -> None:
    """The finder must scope itself to ``esphome.components.<X>`` —
    everything else (other esphome submodules, third-party packages) is
    left for the normal import machinery."""
    finder = _AliasFinder()
    assert finder.find_spec("esphome.core", None) is None
    assert finder.find_spec("os.path", None) is None
    # `esphome.components` itself (no domain segment) is not a candidate.
    assert finder.find_spec("esphome.components", None) is None


# ---------------------------------------------------------------------------
# YAML pre-pass: top-level key rename + centralized deprecation warning
# ---------------------------------------------------------------------------
#
# The companion to the loader-side alias map: ``esphome.config`` runs a
# pre-pass over the user's parsed YAML that rewrites legacy top-level keys
# to their canonical names, surfacing a one-shot deprecation warning. These
# tests pin down the rewrite behavior, the warning text, and the
# both-keys-present conflict.


def test_resolve_component_aliases_renames_legacy_key(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A legacy alias key ``rp2040:`` should be renamed to the canonical
    ``rp2:`` and a deprecation warning citing the removal version logged."""
    import logging

    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    CORE.data.pop(_ALIAS_WARNED_KEY, None)  # ensure the warning fires
    config = {"esphome": {"name": "test"}, "rp2040": {"board": "rpipicow"}}

    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        _resolve_component_aliases(config)

    assert "rp2040" not in config
    assert config["rp2"] == {"board": "rpipicow"}
    assert any(
        "'rp2040:' top-level key is deprecated" in record.message
        and "rename it to 'rp2:'" in record.message
        and "2027.7.0" in record.message
        for record in caplog.records
    )


def test_resolve_component_aliases_dedupes_warning_within_a_run(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Schema validators can run twice (auto-load discovery + final pass)
    so the rename pass must emit the warning only once per alias per run.
    Deduped via ``CORE.data``; cleared between runs."""
    import logging

    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        _resolve_component_aliases({"rp2040": {"board": "rpipicow"}})
        _resolve_component_aliases({"rp2040": {"board": "rpipico2w"}})

    matches = [
        r
        for r in caplog.records
        if "'rp2040:' top-level key is deprecated" in r.message
    ]
    assert len(matches) == 1


def test_resolve_component_aliases_rejects_both_keys_present() -> None:
    """If the user has BOTH legacy and canonical keys, silently dropping
    one would hide a real misconfiguration. Raise instead."""
    import voluptuous as vol

    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {
        "rp2": {"board": "rpipicow"},
        "rp2040": {"board": "rpipicow"},
    }
    with pytest.raises(vol.Invalid, match="Both 'rp2040:'"):
        _resolve_component_aliases(config)


def test_resolve_component_aliases_no_op_when_no_legacy_keys() -> None:
    """The pre-pass must be a no-op (no warning, no mutation) for configs
    that already use canonical keys."""
    import logging

    from esphome.config import _ALIAS_WARNED_KEY, _resolve_component_aliases
    from esphome.core import CORE

    CORE.data.pop(_ALIAS_WARNED_KEY, None)
    config = {"esphome": {"name": "test"}, "rp2": {"board": "rpipicow"}}
    original = dict(config)

    with caplog_at_warning() as records:
        _resolve_component_aliases(config)

    assert config == original
    assert not any("deprecated" in r.message for r in records)
    _ = logging  # silence unused-import in branches that don't read records


# Helper context manager — small enough to inline rather than pull in
# caplog for the simple "did anything warn?" case above.
import contextlib  # noqa: E402


@contextlib.contextmanager
def caplog_at_warning():
    """Minimal in-test caplog substitute: collect WARNING records on a
    dedicated handler attached to ``esphome.config``."""
    import logging

    logger = logging.getLogger("esphome.config")
    records: list[logging.LogRecord] = []

    class _Handler(logging.Handler):
        def emit(self, record):  # noqa: D401
            records.append(record)

    handler = _Handler(level=logging.WARNING)
    logger.addHandler(handler)
    prev_level = logger.level
    logger.setLevel(logging.WARNING)
    try:
        yield records
    finally:
        logger.removeHandler(handler)
        logger.setLevel(prev_level)
