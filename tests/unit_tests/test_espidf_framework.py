"""Tests for esphome.espidf.framework helpers."""

# pylint: disable=protected-access

import io
from pathlib import Path
import tarfile
from unittest.mock import patch

import pytest

from esphome.espidf.framework import (
    _clone_idf_with_submodules,
    _get_framework_path,
    _get_python_env_path,
    _parse_git_source,
    check_esp_idf_install,
)
from esphome.framework_helpers import _tar_extract_all


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


def _make_idf_tree(framework_path: Path) -> None:
    """Create the minimum tree _clone_idf_with_submodules sanity-checks for."""
    (framework_path / "tools").mkdir(parents=True)
    (framework_path / "tools" / "idf_tools.py").write_text("# stub\n")


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


def test_check_esp_idf_install_fresh(setup_core: Path) -> None:
    """A forced install drives the download/extract, venv, and pip-install path."""
    version = "5.1.2"
    # archive_extract_all is mocked, so pre-create the framework dir that the
    # extracted-marker touch writes into.
    _get_framework_path(version).mkdir(parents=True, exist_ok=True)

    with (
        patch("esphome.espidf.framework.rmdir"),
        patch(
            "esphome.espidf.framework.download_from_mirrors",
            return_value="https://example.com/idf.tar.xz",
        ) as mock_download,
        patch("esphome.espidf.framework.archive_extract_all") as mock_extract,
        patch("esphome.espidf.framework.create_venv") as mock_venv,
        patch(
            "esphome.espidf.framework.run_command_ok", return_value=True
        ) as mock_run_ok,
        patch("esphome.espidf.framework._write_idf_version_txt"),
        patch("esphome.espidf.framework._patch_tools_json_for_linux_arm64"),
        patch("esphome.espidf.framework._write_stamp"),
        patch("esphome.espidf.framework._get_idf_version", return_value=version),
        patch("esphome.espidf.framework._get_python_version", return_value="3.11.0"),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        framework_path, python_env_path = check_esp_idf_install(version, force=True)

    assert framework_path == _get_framework_path(version)
    assert python_env_path == _get_python_env_path(version)
    # framework tarball + python-env constraints file are both downloaded
    assert mock_download.call_count == 2
    mock_extract.assert_called_once()
    mock_venv.assert_called_once()
    assert mock_run_ok.called
