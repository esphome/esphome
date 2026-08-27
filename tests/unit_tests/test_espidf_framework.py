"""Tests for esphome.espidf.framework helpers."""

# pylint: disable=protected-access

from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import importlib.util
import io
import json
import logging
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tarfile
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome.espidf.framework import (
    ESPHOME_STAMP_FILE,
    STAMP_SCHEMA_VERSION,
    _ccache_env,
    _check_esphome_idf_framework_install,
    _check_stamp,
    _check_windows_path_length,
    _clone_idf_with_submodules,
    _get_framework_path,
    _get_idf_tool_paths,
    _get_idf_version,
    _get_python_env_path,
    _get_python_version,
    _parse_git_source,
    _patch_tools_json_demote_unused_tools,
    _patch_tools_json_for_linux_arm64,
    _prefetch_idf_tool_archives,
    _read_stamp,
    _stamp_covers,
    _windows_long_paths_enabled,
    _write_idf_version_txt,
    _write_stamp,
    check_esp_idf_install,
    get_framework_env,
    get_idf_tools_path,
)
from esphome.framework_helpers import _tar_extract_all, get_python_env_executable_path


@pytest.fixture(autouse=True)
def _isolate_idf_install_path(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Pin the ESP-IDF install root to a tmp dir for every test.

    The default location is the OS user cache dir, so without this any test
    that builds framework paths or pre-creates the framework dir would touch
    the real ``~/.cache/esphome`` on the developer's machine. Tests that need
    to exercise the override or default-resolution logic clear/override the env
    themselves.
    """
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(tmp_path / "idf_install"))


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        # github:// shorthand
        (
            "github://espressif/esp-idf",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "github://espressif/esp-idf@release/v6.0",
            ("https://github.com/espressif/esp-idf.git", "release/v6.0"),
        ),
        # explicit https://github.com/...git URL
        (
            "https://github.com/espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "https://github.com/espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "https://github.com/espressif/esp-idf.git@v6.0.1",
            ("https://github.com/espressif/esp-idf.git", "v6.0.1"),
        ),
        # '#' ref separator (PlatformIO/git-web convention) works on both forms
        (
            "https://github.com/espressif/esp-idf.git#release/v6.1",
            ("https://github.com/espressif/esp-idf.git", "release/v6.1"),
        ),
        (
            "github://espressif/esp-idf#release/v6.1",
            ("https://github.com/espressif/esp-idf.git", "release/v6.1"),
        ),
        (
            "github://espressif/esp-idf.git#master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        # Tolerate a trailing ".git" on the shorthand so the user doesn't
        # silently end up with a doubled "...esp-idf.git.git" URL.
        (
            "github://espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
    ],
)
def test_parse_git_source_recognized(
    source: str, expected: tuple[str, str | None]
) -> None:
    assert _parse_git_source(source) == expected


@pytest.mark.parametrize(
    "source",
    [
        # archive URLs fall through to the existing download path
        "https://github.com/espressif/esp-idf/archive/refs/heads/master.zip",
        "https://dl.espressif.com/dl/esp-idf/v6.0.1/esp-idf-v6.0.1.zip",
        "https://github.com/esphome-libs/esp-idf/releases/download/v5.5.4/esp-idf-v5.5.4.tar.xz",
        # SSH and other git protocols are intentionally rejected — match
        # external_components, which only recognizes github:// + structured
        # dicts for these.
        "git@github.com:espressif/esp-idf.git",
        "ssh://git@github.com/espressif/esp-idf.git",
        "git://github.com/espressif/esp-idf.git",
        # non-GitHub .git URLs are intentionally rejected for the same reason
        "https://gitlab.com/foo/bar.git",
        "https://github.example.com/foo/bar.git",
    ],
)
def test_parse_git_source_rejected(source: str) -> None:
    assert _parse_git_source(source) is None


def _make_idf_tree(framework_path: Path, *, gitmodules: bool = True) -> None:
    """Create the minimum tree _clone_idf_with_submodules sanity-checks for.

    ``gitmodules=False`` simulates a fork that vendors components in-tree
    instead of declaring submodules; update_submodules skips the git call
    when that file is missing.
    """
    (framework_path / "tools").mkdir(parents=True)
    (framework_path / "tools" / "idf_tools.py").write_text("# stub\n")
    if gitmodules:
        (framework_path / ".gitmodules").write_text("# stub\n")


def test_clone_idf_with_submodules_without_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path, "https://github.com/espressif/esp-idf.git", None
        )

    # No ref -> just clone + submodule update, no fetch/reset.
    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    assert calls[0] == [
        "git",
        "clone",
        "--depth=1",
        "--",
        "https://github.com/espressif/esp-idf.git",
        str(framework_path),
    ]
    assert calls[-1][:5] == ["git", "submodule", "update", "--init", "--recursive"]
    assert not any(c[1] == "fetch" for c in calls)
    assert not any(c[1] == "reset" for c in calls)
    # The clone must retry transient network failures and clean up a
    # partial destination between attempts
    clone_kwargs = run_git_command_mock.call_args_list[0].kwargs
    assert clone_kwargs["network"] is True
    assert clone_kwargs["retry_cleanup"] == framework_path


def test_clone_idf_with_submodules_with_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            "master",
        )

    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    # clone, fetch ref, reset hard, submodule update
    assert calls[0][:2] == ["git", "clone"]
    assert calls[1] == [
        "git",
        "fetch",
        "--depth=1",
        "--",
        "origin",
        "master",
    ]
    assert calls[2] == ["git", "reset", "--hard", "FETCH_HEAD"]
    assert calls[3][:5] == ["git", "submodule", "update", "--init", "--recursive"]
    # Clone and fetch talk to the network and must carry the retry flag;
    # the local reset must not
    kwargs = [c.kwargs for c in run_git_command_mock.call_args_list]
    assert kwargs[0]["network"] is True
    assert kwargs[0]["retry_cleanup"] == framework_path
    assert kwargs[1]["network"] is True
    assert "network" not in kwargs[2]


def test_clone_idf_with_submodules_raises_when_tree_missing(
    tmp_path: Path,
) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    # Deliberately do NOT call _make_idf_tree — simulate a clone that
    # returned 0 but produced no tools/idf_tools.py.

    with (
        patch("esphome.git.run_git_command", return_value=""),
        pytest.raises(RuntimeError, match="no usable ESP-IDF tree"),
    ):
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            None,
        )


def test_clone_idf_accepts_flattened_fork_without_gitmodules(
    tmp_path: Path,
) -> None:
    """A fork that vendors components in-tree instead of as submodules is valid.

    No .gitmodules means the submodule step is skipped entirely.
    """
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path, gitmodules=False)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/example/flattened-esp-idf.git",
            None,
        )

    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    assert not any(c[1] == "submodule" for c in calls)


# ---------------------------------------------------------------------------
# Helpers for _tar_extract_all hard-link prefix-stripping tests
# ---------------------------------------------------------------------------


def _make_tar(
    members: list[tarfile.TarInfo], file_contents: dict[str, bytes]
) -> io.BytesIO:
    """Build an in-memory tar archive from a list of TarInfo objects."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tf:
        for info in members:
            if info.isreg() and info.name in file_contents:
                data = file_contents[info.name]
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            else:
                tf.addfile(info)
    buf.seek(0)
    return buf


def _regular(name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.REGTYPE
    info.size = 0
    info.mode = 0o644
    return info


def _hardlink(name: str, linkname: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.LNKTYPE
    info.linkname = linkname
    info.size = 0
    info.mode = 0o644
    return info


class TestTarExtractHardLinkPrefixStripping:
    """
    Covers the hard-link prefix-stripping block in _tar_extract_all (L528-541).

    Archive layout used by every test:

        wrapper/                   ← single top-level wrapper dir (stripped)
        wrapper/target.txt         ← regular file; becomes target.txt in dest
        wrapper/link_good          ← hard link to wrapper/target.txt  (kept, linkname stripped)
        wrapper/link_exact_root    ← hard link to "wrapper"            (skipped – equals strip_root)
        wrapper/link_exact_prefix  ← hard link to "wrapper/"           (skipped – equals strip_prefix)
        wrapper/link_outside       ← hard link to "other/target.txt"   (skipped – not under prefix)
    """

    WRAPPER = "wrapper"

    def _build_archive(self) -> io.BytesIO:
        members = [
            _regular(f"{self.WRAPPER}/"),
            _regular(f"{self.WRAPPER}/target.txt"),
            _hardlink(f"{self.WRAPPER}/link_good", f"{self.WRAPPER}/target.txt"),
            _hardlink(f"{self.WRAPPER}/link_exact_root", self.WRAPPER),
            _hardlink(f"{self.WRAPPER}/link_exact_prefix", f"{self.WRAPPER}/"),
            _hardlink(f"{self.WRAPPER}/link_outside", "other/target.txt"),
        ]
        return _make_tar(members, {f"{self.WRAPPER}/target.txt": b"hello"})

    def test_good_hardlink_is_extracted_with_stripped_linkname(
        self, tmp_path: Path
    ) -> None:
        """Hard link whose linkname starts with wrapper/ is extracted and its
        linkname has the prefix removed so tarfile can resolve the target."""
        _tar_extract_all(self._build_archive(), tmp_path)
        link = tmp_path / "link_good"
        assert link.exists(), "link_good should have been extracted"
        assert link.read_bytes() == b"hello"

    def test_hardlink_equal_to_strip_root_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname equals strip_root exactly must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_exact_root").exists()

    def test_hardlink_equal_to_strip_prefix_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname equals strip_prefix (strip_root + '/') must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_exact_prefix").exists()

    def test_hardlink_outside_prefix_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname does not start with wrapper/ must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_outside").exists()

    def test_regular_file_and_no_spurious_files(self, tmp_path: Path) -> None:
        """Sanity check: target.txt is extracted and no unexpected files appear."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert (tmp_path / "target.txt").read_bytes() == b"hello"
        extracted = {p.name for p in tmp_path.iterdir()}
        assert extracted == {"target.txt", "link_good"}


_IDF_VERSION = "5.1.2"


def _fake_download_from_mirrors(
    mirrors: list[str],
    substitutions: dict[str, str],
    target: object,
    **kwargs: object,
) -> str:
    """Stand-in for download_from_mirrors that creates path targets, since
    the framework code opens the downloaded tarball afterwards."""
    path = Path(target)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()
    return "https://example.com/idf.tar.xz"


@pytest.fixture
def espidf_mocks(setup_core: Path):
    """Patch the heavy I/O of check_esp_idf_install and pre-create the framework dir."""
    # archive_extract_all is mocked, so pre-create the framework dir that the
    # extracted-marker touch writes into.
    _get_framework_path(_IDF_VERSION).mkdir(parents=True, exist_ok=True)
    # One mock covers the tarball (via framework_helpers.download_and_extract)
    # and the constraints file (espidf-bound download_from_mirrors), so call
    # counts and ordering assertions span the two.
    download = MagicMock(side_effect=_fake_download_from_mirrors)
    with (
        patch("esphome.espidf.framework.rmdir") as rmdir_mock,
        patch("esphome.framework_helpers.download_from_mirrors", download),
        patch("esphome.espidf.framework.download_from_mirrors", download),
        patch("esphome.framework_helpers.archive_extract_all") as extract,
        patch("esphome.espidf.framework.create_venv") as venv,
        patch("esphome.espidf.framework.run_command_ok", return_value=True) as run_ok,
        patch(
            "esphome.espidf.framework._get_idf_tool_paths", return_value=([], {})
        ) as tool_paths,
        patch("esphome.espidf.framework._clone_idf_with_submodules") as clone,
        patch("esphome.espidf.framework._write_idf_version_txt"),
        patch("esphome.espidf.framework._patch_tools_json_for_linux_arm64"),
        patch("esphome.espidf.framework._patch_tools_json_demote_unused_tools"),
        patch("esphome.espidf.framework._prefetch_idf_tool_archives"),
        patch("esphome.espidf.framework._write_stamp"),
        patch("esphome.espidf.framework._check_stamp", return_value=True),
        patch("esphome.espidf.framework._stamp_covers", return_value=True),
        patch("esphome.espidf.framework._get_idf_version", return_value=_IDF_VERSION),
        patch("esphome.espidf.framework._get_python_version", return_value="3.11.0"),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        yield SimpleNamespace(
            download=download,
            extract=extract,
            venv=venv,
            run_ok=run_ok,
            tool_paths=tool_paths,
            clone=clone,
            rmdir=rmdir_mock,
        )


def test_check_esp_idf_install_fresh(espidf_mocks: SimpleNamespace) -> None:
    """A forced install drives download/extract, venv creation, and pip installs."""
    framework_path, python_env_path = check_esp_idf_install(_IDF_VERSION, force=True)

    assert framework_path == _get_framework_path(_IDF_VERSION)
    assert python_env_path == _get_python_env_path(_IDF_VERSION)
    # framework tarball + python-env constraints file are both downloaded
    assert espidf_mocks.download.call_count == 2
    espidf_mocks.extract.assert_called_once()
    espidf_mocks.venv.assert_called_once()
    espidf_mocks.clone.assert_not_called()
    # the tool download cache (<IDF_TOOLS_PATH>/dist) is pruned after install
    espidf_mocks.rmdir.assert_any_call(
        get_idf_tools_path() / "dist", msg="Remove ESP-IDF tool download cache"
    )


def test_check_esp_idf_install_dist_prune_failure_ignored(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A failure to prune the tool download cache must not fail the install."""
    tools_dist = get_idf_tools_path() / "dist"

    def rmdir_side_effect(directory: Path, msg: str | None = None) -> None:
        if directory == tools_dist:
            raise RuntimeError("cannot remove dist")

    espidf_mocks.rmdir.side_effect = rmdir_side_effect

    # install still succeeds despite the failed prune
    framework_path, _ = check_esp_idf_install(_IDF_VERSION, force=True)
    assert framework_path == _get_framework_path(_IDF_VERSION)


def test_check_esp_idf_install_git_source(espidf_mocks: SimpleNamespace) -> None:
    """A git source_url clones instead of downloading; explicit tools skip discovery."""
    check_esp_idf_install(
        _IDF_VERSION,
        force=True,
        source_url="https://github.com/espressif/esp-idf.git",
        tools=["xtensa-esp-elf"],
    )

    espidf_mocks.clone.assert_called_once()
    # framework is cloned, so only the python-env constraints file is downloaded
    assert espidf_mocks.download.call_count == 1


def test_check_esp_idf_install_already_installed(espidf_mocks: SimpleNamespace) -> None:
    """Marker + matching stamps + existing python env → nothing is re-installed."""
    framework_path = _get_framework_path(_IDF_VERSION)
    (framework_path / ".esphome_extracted").touch()
    python_env_path = _get_python_env_path(_IDF_VERSION)
    env_python = get_python_env_executable_path(python_env_path, "python")
    env_python.parent.mkdir(parents=True, exist_ok=True)
    env_python.touch()

    check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_not_called()


def test_corrupt_tarball_removed_when_extraction_fails(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A tarball that fails to extract (e.g. torn by an unclean shutdown) is
    deleted so the next run re-downloads instead of failing forever."""
    espidf_mocks.extract.side_effect = RuntimeError("xz: unexpected end of input")
    tarball = get_idf_tools_path() / "dist" / f"esp-idf-{_IDF_VERSION}.tar.xz"

    with pytest.raises(RuntimeError, match="unexpected end of input"):
        check_esp_idf_install(_IDF_VERSION, force=True)

    assert not tarball.exists()


def test_check_esp_idf_install_framework_failure(espidf_mocks: SimpleNamespace) -> None:
    """A failing idf_tools install raises."""
    espidf_mocks.run_ok.side_effect = [False]
    with pytest.raises(RuntimeError, match="framework installation failure"):
        check_esp_idf_install(_IDF_VERSION, force=True)


def test_check_esp_idf_install_pip_upgrade_failure(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A failing pip upgrade in the python env raises (framework install ok)."""
    espidf_mocks.run_ok.side_effect = [True, False]
    with pytest.raises(RuntimeError, match="Python environment packages failure"):
        check_esp_idf_install(_IDF_VERSION, force=True)


def test_check_esp_idf_install_feature_failure(espidf_mocks: SimpleNamespace) -> None:
    """A failing feature requirements install raises."""
    espidf_mocks.run_ok.side_effect = [True, True, False]
    with pytest.raises(RuntimeError, match="Python dependencies for"):
        check_esp_idf_install(_IDF_VERSION, force=True, features=["fb"])


def _mark_installed() -> None:
    """Create the extracted marker and python-env interpreter so the install
    check takes the already-installed path rather than force-installing."""
    (_get_framework_path(_IDF_VERSION) / ".esphome_extracted").touch()
    env_python = get_python_env_executable_path(
        _get_python_env_path(_IDF_VERSION), "python"
    )
    env_python.parent.mkdir(parents=True, exist_ok=True)
    env_python.touch()


def test_check_esp_idf_install_stamp_mismatch_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A stamp mismatch reinstalls tools (marker present, so no re-extract).

    The python env is left alone: it depends on the framework version and
    features, not on which toolchains are installed.
    """
    _mark_installed()
    with patch("esphome.espidf.framework._stamp_covers", return_value=False):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()  # marker present -> no re-extract
    espidf_mocks.venv.assert_not_called()  # tools-only install -> venv kept


def test_check_esp_idf_install_check_command_failure_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A failing tool-path resolution reinstalls tools (marker present, no re-extract)."""
    _mark_installed()
    # Managed tool resolution fails -> install stays True; the later installs succeed.
    espidf_mocks.tool_paths.side_effect = RuntimeError("missing ESP-IDF tool")
    check_esp_idf_install(_IDF_VERSION, features=["fb"])

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_not_called()  # tools-only install -> venv kept


def test_check_esp_idf_install_unknown_python_version_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """An undeterminable python version rebuilds the venv (framework stamp still ok)."""
    _mark_installed()
    with patch("esphome.espidf.framework._get_python_version", return_value=None):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()  # framework stamp matched
    espidf_mocks.venv.assert_called_once()  # python env rebuilt


def test_check_esp_idf_install_python_stamp_mismatch_rebuilds_venv(
    espidf_mocks: SimpleNamespace,
) -> None:
    """Framework stamp matches but the python-env stamp does not -> venv rebuilt."""

    # _check_stamp only guards the python env now (the framework uses
    # _stamp_covers, patched True by the fixture); failing it rebuilds the venv.
    def stamp_ok(_stamp_file, info: dict) -> bool:
        return "python_version" not in info

    _mark_installed()
    with patch("esphome.espidf.framework._check_stamp", side_effect=stamp_ok):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_called_once()


def _requested_stamp(targets: list[str], tools: list[str] | None = None) -> dict:
    return {
        "schema_version": STAMP_SCHEMA_VERSION,
        "targets": targets,
        "tools": tools or ["required"],
    }


@pytest.mark.parametrize(
    ("stored", "targets", "expected"),
    [
        # a stored "all" covers any target
        (_requested_stamp(["all"]), ["esp32"], True),
        # exact match and superset both cover
        (_requested_stamp(["esp32"]), ["esp32"], True),
        (_requested_stamp(["esp32", "esp32c3"]), ["esp32"], True),
        # a new target is not covered
        (_requested_stamp(["esp32"]), ["esp32c3"], False),
        # tools and schema_version must match exactly
        (_requested_stamp(["all"], tools=["cmake", "required"]), ["esp32"], False),
        (_requested_stamp(["all"]) | {"schema_version": "no"}, ["esp32"], False),
        # an unknown extra field participates in invalidation by default
        (_requested_stamp(["all"]) | {"module_version": 1}, ["esp32"], False),
        # missing/corrupt stamps never cover
        (None, ["esp32"], False),
        (
            {"schema_version": STAMP_SCHEMA_VERSION, "tools": ["required"]},
            ["esp32"],
            False,
        ),
    ],
)
def test_stamp_covers(stored: dict | None, targets: list[str], expected: bool) -> None:
    assert _stamp_covers(stored, _requested_stamp(targets)) is expected


@contextmanager
def _framework_install_patches():
    """Patches for calling _check_esphome_idf_framework_install directly with
    real stamp files (unlike espidf_mocks, which stubs the stamp layer)."""
    with (
        patch("esphome.espidf.framework.run_command_ok", return_value=True) as run_ok,
        patch("esphome.espidf.framework._get_idf_tool_paths", return_value=([], {})),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch("esphome.espidf.framework.rmdir"),
    ):
        yield run_ok


def _extracted_framework_with_stamp(stamp: dict) -> Path:
    framework_path = _get_framework_path(_IDF_VERSION)
    framework_path.mkdir(parents=True, exist_ok=True)
    (framework_path / ".esphome_extracted").touch()
    _write_stamp(framework_path / ESPHOME_STAMP_FILE, stamp)
    return framework_path


def test_framework_install_target_subset_skips_install() -> None:
    """A stamp holding a superset of the requested targets skips the installer."""
    framework_path = _extracted_framework_with_stamp(_requested_stamp(["all"]))

    with _framework_install_patches() as run_ok:
        _, fresh_extract = _check_esphome_idf_framework_install(
            _IDF_VERSION, ["esp32"], ["required"]
        )

    run_ok.assert_not_called()
    assert fresh_extract is False
    # the stamp is untouched
    stamp = json.loads((framework_path / ESPHOME_STAMP_FILE).read_text())
    assert stamp["targets"] == ["all"]


def test_framework_install_new_target_installs_and_merges_stamp() -> None:
    """A new target runs the installer for just that target and the stamp
    records the union of everything installed so far."""
    framework_path = _extracted_framework_with_stamp(_requested_stamp(["esp32"]))

    with _framework_install_patches() as run_ok:
        _, fresh_extract = _check_esphome_idf_framework_install(
            _IDF_VERSION, ["esp32c3"], ["required"]
        )

    assert fresh_extract is False
    assert "--targets=esp32c3" in run_ok.call_args[0][0]
    stamp = json.loads((framework_path / ESPHOME_STAMP_FILE).read_text())
    assert stamp["targets"] == ["esp32", "esp32c3"]


def test_check_esp_idf_install_env_targets_override_wins(
    espidf_mocks: SimpleNamespace,
) -> None:
    """An explicitly set ESPHOME_IDF_DEFAULT_TARGETS overrides per-variant targets."""
    with patch("esphome.espidf.framework._IDF_DEFAULT_TARGETS_EXPLICIT", True):
        check_esp_idf_install(_IDF_VERSION, force=True, targets=["esp32"])

    install_cmd = espidf_mocks.run_ok.call_args_list[0][0][0]
    assert "--targets=all" in install_cmd


def test_check_esp_idf_install_uses_requested_targets(
    espidf_mocks: SimpleNamespace,
) -> None:
    """Without the env override, the caller's per-variant targets are installed."""
    check_esp_idf_install(_IDF_VERSION, force=True, targets=["esp32"])

    install_cmd = espidf_mocks.run_ok.call_args_list[0][0][0]
    assert "--targets=esp32" in install_cmd


def test_framework_install_all_request_collapses_merged_stamp_to_all() -> None:
    """Requesting "all" over a per-variant stamp merges and collapses to
    ["all"], not ["all", "esp32"], so the stamp shape stays canonical."""
    framework_path = _extracted_framework_with_stamp(_requested_stamp(["esp32"]))

    with _framework_install_patches() as run_ok:
        _check_esphome_idf_framework_install(_IDF_VERSION, ["all"], ["required"])

    run_ok.assert_called_once()
    stamp = json.loads((framework_path / ESPHOME_STAMP_FILE).read_text())
    assert stamp["targets"] == ["all"]


def test_framework_install_tools_change_resets_stamp_targets() -> None:
    """A reinstall triggered by a tools change must not carry the old stamp's
    targets forward: the installer only ran for this build's targets, so a
    merged stamp would let other variants skip the reinstall they need."""
    framework_path = _extracted_framework_with_stamp(
        _requested_stamp(["all"], tools=["cmake", "required"])
    )

    with _framework_install_patches() as run_ok:
        _check_esphome_idf_framework_install(_IDF_VERSION, ["esp32"], ["required"])

    run_ok.assert_called_once()
    stamp = json.loads((framework_path / ESPHOME_STAMP_FILE).read_text())
    assert stamp["targets"] == ["esp32"]
    assert stamp["tools"] == ["required"]


@pytest.mark.parametrize(
    ("lib", "expect_hint"),
    [
        (None, True),
        ("libusb-1.0.so.0", False),
    ],
)
def test_check_esp_idf_install_failure_libusb_hint(
    espidf_mocks: SimpleNamespace,
    caplog: pytest.LogCaptureFixture,
    lib: str | None,
    expect_hint: bool,
) -> None:
    """A failed tools install only shows the libusb hint when libusb-1.0 is
    actually missing."""
    espidf_mocks.run_ok.return_value = False
    # Fake Linux so the gate is exercised on all CI hosts; faking Linux is safe
    # everywhere (unlike faking Windows, which pulls in winreg on other hosts)
    with (
        patch("esphome.espidf.framework.find_library", return_value=lib),
        patch("esphome.espidf.framework.platform.system", return_value="Linux"),
        caplog.at_level(logging.ERROR, logger="esphome.espidf.framework"),
        pytest.raises(RuntimeError, match="framework installation failure"),
    ):
        check_esp_idf_install(_IDF_VERSION, force=True)
    assert ("libusb-1.0.so.0 was not found" in caplog.text) == expect_hint


def test_check_esp_idf_install_unparseable_version(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A non-semver version skips the MAJOR/MINOR substitutions without erroring."""
    bad_version = "main"
    _get_framework_path(bad_version).mkdir(parents=True, exist_ok=True)
    check_esp_idf_install(bad_version, force=True)

    espidf_mocks.extract.assert_called_once()


@pytest.mark.parametrize(
    ("version", "short_version"),
    [
        ("6.0.0", "6.0"),
        ("6.0.0-rc1", "6.0-rc1"),
        ("5.5.4", None),  # vX.Y tags only exist for X.Y.0 releases
    ],
)
def test_check_esp_idf_install_short_version_substitution(
    espidf_mocks: SimpleNamespace, version: str, short_version: str | None
) -> None:
    """SHORT_VERSION is only offered for x.y.0 releases, so the vX.Y mirror
    template is never tried for versions whose tag cannot exist."""
    _get_framework_path(version).mkdir(parents=True, exist_ok=True)
    check_esp_idf_install(version, force=True)

    # First call downloads the framework archive; a later call fetches the
    # constraints file with its own substitutions.
    substitutions = espidf_mocks.download.call_args_list[0][0][1]
    assert substitutions.get("SHORT_VERSION") == short_version
    assert substitutions["VERSION"] == version


# ---------------------------------------------------------------------------
# _patch_tools_json_for_linux_arm64 (arm64-only ninja backport)
# ---------------------------------------------------------------------------


def _write_tools_json(framework_path: Path, data: dict) -> Path:
    tools_dir = framework_path / "tools"
    tools_dir.mkdir(parents=True, exist_ok=True)
    tools_json = tools_dir / "tools.json"
    tools_json.write_text(json.dumps(data), encoding="utf-8")
    return tools_json


def test_patch_tools_json_non_aarch64_is_noop(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path, {"tools": [{"name": "ninja", "versions": [{"name": "1.12.1"}]}]}
    )
    before = tools_json.read_text(encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="x86_64"):
        _patch_tools_json_for_linux_arm64(tmp_path)
    assert tools_json.read_text(encoding="utf-8") == before


def test_patch_tools_json_missing_file_is_noop(tmp_path: Path) -> None:
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)  # no tools/tools.json present


def test_patch_tools_json_corrupt_file_warns_and_skips(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    (tmp_path / "tools" / "tools.json").write_text("{ not json", encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)  # JSONDecodeError -> skip


def test_patch_tools_json_injects_ninja_arm64(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {"name": "ninja", "versions": [{"name": "1.12.1"}]},
                {"name": "cmake", "versions": [{"name": "3.24.0"}]},
            ]
        },
    )
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)

    data = json.loads(tools_json.read_text(encoding="utf-8"))
    ninja = next(t for t in data["tools"] if t["name"] == "ninja")
    assert "linux-arm64" in ninja["versions"][0]
    assert ninja["versions"][0]["linux-arm64"]["size"] == 121787


def test_patch_tools_json_already_patched_is_noop(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {
                    "name": "ninja",
                    "versions": [{"name": "1.12.1", "linux-arm64": {"url": "x"}}],
                }
            ]
        },
    )
    before = tools_json.read_text(encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)
    assert tools_json.read_text(encoding="utf-8") == before


# ---------------------------------------------------------------------------
# _prefetch_idf_tool_archives
# ---------------------------------------------------------------------------


_PREFETCH_JSON = json.dumps(
    [
        {
            "name": "cmake@3.30.2",
            "url": "https://example.com/cmake.tar.gz",
            "size": 123,
            "sha256": "ab" * 32,
            "dest": "cmake-3.30.2.tar.gz",
        },
        {
            "name": "ninja@1.12.1",
            "url": "https://example.com/ninja.zip",
            "size": 45,
            "sha256": "cd" * 32,
            "dest": "ninja.zip",
        },
    ]
)


def test_prefetch_leaves_unverifiable_entries_to_the_installer(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An entry missing sha256 or size must not download unverified; the
    installer handles it and fails loudly on a bad archive."""
    entries = json.loads(_PREFETCH_JSON)
    del entries[0]["sha256"]
    del entries[1]["size"]
    entries.append(
        {
            "name": "gcc@14.2.0",
            "url": "https://example.com/gcc.tar.gz",
            "size": 67,
            "sha256": "ef" * 32,
            "dest": "gcc.tar.gz",
        }
    )
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, json.dumps(entries), ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch("esphome.framework_helpers._BatchDownloadProgress") as progress_cls,
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)
    assert [call[0][0] for call in download.call_args_list] == [
        "https://example.com/gcc.tar.gz"
    ]
    assert download.call_args[1]["sha256"] == "ef" * 32
    progress_cls.assert_called_once_with("Downloading ESP-IDF tools", 67)
    assert "cmake@3.30.2 has no sha256/size" in caplog.text
    assert "ninja@1.12.1 has no sha256/size" in caplog.text


def test_prefetch_all_entries_unverifiable_is_a_noop(tmp_path: Path) -> None:
    entries = json.loads(_PREFETCH_JSON)
    for entry in entries:
        del entry["sha256"]
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, json.dumps(entries), ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)
    download.assert_not_called()


def test_prefetch_dedupes_entries_by_dest(tmp_path: Path) -> None:
    """Two entries resolving to one dest would interleave writes into the
    same .part file; only the first downloads."""
    entries = json.loads(_PREFETCH_JSON)
    dup = dict(entries[0]) | {"name": "cmake-alias@3.30.2"}
    entries.append(dup)
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, json.dumps(entries), ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch("esphome.framework_helpers._BatchDownloadProgress"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)
    dests = [call[0][1].name for call in download.call_args_list]
    assert dests.count("cmake-3.30.2.tar.gz") == 1


def test_prefetch_downloads_each_archive_with_resume(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, _PREFETCH_JSON, ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch("esphome.framework_helpers._BatchDownloadProgress") as progress_cls,
    ):
        # Materialize the lazy mock before threads race its first creation
        tracker = progress_cls.return_value.tracker.return_value
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    dist = get_idf_tools_path() / "dist"
    # Archives download concurrently, so the call order is not fixed.
    calls = {call[0]: call[1] for call in download.call_args_list}
    assert set(calls) == {
        ("https://example.com/cmake.tar.gz", dist / "cmake-3.30.2.tar.gz"),
        ("https://example.com/ninja.zip", dist / "ninja.zip"),
    }
    kwargs = calls[("https://example.com/cmake.tar.gz", dist / "cmake-3.30.2.tar.gz")]
    assert kwargs["sha256"] == "ab" * 32
    assert kwargs["size"] == 123
    # every archive reports into the one combined progress bar via the
    # cancellation-checked wrapper; verify it delegates to the tracker
    progress_cls.assert_called_once_with("Downloading ESP-IDF tools", 123 + 45)
    before = tracker.call_count
    for kw in calls.values():
        kw["progress"](7)
    assert tracker.call_count == before + len(calls)


def test_prefetch_downloads_archives_concurrently(tmp_path: Path) -> None:
    """More than one archive fans out over a bounded thread pool."""
    entries = [
        {
            "name": f"tool{i}@1",
            "url": f"https://example.com/tool{i}.tar.gz",
            "size": 10,
            "sha256": "ab" * 32,
            "dest": f"tool{i}.tar.gz",
        }
        for i in range(6)
    ]
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, json.dumps(entries), ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch(
            "esphome.framework_helpers.ThreadPoolExecutor", wraps=ThreadPoolExecutor
        ) as pool,
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    pool.assert_called_once_with(max_workers=4)
    assert download.call_count == 6


def test_prefetch_skips_already_downloaded_archives(tmp_path: Path) -> None:
    dist = get_idf_tools_path() / "dist"
    dist.mkdir(parents=True)
    (dist / "cmake-3.30.2.tar.gz").write_bytes(b"cached")
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, _PREFETCH_JSON, ""),
        ),
        patch("esphome.framework_helpers.download_with_resume") as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    # only the missing archive is downloaded
    assert download.call_count == 1
    assert download.call_args[0][1] == dist / "ninja.zip"


@pytest.mark.parametrize(
    ("run_result", "download_error", "expected_log"),
    [
        ((False, "", "script exploded"), None, "tool downloads"),  # script failure
        ((True, "{ not json", ""), None, "prefetch failed"),  # unparsable output
        (
            (True, _PREFETCH_JSON, ""),
            OSError("network down"),
            "Could not prefetch",
        ),  # download failure
    ],
)
def test_prefetch_failures_never_raise(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    run_result: tuple[bool, str, str],
    download_error: Exception | None,
    expected_log: str,
) -> None:
    """The prefetch is best-effort; idf_tools downloads whatever is missing."""
    with (
        patch("esphome.espidf.framework.run_command", return_value=run_result),
        patch(
            "esphome.framework_helpers.download_with_resume",
            side_effect=download_error,
        ),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    assert expected_log in caplog.text


def test_prefetch_total_failure_logs_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Every archive failing is a systematic fault (proxy, bad kwarg), not
    a flaky mirror; it must be distinguishable at ERROR because the resume
    workaround is off for the whole install."""
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, _PREFETCH_JSON, ""),
        ),
        patch(
            "esphome.framework_helpers.download_with_resume",
            side_effect=OSError("proxy refuses everything"),
        ),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)
    assert "Every ESP-IDF tool prefetch failed" in caplog.text


def test_prefetch_one_failed_archive_does_not_stop_the_rest(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A single archive failing its download must not abort the prefetch of
    the remaining archives."""

    def _fail_cmake_download(url: str, *args, **kwargs) -> None:
        if "cmake" in url:
            raise OSError("network down")

    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, _PREFETCH_JSON, ""),
        ),
        patch(
            "esphome.framework_helpers.download_with_resume",
            side_effect=_fail_cmake_download,
        ) as download,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    assert download.call_count == 2
    assert "Could not prefetch cmake@3.30.2" in caplog.text
    # One flaky archive is routine, never the systematic-fault ERROR
    assert "Every ESP-IDF tool prefetch failed" not in caplog.text


