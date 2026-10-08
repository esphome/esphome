"""Unit tests for script/clang_tidy_hash.py module."""

from pathlib import Path
import sys

import pytest

# Add the script directory to Python path so we can import clang_tidy_hash
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import clang_tidy_hash  # noqa: E402
from clang_tidy_hash import CLANG_TIDY_GLOBAL_FILES  # noqa: E402


def _populate(repo_root: Path) -> None:
    """Create every clang-tidy global file plus a base sdkconfig.defaults."""
    for name in CLANG_TIDY_GLOBAL_FILES:
        path = repo_root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"contents of {name}\n")
    (repo_root / "sdkconfig.defaults").write_text("CONFIG_BASE=y\n")


def test_calculate_clang_tidy_hash_is_deterministic(tmp_path: Path) -> None:
    """Same inputs must produce the same hash."""
    _populate(tmp_path)
    assert clang_tidy_hash.calculate_clang_tidy_hash(
        repo_root=tmp_path
    ) == clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)


@pytest.mark.parametrize("filename", CLANG_TIDY_GLOBAL_FILES)
def test_calculate_clang_tidy_hash_changes_with_each_global_file(
    tmp_path: Path, filename: str
) -> None:
    """Editing any global file must change the hash."""
    _populate(tmp_path)
    before = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    (tmp_path / filename).write_text("changed\n")
    after = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    assert after != before


def test_calculate_clang_tidy_hash_includes_per_target_sdkconfig(
    tmp_path: Path,
) -> None:
    """Per-target sdkconfig.defaults.<target> files must be part of the hash."""
    _populate(tmp_path)
    before = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    per_target = tmp_path / "sdkconfig.defaults.esp32c6"
    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=y\n")
    after_add = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_add != before

    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=n\n")
    after_edit = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_edit != after_add


def test_calculate_clang_tidy_hash_handles_missing_optional_files(
    tmp_path: Path,
) -> None:
    """Hash calculation must not fail when files are absent."""
    (tmp_path / ".clang-tidy").write_text("Checks: '-*'\n")
    result = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert len(result) == 64  # sha256 hexdigest length


def test_read_file_bytes(tmp_path: Path) -> None:
    """Test read_file_bytes helper function."""
    test_file = tmp_path / "test.bin"
    test_content = b"binary content\x00\xff"
    test_file.write_bytes(test_content)

    result = clang_tidy_hash.read_file_bytes(test_file)

    assert result == test_content


def test_global_files_exist_in_real_repo() -> None:
    """Every required entry must resolve under the real repo root.

    A stale/renamed/typo'd path is silently skipped by calculate_clang_tidy_hash()'s
    exists() check, which would quietly weaken the cache key and the full-scan
    trigger with no error -- guard against that here since the other tests only
    ever populate a synthetic tmp_path root.
    """
    repo_root = clang_tidy_hash.get_repo_root()
    for name in CLANG_TIDY_GLOBAL_FILES:
        assert (repo_root / name).is_file(), f"{name} not found under repo root"


def test_is_cached_missing_file_returns_false(tmp_path: Path) -> None:
    """A hash file that doesn't exist is never cached."""
    assert not clang_tidy_hash.is_cached(tmp_path / "missing.hash", "abc123")


def test_update_cache_then_is_cached_true(tmp_path: Path) -> None:
    """A freshly written cache file is reported as cached."""
    hash_path = tmp_path / ".compile_commands.hash"

    clang_tidy_hash.update_cache(hash_path, "abc123")

    assert clang_tidy_hash.is_cached(hash_path, "abc123")


def test_is_cached_false_after_global_hash_changes(tmp_path: Path) -> None:
    """A cache written under one key is stale once the caller's key changes."""
    hash_path = tmp_path / ".compile_commands.hash"
    clang_tidy_hash.update_cache(hash_path, "abc123")

    assert not clang_tidy_hash.is_cached(hash_path, "def456")


def test_cache_key_distinguishes_extra(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The nrf52 source-file list (passed as ``extra``) is part of the cache key."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")

    key_a = clang_tidy_hash.cache_key(extra="file_a.cpp\nfile_b.cpp")
    key_c = clang_tidy_hash.cache_key(extra="file_c.cpp")

    assert key_a != key_c


def test_cache_key_empty_extra_equals_bare_hash(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """An empty ``extra`` must produce the same key as calculate_clang_tidy_hash() alone."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")

    assert clang_tidy_hash.cache_key("") == clang_tidy_hash.calculate_clang_tidy_hash()


def test_calculate_idedata_cache_hash_changes_with_infra_code(tmp_path: Path) -> None:
    _populate(tmp_path)
    infra = tmp_path / "esphome" / "espidf" / "clang_tidy.py"
    infra.parent.mkdir(parents=True)
    infra.write_text("a")
    before = clang_tidy_hash.calculate_idedata_cache_hash(repo_root=tmp_path)
    assert before == clang_tidy_hash.calculate_idedata_cache_hash(repo_root=tmp_path)
    infra.write_text("b")
    assert clang_tidy_hash.calculate_idedata_cache_hash(repo_root=tmp_path) != before


def test_calculate_idedata_cache_hash_includes_listed_files(tmp_path: Path) -> None:
    _populate(tmp_path)
    before = clang_tidy_hash.calculate_idedata_cache_hash(repo_root=tmp_path)
    listed = tmp_path / "esphome" / "platformio" / "extra_script.py"
    listed.parent.mkdir(parents=True, exist_ok=True)
    listed.write_text("x")
    assert clang_tidy_hash.calculate_idedata_cache_hash(repo_root=tmp_path) != before


def test_idedata_cache_hash_only_widens_for_esp32(tmp_path: Path) -> None:
    _populate(tmp_path)
    infra = tmp_path / "esphome" / "espidf" / "clang_tidy.py"
    infra.parent.mkdir(parents=True)
    infra.write_text("a")
    esp32_before = clang_tidy_hash.idedata_cache_hash("esp32-idf-tidy", tmp_path)
    other_before = clang_tidy_hash.idedata_cache_hash("esp8266-arduino-tidy", tmp_path)
    infra.write_text("b")
    assert (
        clang_tidy_hash.idedata_cache_hash("esp32-idf-tidy", tmp_path) != esp32_before
    )
    assert (
        clang_tidy_hash.idedata_cache_hash("esp8266-arduino-tidy", tmp_path)
        == other_before
    )