def test_prefetch_finishes_progress_bar_and_cancels_queue(tmp_path: Path) -> None:
    """The batch bar is closed out after the pool, and the pool is shut down
    with cancel_futures so Ctrl-C does not drain every queued archive."""
    with (
        patch(
            "esphome.espidf.framework.run_command",
            return_value=(True, _PREFETCH_JSON, ""),
        ),
        patch("esphome.framework_helpers.download_with_resume"),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
        patch("esphome.framework_helpers._BatchDownloadProgress") as progress_cls,
        patch("esphome.framework_helpers.ThreadPoolExecutor") as pool_cls,
    ):
        pool = MagicMock(wraps=ThreadPoolExecutor(max_workers=2))
        pool_cls.return_value = pool
        _prefetch_idf_tool_archives(tmp_path, "esp32", ["required"], None)

    pool.shutdown.assert_called_once_with(wait=True, cancel_futures=True)
    progress_cls.return_value.done.assert_called_once_with()


def test_prefetch_passes_targets_and_tools_to_script(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.run_command", return_value=(True, "[]", "")
        ) as run,
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        _prefetch_idf_tool_archives(
            tmp_path, "esp32,esp32c3", ["required", "cmake"], {"IDF_TOOLS_PATH": "/x"}
        )

    cmd = run.call_args[0][0]
    assert cmd[-3:] == ["esp32,esp32c3", "required", "cmake"]
    assert cmd[1].endswith("get_tool_downloads.py")
    # the script inherits the caller's env plus the framework tools PYTHONPATH
    env = run.call_args[1]["env"]
    assert env["IDF_TOOLS_PATH"] == "/x"
    assert env["PYTHONPATH"] == str(tmp_path / "tools")


def test_framework_install_prefetches_before_installer(
    espidf_mocks: SimpleNamespace,
) -> None:
    """The prefetch runs before idf_tools.py install so the installer finds
    the archives already in dist/."""
    calls: list[str] = []
    with (
        patch(
            "esphome.espidf.framework._prefetch_idf_tool_archives",
            side_effect=lambda *a, **k: calls.append("prefetch"),
        ),
    ):
        espidf_mocks.run_ok.side_effect = lambda *a, **k: (
            calls.append("install") or True
        )
        check_esp_idf_install(_IDF_VERSION, force=True)

    assert calls.index("prefetch") < calls.index("install")


# ---------------------------------------------------------------------------
# get_tool_downloads.py (against the stub idf_tools module in fixtures/)
# ---------------------------------------------------------------------------


_IDF_TOOLS_STUB_DIR = Path(__file__).parent / "fixtures" / "idf_tools_stub"


def _run_downloads_script(
    tmp_path: Path, *args: str, env_extra: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    """Run the real get_tool_downloads.py against the stub idf_tools module."""
    script = Path(__file__).parents[2] / "esphome" / "espidf" / "get_tool_downloads.py"
    env = os.environ | {
        "PYTHONPATH": str(_IDF_TOOLS_STUB_DIR),
        "IDF_TOOLS_PATH": str(tmp_path / "tp"),
    }
    if env_extra:
        env |= env_extra
    return subprocess.run(
        [sys.executable, str(script), str(tmp_path / "fw"), *args],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )


def test_get_tool_downloads_lists_missing_tools(tmp_path: Path) -> None:
    """Installed versions are skipped, tools that fail their binary check are
    still listed, rename_dist decides the dist filename, and idf_tools' stdout
    chatter stays off the JSON channel."""
    result = _run_downloads_script(tmp_path, "esp32", "required")

    assert result.returncode == 0, result.stderr
    downloads = {d["name"]: d for d in json.loads(result.stdout)}
    # installed-tool@1.0 is already installed and must not be listed
    assert set(downloads) == {"cmake@3.30.2", "ninja@1.12.1", "broken-tool@2.0"}
    assert downloads["cmake@3.30.2"]["dest"] == "cmake.tar.gz"
    assert downloads["cmake@3.30.2"]["size"] == 11
    assert downloads["cmake@3.30.2"]["sha256"] == "aa"
    # rename_dist overrides the URL basename
    assert downloads["ninja@1.12.1"]["dest"] == "ninja-v1.zip"
    # the stub prints informational lines; they must be on stderr
    assert "Changed download URL" in result.stderr


def test_get_tool_downloads_applies_mirror_rewrite(tmp_path: Path) -> None:
    result = _run_downloads_script(
        tmp_path,
        "esp32",
        "required",
        env_extra={"TEST_MIRROR_PREFIX": "https://mirror.test/"},
    )

    assert result.returncode == 0, result.stderr
    downloads = json.loads(result.stdout)
    assert all(d["url"].startswith("https://mirror.test/") for d in downloads)


def _run_downloads_inprocess(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
    *args: str,
) -> list[dict]:
    """Execute get_tool_downloads.py in-process against the stub idf_tools.

    Unlike the subprocess variant this runs under coverage, exercising the
    script's own lines.
    """
    spec = importlib.util.spec_from_file_location(
        "idf_tools", _IDF_TOOLS_STUB_DIR / "idf_tools.py"
    )
    stub = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(stub)
    monkeypatch.setitem(sys.modules, "idf_tools", stub)
    monkeypatch.setenv("IDF_TOOLS_PATH", str(tmp_path / "tp"))
    script = Path(__file__).parents[2] / "esphome" / "espidf" / "get_tool_downloads.py"
    monkeypatch.setattr(sys, "argv", [str(script), str(tmp_path / "fw"), *args])
    runpy.run_path(str(script))
    return json.loads(capsys.readouterr().out)


def test_get_tool_downloads_inprocess_full_flow(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """In-process run covering the whole script: required expansion,
    installed/broken tools, rename_dist, and version pinning via tool@version."""
    downloads = {
        d["name"]: d
        for d in _run_downloads_inprocess(
            tmp_path, monkeypatch, capsys, "esp32", "required"
        )
    }
    assert set(downloads) == {"cmake@3.30.2", "ninja@1.12.1", "broken-tool@2.0"}
    assert downloads["ninja@1.12.1"]["dest"] == "ninja-v1.zip"
    assert downloads["cmake@3.30.2"]["url"] == "https://gh.test/cmake.tar.gz"


def test_get_tool_downloads_inprocess_explicit_tool_specs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Explicit tool names and tool@version specs resolve; unknown tools and
    unknown versions are skipped."""
    downloads = _run_downloads_inprocess(
        tmp_path,
        monkeypatch,
        capsys,
        "esp32",
        "cmake@3.30.2",
        "no-such-tool",
        "cmake@9.9.9",
    )
    assert [d["name"] for d in downloads] == ["cmake@3.30.2"]


# ---------------------------------------------------------------------------
# _patch_tools_json_demote_unused_tools (openocd, gdb, ULP toolchain optional)
# ---------------------------------------------------------------------------


def test_demote_unused_tools_patches_install_type(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {"name": "openocd-esp32", "install": "always"},
                {"name": "xtensa-esp-elf-gdb", "install": "always"},
                {"name": "riscv32-esp-elf-gdb", "install": "always"},
                {"name": "esp32ulp-elf", "install": "always"},
                {"name": "xtensa-esp-elf", "install": "always"},
                {"name": "esp-rom-elfs", "install": "always"},
            ]
        },
    )
    _patch_tools_json_demote_unused_tools(tmp_path)

    data = json.loads(tools_json.read_text(encoding="utf-8"))
    install_types = {t["name"]: t["install"] for t in data["tools"]}
    assert install_types == {
        "openocd-esp32": "on_request",
        "xtensa-esp-elf-gdb": "on_request",
        "riscv32-esp-elf-gdb": "on_request",
        "esp32ulp-elf": "on_request",
        # the compiler toolchain and ROM ELFs stay required
        "xtensa-esp-elf": "always",
        "esp-rom-elfs": "always",
    }


def test_demote_unused_tools_drops_xtensa_from_riscv_targets(tmp_path: Path) -> None:
    """riscv32-esp-elf loses the xtensa chips (ULP-RISC-V only, which ESPHome
    never builds) but keeps its RISC-V targets; other tools are untouched."""
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {
                    "name": "riscv32-esp-elf",
                    "install": "always",
                    "supported_targets": ["esp32s2", "esp32s3", "esp32c3", "esp32p4"],
                },
                {
                    "name": "xtensa-esp-elf",
                    "install": "always",
                    "supported_targets": ["esp32", "esp32s2", "esp32s3"],
                },
            ]
        },
    )
    _patch_tools_json_demote_unused_tools(tmp_path)

    data = json.loads(tools_json.read_text(encoding="utf-8"))
    riscv = next(t for t in data["tools"] if t["name"] == "riscv32-esp-elf")
    xtensa = next(t for t in data["tools"] if t["name"] == "xtensa-esp-elf")
    assert riscv["supported_targets"] == ["esp32c3", "esp32p4"]
    assert riscv["install"] == "always"
    assert xtensa["supported_targets"] == ["esp32", "esp32s2", "esp32s3"]


def test_demote_unused_tools_bad_supported_targets_type_still_demotes(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A non-list supported_targets on riscv32-esp-elf must not abort the
    other demotions; the targets patch is best-effort and logs the skip so a
    silently resumed riscv download is diagnosable."""
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {
                    "name": "riscv32-esp-elf",
                    "install": "always",
                    "supported_targets": None,
                },
                {"name": "openocd-esp32", "install": "always"},
            ]
        },
    )
    with caplog.at_level(logging.WARNING, logger="esphome.espidf.framework"):
        _patch_tools_json_demote_unused_tools(tmp_path)

    data = json.loads(tools_json.read_text(encoding="utf-8"))
    openocd = next(t for t in data["tools"] if t["name"] == "openocd-esp32")
    riscv = next(t for t in data["tools"] if t["name"] == "riscv32-esp-elf")
    assert openocd["install"] == "on_request"
    assert riscv["supported_targets"] is None
    assert "Unexpected supported_targets" in caplog.text


def test_patch_tools_json_unexpected_structure_warns_and_skips(
    tmp_path: Path,
) -> None:
    """Valid JSON with an unexpected shape must skip the patch, not raise."""
    tools_dir = tmp_path / "tools"
    tools_dir.mkdir()
    tools_json = tools_dir / "tools.json"
    tools_json.write_text('["not", "a", "dict"]', encoding="utf-8")
    before = tools_json.read_text(encoding="utf-8")
    _patch_tools_json_demote_unused_tools(tmp_path)  # AttributeError -> skip
    assert tools_json.read_text(encoding="utf-8") == before


def test_demote_unused_tools_already_patched_is_noop(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {"name": "openocd-esp32", "install": "on_request"},
                {"name": "xtensa-esp-elf-gdb", "install": "on_request"},
                {"name": "riscv32-esp-elf-gdb", "install": "on_request"},
                {"name": "esp32ulp-elf", "install": "on_request"},
                {
                    "name": "riscv32-esp-elf",
                    "install": "always",
                    "supported_targets": ["esp32c3", "esp32p4"],
                },
            ]
        },
    )
    before = tools_json.read_text(encoding="utf-8")
    _patch_tools_json_demote_unused_tools(tmp_path)
    assert tools_json.read_text(encoding="utf-8") == before


# ---------------------------------------------------------------------------
# Subprocess-backed helpers (_exec -> run_command rename) and get_framework_env
# ---------------------------------------------------------------------------


def test_get_idf_version_parses_stdout(tmp_path: Path) -> None:
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, "5.1.2\n", "")
    ):
        assert _get_idf_version(tmp_path) == "5.1.2"


def test_get_idf_version_raises_on_failure(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "boom")),
        pytest.raises(RuntimeError, match="Can't get ESP-IDF version"),
    ):
        _get_idf_version(tmp_path)


def test_get_idf_tool_paths_parses_json(tmp_path: Path) -> None:
    payload = json.dumps({"paths_to_export": ["/a", "/b"], "export_vars": {"X": "1"}})
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, payload, "")
    ):
        paths, export_vars = _get_idf_tool_paths(tmp_path)
    assert paths == ["/a", "/b"]
    assert export_vars == {"X": "1"}


def test_get_idf_tool_paths_raises_on_bad_json(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.run_command", return_value=(True, "not json", "")
        ),
        pytest.raises(RuntimeError, match="Can't extract ESP-IDF tool paths"),
    ):
        _get_idf_tool_paths(tmp_path)


def test_get_idf_tool_paths_raises_on_failure(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "err")),
        pytest.raises(RuntimeError, match="Can't get ESP-IDF tool paths"),
    ):
        _get_idf_tool_paths(tmp_path)


def test_get_python_version_parses_stdout(tmp_path: Path) -> None:
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, "3.11.0\n", "")
    ):
        assert _get_python_version(tmp_path / "python") == "3.11.0"


def test_get_python_version_returns_falsy_on_failure(tmp_path: Path) -> None:
    with patch("esphome.espidf.framework.run_command", return_value=(False, "", "")):
        # non-throwing failure returns the (empty) stdout as-is
        assert not _get_python_version(tmp_path / "python")


def test_get_python_version_raises_when_requested(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "")),
        pytest.raises(RuntimeError, match="Can't get Python version"),
    ):
        _get_python_version(tmp_path / "python", throw_exception=True)


def test_write_stamp_writes_json(tmp_path: Path) -> None:
    stamp = tmp_path / "stamp.json"
    _write_stamp(stamp, {"a": "1", "b": "2"})
    assert json.loads(stamp.read_text(encoding="utf-8")) == {"a": "1", "b": "2"}


def test_get_framework_env_with_python_env(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        patch("esphome.espidf.framework._get_idf_version", return_value="5.1.2"),
        patch(
            "esphome.espidf.framework._get_idf_tool_paths",
            return_value=(["/tool/bin"], {"IDF_X": "1"}),
        ),
        # ccache env is covered separately; keep this test host-independent.
        patch("esphome.espidf.framework._ccache_env", return_value={}),
    ):
        env = get_framework_env(
            tmp_path / "fw", tmp_path / "penv", {"PATH": "/usr/bin"}
        )

    assert env["IDF_PATH"] == str(tmp_path / "fw")
    assert env["ESP_IDF_VERSION"] == "5.1.2"
    assert env["IDF_X"] == "1"
    assert env["IDF_PYTHON_ENV_PATH"] == str(tmp_path / "penv")
    assert "/tool/bin" in env["PATH"]


def test_get_framework_env_without_python_env_uses_os_path(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        patch("esphome.espidf.framework._get_idf_version", return_value="5.1.2"),
        patch("esphome.espidf.framework._get_idf_tool_paths", return_value=([], {})),
        # ccache env is covered separately; keep this test host-independent.
        patch("esphome.espidf.framework._ccache_env", return_value={}),
    ):
        env = get_framework_env(tmp_path / "fw")

    assert "IDF_PYTHON_ENV_PATH" not in env
    assert env["PATH"]  # taken from os.environ


# ---------------------------------------------------------------------------
# _ccache_env
# ---------------------------------------------------------------------------


def _ccache_patches(tmp_path: Path, which: str | None, build_path: Path | None):
    return (
        patch("esphome.espidf.framework.resolve_ccache_path", return_value=which),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        # ccache_defaults_env (build_helpers.ccache) reads CORE at call time
        patch(
            "esphome.core.CORE",
            SimpleNamespace(build_path=build_path),
        ),
    )


def test_ccache_env_default_enabled_when_available(tmp_path: Path) -> None:
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    with patch.dict("os.environ", {}, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert env["CCACHE_DIR"] == str(tmp_path / "tools" / "ccache")
    assert env["CCACHE_NOHASHDIR"] == "true"
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())


def test_ccache_env_disabled_when_binary_missing(tmp_path: Path) -> None:
    # build_path is None here too: a disabled cache must not require it.
    p1, p2, p3 = _ccache_patches(tmp_path, None, None)
    with patch.dict("os.environ", {}, clear=True), p1, p2, p3:
        # Canonical off, so an inherited/unparsable value cannot enable it
        assert _ccache_env() == {"IDF_CCACHE_ENABLE": "0"}


def test_ccache_env_opt_out_via_env(tmp_path: Path) -> None:
    # Explicit IDF_CCACHE_ENABLE=0 wins even when the binary is present, and
    # short-circuits before build_path is needed.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", None)
    with patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "0"}, clear=True), p1, p2, p3:
        # The canonical off spelling is exported: the raw value is inherited
        # by idf.py, where a spelling like "disable" would read as truthy
        assert _ccache_env() == {"IDF_CCACHE_ENABLE": "0"}


def test_ccache_env_opt_in_without_binary(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # Explicit IDF_CCACHE_ENABLE=1 forces it on; without a usable binary
    # idf.py silently skips ccache, so this branch must say so out loud.
    p1, p2, p3 = _ccache_patches(tmp_path, None, tmp_path / "build")
    with patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "1"}, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert env["CCACHE_DIR"] == str(tmp_path / "tools" / "ccache")
    assert env["CCACHE_DEPEND"] == "1"
    assert "no ccache binary is on PATH" in caplog.text


def test_ccache_env_opt_in_with_working_binary(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # Forced on with a working binary: no warning fires at all.
    ccache = tmp_path / "ccache"
    ccache.touch()
    p1, p2, p3 = _ccache_patches(tmp_path, str(ccache), tmp_path / "build")
    with (
        patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "1"}, clear=True),
        patch("esphome.espidf.framework.shutil.which", return_value=str(ccache)),
        patch("esphome.espidf.framework.tool_version_runs", return_value=True),
        p1,
        p2,
        p3,
    ):
        env = _ccache_env()
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert not [r for r in caplog.records if r.levelno >= logging.WARNING]


def test_ccache_env_opt_in_with_rejected_binary(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # Forced on with a present-but-rejected binary: idf.py does its own
    # PATH lookup and uses it anyway; the warning must say so, not claim
    # the build runs without ccache.
    # A present but non-executable file: the real probe fails and logs
    # the forced-on message (patching the probe would silence it)
    broken = tmp_path / "broken-ccache"
    broken.touch()
    p1, p2, p3 = _ccache_patches(tmp_path, None, tmp_path / "build")
    with (
        patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "1"}, clear=True),
        patch("esphome.espidf.framework.shutil.which", return_value=str(broken)),
        p1,
        p2,
        p3,
    ):
        env = _ccache_env()
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert "idf.py will use it anyway" in caplog.text
    # Exactly one story: the resolver's contradictory "compiling without
    # ccache" must not precede it
    assert "compiling without ccache" not in caplog.text


def test_ccache_env_honors_shared_esphome_opt_out(tmp_path: Path) -> None:
    """ESPHOME_CCACHE_ENABLE=0 disables ccache here too; the shared policy
    must not apply to every backend except this one."""
    _p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    env_vars = {"ESPHOME_CCACHE_ENABLE": "0", "PATH": "/usr/bin"}
    with patch.dict("os.environ", env_vars, clear=True), p2, p3:
        # The real resolver runs so the opt-out parse is exercised
        assert _ccache_env() == {"IDF_CCACHE_ENABLE": "0"}


@pytest.mark.parametrize("value", ["off", "no"])
def test_ccache_env_idf_knob_parses_strictly(tmp_path: Path, value: str) -> None:
    """IDF_CCACHE_ENABLE uses the same strict table as the shared knob, so
    "off" disables instead of reading as truthy."""
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    with patch.dict("os.environ", {"IDF_CCACHE_ENABLE": value}, clear=True), p1, p2, p3:
        assert _ccache_env() == {"IDF_CCACHE_ENABLE": "0"}


def test_ccache_env_idf_knob_unrecognized_warns_and_defers(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unparsable IDF_CCACHE_ENABLE warns, defers to the shared resolver,
    and is not forwarded to idf.py as truthy."""
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    env_vars = {"IDF_CCACHE_ENABLE": "enabled"}
    with patch.dict("os.environ", env_vars, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert "unrecognized IDF_CCACHE_ENABLE" in caplog.text
    assert env["IDF_CCACHE_ENABLE"] == "1"


def test_ccache_env_idf_knob_wins_over_shared_opt_out(tmp_path: Path) -> None:
    """IDF_CCACHE_ENABLE=1 takes precedence over ESPHOME_CCACHE_ENABLE=0."""
    p1, p2, p3 = _ccache_patches(tmp_path, None, tmp_path / "build")
    env_vars = {"IDF_CCACHE_ENABLE": "1", "ESPHOME_CCACHE_ENABLE": "0"}
    with patch.dict("os.environ", env_vars, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert env["CCACHE_DIR"] == str(tmp_path / "tools" / "ccache")
    assert env["IDF_CCACHE_ENABLE"] == "1"


def test_ccache_env_preserves_user_overrides(tmp_path: Path) -> None:
    # User-set CCACHE_* values must not be clobbered; unset ones still default.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    user_env = {"CCACHE_DIR": "/my/cache", "CCACHE_MAXSIZE": "9G"}
    with patch.dict("os.environ", user_env, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert "CCACHE_DIR" not in env
    assert "CCACHE_MAXSIZE" not in env
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert env["CCACHE_DEPEND"] == "1"


def test_ccache_env_raises_without_build_path(tmp_path: Path) -> None:
    # Enabled but no build_path means the IDF env was built too early -- fail
    # loudly instead of silently dropping CCACHE_BASEDIR.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", None)
    with (
        patch.dict("os.environ", {}, clear=True),
        p1,
        p2,
        p3,
        pytest.raises(ValueError, match="build_path"),
    ):
        _ccache_env()


# ---------------------------------------------------------------------------
# _check_stamp / _write_idf_version_txt / get_idf_tools_path
# ---------------------------------------------------------------------------


def test_check_stamp_matches(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text(json.dumps({"a": "1"}), encoding="utf-8")
    assert _check_stamp(f, {"a": "1"}) is True


def test_check_stamp_mismatch(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text(json.dumps({"a": "1"}), encoding="utf-8")
    assert _check_stamp(f, {"a": "2"}) is False


def test_check_stamp_missing_file(tmp_path: Path) -> None:
    assert _check_stamp(tmp_path / "nope.json", {"a": "1"}) is False


def test_check_stamp_corrupt_file(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text("{ not json", encoding="utf-8")
    assert _check_stamp(f, {"a": "1"}) is False


def test_read_stamp_corrupt_file_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # A corrupt stamp forces a full reinstall on every build, so it warns
    # where the normal missing-file case stays silent.
    f = tmp_path / "s.json"
    f.write_text("{ not json", encoding="utf-8")
    with caplog.at_level(logging.WARNING, logger="esphome.espidf.framework"):
        assert _read_stamp(f) is None
    assert "Ignoring corrupt stamp file" in caplog.text


def test_read_stamp_unreadable_file_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # An I/O fault (permissions, disk error) is distinguished from a simply
    # missing stamp with a warning before falling back to reinstall.
    f = tmp_path / "s.json"
    f.write_text(json.dumps({"a": "1"}), encoding="utf-8")
    with (
        patch.object(Path, "open", side_effect=PermissionError("denied")),
        caplog.at_level(logging.WARNING, logger="esphome.espidf.framework"),
    ):
        assert _read_stamp(f) is None
    assert "Could not read stamp file" in caplog.text


def test_read_stamp_non_dict_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # Well-formed JSON that is not an object is a fault, not a first install;
    # it must leave a trace before forcing reinstalls.
    f = tmp_path / "s.json"
    f.write_text("null", encoding="utf-8")
    with caplog.at_level(logging.WARNING, logger="esphome.espidf.framework"):
        assert _read_stamp(f) is None
    assert "unexpected type NoneType" in caplog.text


def test_read_stamp_missing_file_is_silent(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # Missing stamps are the normal first-install case and must not log.
    with caplog.at_level(logging.DEBUG, logger="esphome.espidf.framework"):
        assert _read_stamp(tmp_path / "nope.json") is None
    assert "stamp file" not in caplog.text


def test_write_idf_version_txt_writes_when_missing(tmp_path: Path) -> None:
    _write_idf_version_txt(tmp_path, "5.1.2")
    assert (tmp_path / "version.txt").read_text(encoding="utf-8") == "v5.1.2\n"


def test_write_idf_version_txt_skips_when_present(tmp_path: Path) -> None:
    (tmp_path / "version.txt").write_text("existing\n", encoding="utf-8")
    _write_idf_version_txt(tmp_path, "5.1.2")
    assert (tmp_path / "version.txt").read_text(encoding="utf-8") == "existing\n"


def testget_idf_tools_path_env_override(tmp_path: Path) -> None:
    override = str(tmp_path / "custom-idf")
    with patch.dict("os.environ", {"ESPHOME_ESP_IDF_PREFIX": override}):
        assert get_idf_tools_path() == Path(override)


@pytest.mark.parametrize("value", ["", "   "])
def testget_idf_tools_path_blank_env_falls_back_to_default(
    value: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A blank ESPHOME_ESP_IDF_PREFIX is treated as unset, not as CWD.

    Path("") would resolve to the working directory, which clean-all could then
    delete by accident.
    """
    import platformdirs

    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", value)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "idf"
    ).resolve()
    assert get_idf_tools_path() == expected


def testget_idf_tools_path_default_uses_user_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Without the env override the install root is the machine-global OS user
    cache dir, not the per-config ``<data_dir>/idf``."""
    import platformdirs

    monkeypatch.delenv("ESPHOME_ESP_IDF_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "idf"
    ).resolve()
    assert get_idf_tools_path() == expected


def test_write_idf_version_txt_warns_on_write_error(tmp_path: Path) -> None:
    with patch("pathlib.Path.write_text", side_effect=OSError("denied")):
        # write failure is caught and warned, not raised
        _write_idf_version_txt(tmp_path, "5.1.2")


def _fake_winreg(
    query_result: int | None = None, query_error: OSError | None = None
) -> SimpleNamespace:
    """Build a minimal winreg stand-in (the real module is Windows-only)."""

    @contextmanager
    def open_key(root, path):
        yield "hkey"

    def query_value_ex(key, name):
        if query_error is not None:
            raise query_error
        return query_result, 4  # (value, REG_DWORD)

    return SimpleNamespace(
        HKEY_LOCAL_MACHINE=object(),
        OpenKey=open_key,
        QueryValueEx=query_value_ex,
    )


@pytest.mark.parametrize(("reg_value", "expected"), [(1, True), (0, False)])
def test_windows_long_paths_enabled_reads_registry(
    reg_value: int, expected: bool
) -> None:
    with patch.dict(sys.modules, {"winreg": _fake_winreg(query_result=reg_value)}):
        assert _windows_long_paths_enabled() is expected


def test_windows_long_paths_enabled_missing_value() -> None:
    """A missing registry value (FileNotFoundError is an OSError) reads as disabled."""
    fake = _fake_winreg(query_error=FileNotFoundError("no such value"))
    with patch.dict(sys.modules, {"winreg": fake}):
        assert _windows_long_paths_enabled() is False


# 8 chars -> projected well under the 260 limit even with the ~245-char reserve
_SHORT_IDF_PATH = "C:\\e\\idf"
# 25 chars -> projected over the limit
_LONG_IDF_PATH = "C:\\Users\\bob\\.esphome\\idf"


def test_check_windows_path_length_noop_off_windows(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Off Windows the check returns before touching the registry or the path."""
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Linux"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled"
        ) as long_paths_mock,
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    long_paths_mock.assert_not_called()
    assert not caplog.records


def test_check_windows_path_length_noop_when_long_paths_enabled(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=True
        ),
        patch("esphome.espidf.framework.get_idf_tools_path") as get_path_mock,
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    get_path_mock.assert_not_called()
    assert not caplog.records


def test_check_windows_path_length_short_path_silent(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=False
        ),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=_SHORT_IDF_PATH,
        ),
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    assert not caplog.records


def test_check_windows_path_length_long_path_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=False
        ),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=_LONG_IDF_PATH,
        ),
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    assert len(caplog.records) == 1
    message = caplog.records[0].getMessage()
    assert _LONG_IDF_PATH in message
    assert "long path support" in message
    # The install is global now; the remedy is the prefix env, not moving the project.
    assert "ESPHOME_ESP_IDF_PREFIX" in message
