"""Tests for esphome.framework_helpers."""

# pylint: disable=protected-access

import importlib.util
import io
import logging
import os
from pathlib import Path
import subprocess
import sys
import tarfile
from unittest.mock import MagicMock, Mock, patch
import zipfile

import pytest
import requests as req

from esphome.framework_helpers import (
    _7z_extract_all,
    _detect_archive_root,
    _rename_with_retry,
    _tar_extract_all,
    _zip_extract_all,
    archive_extract_all,
    create_venv,
    download_from_mirrors,
    get_project_compile_flags,
    get_project_link_flags,
    get_python_env_executable_path,
    get_system_python_path,
    rmdir,
    run_command,
    run_command_ok,
    str_to_lst_of_str,
)

_HAS_PY7ZR = importlib.util.find_spec("py7zr") is not None

# ---------------------------------------------------------------------------
# str_to_lst_of_str
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("a;b;c", ["a", "b", "c"]),
        ("  a ; b ", ["a", "b"]),
        (";; a ;;", ["a"]),
        ("single", ["single"]),
        ("", []),
        (["already", "a", "list"], ["already", "a", "list"]),
    ],
)
def test_str_to_lst_of_str(value: str | list, expected: list) -> None:
    assert str_to_lst_of_str(value) == expected


# ---------------------------------------------------------------------------
# rmdir
# ---------------------------------------------------------------------------


def test_rmdir_nonexistent_is_noop(tmp_path: Path) -> None:
    rmdir(tmp_path / "missing")


def test_rmdir_removes_existing_directory(tmp_path: Path) -> None:
    d = tmp_path / "to_remove"
    d.mkdir()
    (d / "file.txt").write_text("x")
    rmdir(d)
    assert not d.exists()


def test_rmdir_logs_debug_with_msg(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    d = tmp_path / "logged"
    d.mkdir()
    with caplog.at_level(logging.DEBUG, logger="esphome.framework_helpers"):
        rmdir(d, msg="cleanup message")
    assert "cleanup message" in caplog.text


def test_rmdir_raises_runtime_error_on_os_error(tmp_path: Path) -> None:
    d = tmp_path / "stubborn"
    d.mkdir()
    with (
        patch("esphome.framework_helpers.rmtree", side_effect=OSError("perm denied")),
        pytest.raises(RuntimeError, match="can't remove"),
    ):
        rmdir(d, msg="cleanup step")


# ---------------------------------------------------------------------------
# get_system_python_path
# ---------------------------------------------------------------------------


def test_get_system_python_path_returns_env_var() -> None:
    with patch.dict(os.environ, {"PYTHONEXEPATH": "/custom/python"}):
        assert get_system_python_path() == "/custom/python"


def test_get_system_python_path_falls_back_to_sys_executable() -> None:
    env = {k: v for k, v in os.environ.items() if k != "PYTHONEXEPATH"}
    with patch.dict(os.environ, env, clear=True):
        assert get_system_python_path() == os.path.normpath(sys.executable)


# ---------------------------------------------------------------------------
# get_python_env_executable_path
# ---------------------------------------------------------------------------


@pytest.mark.skipif(os.name != "posix", reason="PosixPath construction requires POSIX")
def test_get_python_env_executable_path_posix() -> None:
    assert get_python_env_executable_path("/env", "python") == Path("/env/bin/python")


@pytest.mark.skipif(os.name != "nt", reason="WindowsPath construction requires Windows")
def test_get_python_env_executable_path_windows() -> None:
    assert get_python_env_executable_path("/env", "python") == Path(
        "/env/Scripts/python.exe"
    )


# ---------------------------------------------------------------------------
# run_command
# ---------------------------------------------------------------------------


def test_run_command_success_returns_stdout(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=0, stdout="out\n", stderr="")
    ok, stdout, _stderr = run_command(["echo", "hello"])
    assert ok is True
    assert stdout == "out\n"


def test_run_command_failure_returns_false(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=1, stdout="", stderr="boom")
    ok, _stdout, stderr = run_command(["bad"])
    assert ok is False
    assert stderr == "boom"


def test_run_command_stream_output_success(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=0)
    ok, stdout, stderr = run_command(["cmd"], stream_output=True)
    assert ok is True
    assert stdout is None
    assert stderr is None


def test_run_command_stream_output_failure(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=2)
    ok, stdout, _stderr = run_command(["cmd"], stream_output=True)
    assert ok is False
    assert stdout is None


def test_run_command_subprocess_error_returns_false(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.side_effect = subprocess.SubprocessError("exploded")
    ok, stdout, stderr = run_command(["cmd"])
    assert ok is False
    assert stdout is None
    assert stderr is None


def test_run_command_os_error_returns_false(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.side_effect = OSError("not found")
    ok, _stdout, _stderr = run_command(["cmd"])
    assert ok is False


def test_run_command_passes_env(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=0, stdout="", stderr="")
    run_command(["cmd"], env={"MY_VAR": "42"})
    assert mock_subprocess_run.call_args[1]["env"]["MY_VAR"] == "42"


def test_run_command_passes_cwd(mock_subprocess_run: Mock, tmp_path: Path) -> None:
    mock_subprocess_run.return_value = Mock(returncode=0, stdout="", stderr="")
    run_command(["cmd"], cwd=str(tmp_path))
    assert mock_subprocess_run.call_args[1]["cwd"] == str(tmp_path)


# ---------------------------------------------------------------------------
# run_command_ok
# ---------------------------------------------------------------------------


def test_run_command_ok_true(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=0, stdout="", stderr="")
    assert run_command_ok(["cmd"]) is True


def test_run_command_ok_false(mock_subprocess_run: Mock) -> None:
    mock_subprocess_run.return_value = Mock(returncode=1, stdout="", stderr="")
    assert run_command_ok(["cmd"]) is False


# ---------------------------------------------------------------------------
# create_venv
# ---------------------------------------------------------------------------


def test_create_venv_calls_run_command_ok(tmp_path: Path) -> None:
    with patch(
        "esphome.framework_helpers.run_command_ok", return_value=True
    ) as mock_cmd:
        create_venv(tmp_path / "env", msg="test")
    mock_cmd.assert_called_once()


def test_create_venv_raises_on_failure(tmp_path: Path) -> None:
    with (
        patch("esphome.framework_helpers.run_command_ok", return_value=False),
        pytest.raises(RuntimeError, match="Can't create Python virtual environment"),
    ):
        create_venv(tmp_path / "env", msg="test")


# ---------------------------------------------------------------------------
# _detect_archive_root
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("names", "expected"),
    [
        (["wrapper/", "wrapper/a.txt", "wrapper/sub/b.txt"], "wrapper"),
        (["root1/a.txt", "root2/b.txt"], None),
        (["wrapper"], None),  # no descendant → None
        (["", "wrapper/file.txt"], "wrapper"),  # empty names skipped
        (["wrapper\\file.txt"], "wrapper"),  # backslash normalised
        (["w/a", "w/b", "w/c"], "w"),
    ],
)
def test_detect_archive_root(names: list[str], expected: str | None) -> None:
    assert _detect_archive_root(names) == expected


# ---------------------------------------------------------------------------
# Tar archive helpers
# ---------------------------------------------------------------------------


def _make_tar(
    members: list[tarfile.TarInfo],
    file_contents: dict[str, bytes] | None = None,
) -> io.BytesIO:
    buf = io.BytesIO()
    contents = file_contents or {}
    with tarfile.open(fileobj=buf, mode="w") as tf:
        for info in members:
            if info.isreg() and info.name in contents:
                data = contents[info.name]
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            else:
                tf.addfile(info)
    buf.seek(0)
    return buf


def _reg(name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.REGTYPE
    info.size = 0
    info.mode = 0o644
    return info


def _dir(name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.DIRTYPE
    info.mode = 0o755
    return info


def _sym(name: str, target: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.SYMTYPE
    info.linkname = target
    info.mode = 0o777
    return info


def _special(name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.CHRTYPE
    info.mode = 0o600
    return info


def _hlnk(name: str, target: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.LNKTYPE
    info.linkname = target
    info.mode = 0o644
    return info


# ---------------------------------------------------------------------------
# _tar_extract_all — branches not covered by the hard-link prefix-strip tests
# ---------------------------------------------------------------------------


class TestTarExtractAllSecurity:
    def test_flat_archive_no_wrapper(self, tmp_path: Path) -> None:
        """Without a single common root files land directly in extract_dir."""
        buf = _make_tar(
            [_reg("a.txt"), _reg("b.txt")],
            {"a.txt": b"aaa", "b.txt": b"bbb"},
        )
        _tar_extract_all(buf, tmp_path)
        assert (tmp_path / "a.txt").read_bytes() == b"aaa"
        assert (tmp_path / "b.txt").read_bytes() == b"bbb"

    def test_directory_member_extracted(self, tmp_path: Path) -> None:
        buf = _make_tar([_dir("subdir/")])
        _tar_extract_all(buf, tmp_path)
        assert (tmp_path / "subdir").is_dir()

    def test_symlink_within_dest_extracted(self, tmp_path: Path) -> None:
        buf = _make_tar(
            [_reg("target.txt"), _sym("link.txt", "target.txt")],
            {"target.txt": b"data"},
        )
        _tar_extract_all(buf, tmp_path)
        assert (tmp_path / "link.txt").exists()

    def test_path_traversal_skipped(self, tmp_path: Path) -> None:
        """Member resolving outside extract_dir via .. is silently skipped."""
        info = tarfile.TarInfo(name="sub/../../escape.txt")
        info.type = tarfile.REGTYPE
        info.size = 5
        info.mode = 0o644
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w") as tf:
            tf.addfile(info, io.BytesIO(b"OOPS!"))
        buf.seek(0)
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path.parent / "escape.txt").exists()
        assert not list(tmp_path.rglob("escape.txt"))

    def test_absolute_symlink_target_skipped(self, tmp_path: Path) -> None:
        """Symlink pointing to an absolute path is silently skipped."""
        buf = _make_tar(
            [_reg("real.txt"), _sym("danger.lnk", "/etc/passwd")],
            {"real.txt": b"ok"},
        )
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "danger.lnk").exists()

    def test_symlink_escaping_dest_skipped(self, tmp_path: Path) -> None:
        """Symlink whose resolved path exits extract_dir is silently skipped."""
        buf = _make_tar([_sym("up.lnk", "../outside.txt")])
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "up.lnk").exists()

    def test_special_file_skipped(self, tmp_path: Path) -> None:
        """Character-device and other special-file members are silently skipped."""
        buf = _make_tar([_special("chardev")])
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "chardev").exists()

    @pytest.mark.skipif(
        os.name == "nt", reason="Windows has no POSIX executable permission bit"
    )
    def test_executable_bit_preserved(self, tmp_path: Path) -> None:
        """User-executable bit is kept for explicitly executable files."""
        info = _reg("script.sh")
        info.mode = 0o755
        buf = _make_tar([info], {"script.sh": b"#!/bin/sh"})
        _tar_extract_all(buf, tmp_path)
        assert (tmp_path / "script.sh").stat().st_mode & 0o100  # S_IXUSR

    def test_non_executable_exec_bits_stripped(self, tmp_path: Path) -> None:
        """Exec bits are removed when S_IXUSR is not set."""
        info = _reg("data.bin")
        info.mode = 0o654  # group/other exec present, user exec absent
        buf = _make_tar([info], {"data.bin": b"\x00"})
        _tar_extract_all(buf, tmp_path)
        mode = (tmp_path / "data.bin").stat().st_mode
        assert not (mode & 0o111)  # all exec bits cleared


# ---------------------------------------------------------------------------
# ZIP archive helper
# ---------------------------------------------------------------------------


def _make_zip(entries: list[tuple[str, str | bytes]]) -> io.BytesIO:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        for name, content in entries:
            zf.writestr(name, content)
    buf.seek(0)
    return buf


# ---------------------------------------------------------------------------
# _zip_extract_all
# ---------------------------------------------------------------------------


class TestZipExtractAll:
    def test_basic_extraction_strips_wrapper(self, tmp_path: Path) -> None:
        buf = _make_zip([("wrapper/file.txt", "hello")])
        _zip_extract_all(buf, tmp_path)
        assert (tmp_path / "file.txt").read_text() == "hello"

    def test_flat_archive_no_wrapper(self, tmp_path: Path) -> None:
        buf = _make_zip([("a.txt", "aaa"), ("b.txt", "bbb")])
        _zip_extract_all(buf, tmp_path)
        assert (tmp_path / "a.txt").read_text() == "aaa"
        assert (tmp_path / "b.txt").read_text() == "bbb"

    def test_wrapper_root_entry_skipped(self, tmp_path: Path) -> None:
        """The wrapper directory entry itself (step 3a) does not appear in dest."""
        buf = _make_zip([("wrapper/", ""), ("wrapper/file.txt", "content")])
        _zip_extract_all(buf, tmp_path)
        assert (tmp_path / "file.txt").read_text() == "content"
        assert not (tmp_path / "wrapper").exists()

    def test_path_traversal_raises(self, tmp_path: Path) -> None:
        # Two members with different roots so _detect_archive_root returns None
        # and strip_prefix is not applied, leaving "../escape.txt" to hit the
        # commonpath safety check directly.
        buf = _make_zip([("safe.txt", "ok"), ("../escape.txt", "bad")])
        with pytest.raises(ValueError, match="Unsafe path"):
            _zip_extract_all(buf, tmp_path)

    def test_multiple_files_extracted(self, tmp_path: Path) -> None:
        entries = [(f"root/{c}.txt", c * 3) for c in "abc"]
        buf = _make_zip(entries)
        _zip_extract_all(buf, tmp_path)
        for c in "abc":
            assert (tmp_path / f"{c}.txt").read_text() == c * 3


# ---------------------------------------------------------------------------
# archive_extract_all dispatch
# ---------------------------------------------------------------------------


def _gzip_tar_bytes(entries: dict[str, bytes]) -> bytes:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tf:
        for name, content in entries.items():
            info = tarfile.TarInfo(name=name)
            info.size = len(content)
            info.mode = 0o644
            tf.addfile(info, io.BytesIO(content))
    return buf.getvalue()


class TestArchiveExtractAll:
    def test_path_input_gzip_tar(self, tmp_path: Path) -> None:
        archive = tmp_path / "test.tar.gz"
        archive.write_bytes(_gzip_tar_bytes({"file.txt": b"hello"}))
        dest = tmp_path / "out"
        dest.mkdir()
        archive_extract_all(archive, dest)
        assert (dest / "file.txt").read_bytes() == b"hello"

    def test_buffered_reader_input(self, tmp_path: Path) -> None:
        archive = tmp_path / "test.tar.gz"
        archive.write_bytes(_gzip_tar_bytes({"file.txt": b"data"}))
        dest = tmp_path / "out"
        dest.mkdir()
        with archive.open("rb") as f:  # io.BufferedReader
            archive_extract_all(f, dest)
        assert (dest / "file.txt").read_bytes() == b"data"

    def test_rawio_input(self, tmp_path: Path) -> None:
        archive = tmp_path / "test.tar.gz"
        archive.write_bytes(_gzip_tar_bytes({"file.txt": b"raw"}))
        dest = tmp_path / "out"
        dest.mkdir()
        archive_extract_all(io.FileIO(archive), dest)
        assert (dest / "file.txt").read_bytes() == b"raw"

    def test_zip_dispatched(self, tmp_path: Path) -> None:
        archive = tmp_path / "test.zip"
        archive.write_bytes(_make_zip([("file.txt", "hi")]).getvalue())
        dest = tmp_path / "out"
        dest.mkdir()
        archive_extract_all(archive, dest)
        assert (dest / "file.txt").read_text() == "hi"

    def test_invalid_type_raises_type_error(self) -> None:
        with pytest.raises(TypeError, match="archive must be"):
            archive_extract_all(42, ".")  # type: ignore[arg-type]

    def test_unsupported_format_raises_value_error(self, tmp_path: Path) -> None:
        bad = tmp_path / "bad.bin"
        bad.write_bytes(b"\x00\x01\x02\x03\x04\x05\x06")
        with pytest.raises(ValueError, match="Unsupported archive format"):
            archive_extract_all(bad, tmp_path)


# ---------------------------------------------------------------------------
# download_from_mirrors
# ---------------------------------------------------------------------------


def _mock_response(content: bytes, ok: bool = True) -> MagicMock:
    r = MagicMock()
    r.__enter__.return_value = r
    r.__exit__.return_value = False
    if ok:
        r.raise_for_status.return_value = None
    else:
        r.raise_for_status.side_effect = req.HTTPError("503")
    r.headers = {"content-length": "0"}  # suppress ProgressBar
    r.iter_content.return_value = [content] if content else []
    return r


class TestDownloadFromMirrors:
    def test_success_returns_url_and_writes_content(self, tmp_path: Path) -> None:
        target = tmp_path / "out.bin"
        with patch(
            "esphome.framework_helpers.requests.get",
            return_value=_mock_response(b"filedata"),
        ):
            url = download_from_mirrors(["https://example.com/f"], {}, target)
        assert url == "https://example.com/f"
        assert target.read_bytes() == b"filedata"

    def test_substitutions_applied_to_url(self, tmp_path: Path) -> None:
        with patch(
            "esphome.framework_helpers.requests.get",
            return_value=_mock_response(b"x"),
        ) as mock_get:
            download_from_mirrors(
                ["https://example.com/{VERSION}.bin"],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        assert mock_get.call_args[0][0] == "https://example.com/1.2.3.bin"

    def test_falls_back_to_second_mirror(self, tmp_path: Path) -> None:
        with patch(
            "esphome.framework_helpers.requests.get",
            side_effect=[_mock_response(b"", ok=False), _mock_response(b"second")],
        ):
            url = download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"],
                {},
                tmp_path / "out.bin",
            )
        assert url == "https://mirror2.com/f"
        assert (tmp_path / "out.bin").read_bytes() == b"second"

    def test_all_mirrors_fail_reraises_last_exception(self, tmp_path: Path) -> None:
        with (
            patch(
                "esphome.framework_helpers.requests.get",
                return_value=_mock_response(b"", ok=False),
            ),
            pytest.raises(req.HTTPError),
        ):
            download_from_mirrors(["https://example.com/f"], {}, tmp_path / "out.bin")

    def test_empty_mirrors_raises_value_error(self, tmp_path: Path) -> None:
        with pytest.raises(ValueError, match="empty mirrors list"):
            download_from_mirrors([], {}, tmp_path / "out.bin")

    def test_invalid_target_type_raises_type_error(self) -> None:
        with pytest.raises(TypeError, match="target must be"):
            download_from_mirrors(["https://example.com/f"], {}, 42)  # type: ignore[arg-type]

    def test_file_like_target_written(self) -> None:
        buf = io.BytesIO()
        with patch(
            "esphome.framework_helpers.requests.get",
            return_value=_mock_response(b"bytes"),
        ):
            download_from_mirrors(["https://example.com/f"], {}, buf)
        buf.seek(0)
        assert buf.read() == b"bytes"

    def test_progress_bar_shown_when_content_length_known(self, tmp_path: Path) -> None:
        r = _mock_response(b"1234567890")
        r.headers = {"content-length": "10"}
        with (
            patch("esphome.framework_helpers.requests.get", return_value=r),
            patch("esphome.framework_helpers.ProgressBar") as mock_pb,
        ):
            download_from_mirrors(["https://example.com/f"], {}, tmp_path / "out.bin")
        mock_pb.assert_called_once_with("Downloading")
        mock_pb.return_value.update.assert_called()

    def test_empty_chunk_not_written(self, tmp_path: Path) -> None:
        """Empty chunks yielded by iter_content are skipped without writing."""
        r = MagicMock()
        r.__enter__.return_value = r
        r.__exit__.return_value = False
        r.raise_for_status.return_value = None
        r.headers = {"content-length": "0"}
        r.iter_content.return_value = [b""]  # one empty chunk
        target = tmp_path / "out.bin"
        with patch("esphome.framework_helpers.requests.get", return_value=r):
            download_from_mirrors(["https://example.com/f"], {}, target)
        assert target.exists()
        assert target.read_bytes() == b""


# ---------------------------------------------------------------------------
# get_python_env_executable_path — Windows branch
# ---------------------------------------------------------------------------


def test_get_python_env_executable_path_nt() -> None:
    """Windows path uses Scripts/ and .exe suffix."""
    from pathlib import PurePosixPath

    with (
        patch.object(os, "name", "nt"),
        patch("esphome.framework_helpers.Path", PurePosixPath),
    ):
        result = get_python_env_executable_path("/env", "python")
    assert str(result) == "/env/Scripts/python.exe"


# ---------------------------------------------------------------------------
# _tar_extract_all — additional branch coverage
# ---------------------------------------------------------------------------


class TestTarExtractAllBranches:
    @pytest.mark.skipif(
        sys.version_info < (3, 12),
        reason="patching os.name makes pathlib build a WindowsPath, which only "
        "instantiates on POSIX in 3.12+",
    )
    def test_windows_drive_path_skipped(self, tmp_path: Path) -> None:
        """Windows-style drive path (C:/...) is skipped when os.name == 'nt'."""
        info = tarfile.TarInfo(name="C:/secret.txt")
        info.type = tarfile.REGTYPE
        info.size = 0
        info.mode = 0o644
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w") as tf:
            tf.addfile(info)
        buf.seek(0)
        with patch.object(os, "name", "nt"):
            _tar_extract_all(buf, tmp_path)
        assert not list(tmp_path.rglob("*"))

    def test_strip_root_exact_match_skipped(self, tmp_path: Path) -> None:
        """Member whose name equals strip_root exactly (no trailing slash) is skipped."""
        # "wrapper" (file entry) + "wrapper/file.txt" causes _detect_archive_root
        # to return "wrapper"; the bare "wrapper" entry matches strip_root exactly.
        buf = _make_tar(
            [_reg("wrapper"), _reg("wrapper/file.txt")],
            {"wrapper/file.txt": b"content"},
        )
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "wrapper").exists()
        assert (tmp_path / "file.txt").read_bytes() == b"content"

    def test_member_not_under_strip_prefix_skipped(self, tmp_path: Path) -> None:
        """Member whose name doesn't start with strip_prefix is silently skipped."""
        buf = _make_tar([_reg("other/file.txt")], {"other/file.txt": b"data"})
        with patch("esphome.framework_helpers._detect_archive_root", return_value="w"):
            _tar_extract_all(buf, tmp_path)
        assert not list(tmp_path.rglob("*"))

    def test_hardlink_prefix_stripped(self, tmp_path: Path) -> None:
        """Hard-link linkname has wrapper prefix stripped along with its entry name."""
        buf = _make_tar(
            [_reg("wrapper/file.txt"), _hlnk("wrapper/link.txt", "wrapper/file.txt")],
            {"wrapper/file.txt": b"data"},
        )
        _tar_extract_all(buf, tmp_path)
        assert (tmp_path / "file.txt").read_bytes() == b"data"
        assert (tmp_path / "link.txt").exists()

    def test_hardlink_linkname_equals_strip_root_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname equals strip_root is silently skipped."""
        buf = _make_tar(
            [_reg("wrapper/file.txt"), _hlnk("wrapper/link.txt", "wrapper")],
            {"wrapper/file.txt": b"data"},
        )
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "link.txt").exists()

    def test_hardlink_linkname_outside_prefix_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname doesn't start with strip_prefix is skipped."""
        buf = _make_tar(
            [_reg("wrapper/file.txt"), _hlnk("wrapper/link.txt", "other/file.txt")],
            {"wrapper/file.txt": b"data"},
        )
        _tar_extract_all(buf, tmp_path)
        assert not (tmp_path / "link.txt").exists()

    def test_member_mode_none_skips_sanitization(self, tmp_path: Path) -> None:
        """Member with mode=None bypasses the sanitization block without error."""
        info = _reg("file.txt")
        buf = _make_tar([info], {"file.txt": b"data"})
        buf.seek(0)
        with tarfile.open(fileobj=buf) as tf:
            members = tf.getmembers()
        for m in members:
            m.mode = None
        buf.seek(0)
        with (
            patch("tarfile.TarFile.getmembers", return_value=members),
            patch("tarfile.TarFile.extract"),
        ):
            _tar_extract_all(buf, tmp_path)

    def test_progress_bar_shown(self, tmp_path: Path) -> None:
        """A non-empty progress_header causes ProgressBar to be created and updated."""
        buf = _make_tar([_reg("file.txt")], {"file.txt": b"x"})
        with patch("esphome.framework_helpers.ProgressBar") as mock_pb:
            _tar_extract_all(buf, tmp_path, progress_header="Extracting")
        mock_pb.assert_called_once_with("Extracting")
        mock_pb.return_value.update.assert_called()


# ---------------------------------------------------------------------------
# _zip_extract_all — additional branch coverage
# ---------------------------------------------------------------------------


class TestZipExtractAllBranches:
    @pytest.mark.skipif(
        sys.version_info < (3, 12),
        reason="patching os.name makes pathlib build a WindowsPath, which only "
        "instantiates on POSIX in 3.12+",
    )
    def test_windows_drive_path_skipped(self, tmp_path: Path) -> None:
        """Windows-style drive path (C:/...) is skipped when os.name == 'nt'."""
        buf = _make_zip([("C:/secret.txt", "bad")])
        with patch.object(os, "name", "nt"):
            _zip_extract_all(buf, tmp_path)
        assert not list(tmp_path.rglob("*"))

    def test_member_not_under_strip_prefix_skipped(self, tmp_path: Path) -> None:
        """Member whose name doesn't start with strip_prefix is silently skipped."""
        buf = _make_zip([("other/file.txt", "data")])
        with patch("esphome.framework_helpers._detect_archive_root", return_value="w"):
            _zip_extract_all(buf, tmp_path)
        assert not list(tmp_path.rglob("*"))

    def test_progress_bar_shown(self, tmp_path: Path) -> None:
        """A non-empty progress_header causes ProgressBar to be created and updated."""
        buf = _make_zip([("file.txt", "hello")])
        with patch("esphome.framework_helpers.ProgressBar") as mock_pb:
            _zip_extract_all(buf, tmp_path, progress_header="Unzipping")
        mock_pb.assert_called_once_with("Unzipping")
        mock_pb.return_value.update.assert_called()


# ---------------------------------------------------------------------------
# _rename_with_retry
# ---------------------------------------------------------------------------


class TestRenameWithRetry:
    def test_success_on_first_attempt(self, tmp_path: Path) -> None:
        src = tmp_path / "src.txt"
        src.write_text("data")
        dst = tmp_path / "dst.txt"
        _rename_with_retry(src, dst)
        assert dst.read_text() == "data"
        assert not src.exists()

    def test_retries_on_permission_error_then_succeeds(self, tmp_path: Path) -> None:
        src = tmp_path / "src.txt"
        src.write_text("data")
        dst = tmp_path / "dst.txt"
        call_count = 0
        original_rename = Path.rename

        def flaky_rename(self, target):
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                raise PermissionError("locked")
            return original_rename(self, target)

        with (
            patch.object(Path, "rename", flaky_rename),
            patch("esphome.framework_helpers.time.sleep"),
        ):
            _rename_with_retry(src, dst, attempts=3)
        assert dst.read_text() == "data"

    def test_raises_after_all_attempts_fail(self, tmp_path: Path) -> None:
        src = tmp_path / "src.txt"
        src.write_text("data")
        dst = tmp_path / "dst.txt"
        with (
            patch.object(Path, "rename", side_effect=PermissionError("locked")),
            patch("esphome.framework_helpers.time.sleep"),
            pytest.raises(PermissionError),
        ):
            _rename_with_retry(src, dst, attempts=3)

    def test_attempts_zero_is_noop(self, tmp_path: Path) -> None:
        """Zero attempts means the for-loop body never runs; src is untouched."""
        src = tmp_path / "src.txt"
        src.write_text("data")
        dst = tmp_path / "dst.txt"
        _rename_with_retry(src, dst, attempts=0)
        assert src.exists()
        assert not dst.exists()


# ---------------------------------------------------------------------------
# _7z_extract_all
# ---------------------------------------------------------------------------


@pytest.mark.skipif(not _HAS_PY7ZR, reason="py7zr not installed")
class TestSevenZipExtractAll:
    @staticmethod
    def _make_7z(entries: dict[str, bytes]) -> io.BytesIO:
        import py7zr

        buf = io.BytesIO()
        with py7zr.SevenZipFile(buf, "w") as sz:
            for name, content in entries.items():
                sz.writef(io.BytesIO(content), name)
        buf.seek(0)
        return buf

    def test_basic_extraction_no_wrapper(self, tmp_path: Path) -> None:
        buf = self._make_7z({"a.txt": b"aaa", "b.txt": b"bbb"})
        out = tmp_path / "out"
        out.mkdir()
        _7z_extract_all(buf, out)
        assert (out / "a.txt").exists()
        assert (out / "b.txt").exists()

    def test_strips_wrapper_directory(self, tmp_path: Path) -> None:
        buf = self._make_7z({"wrapper/file.txt": b"data"})
        out = tmp_path / "out"
        out.mkdir()
        _7z_extract_all(buf, out)
        assert (out / "file.txt").exists()
        assert not (out / "wrapper").exists()

    def test_staging_suffix_collision(self, tmp_path: Path) -> None:
        """When .extract_tmp_0 already exists, suffix is incremented to find a free slot."""
        out = tmp_path / "out"
        out.mkdir()
        (out / ".extract_tmp_0").mkdir()
        buf = self._make_7z({"file.txt": b"hi"})
        _7z_extract_all(buf, out)
        assert (out / "file.txt").exists()
        # .extract_tmp_1 should be cleaned up after extraction
        assert not (out / ".extract_tmp_1").exists()

    def test_overwrites_existing_directory(self, tmp_path: Path) -> None:
        """Pre-existing destination directory is replaced."""
        out = tmp_path / "out"
        out.mkdir()
        existing_dir = out / "file.txt"
        existing_dir.mkdir()
        buf = self._make_7z({"file.txt": b"new"})
        _7z_extract_all(buf, out)
        assert (out / "file.txt").is_file()

    def test_overwrites_existing_file(self, tmp_path: Path) -> None:
        """Pre-existing destination file is replaced."""
        out = tmp_path / "out"
        out.mkdir()
        (out / "file.txt").write_bytes(b"old")
        buf = self._make_7z({"file.txt": b"new"})
        _7z_extract_all(buf, out)
        assert (out / "file.txt").exists()

    def test_empty_name_skipped(self, tmp_path: Path) -> None:
        """Archive entries with empty names are silently skipped."""
        import py7zr

        buf = self._make_7z({"file.txt": b"data"})
        out = tmp_path / "out"
        out.mkdir()
        with patch.object(
            py7zr.SevenZipFile, "getnames", return_value=["", "file.txt"]
        ):
            _7z_extract_all(buf, out)
        assert (out / "file.txt").exists()

    def test_path_traversal_skipped(self, tmp_path: Path) -> None:
        """Entries whose resolved path exits extract_dir are skipped."""
        import py7zr

        buf = self._make_7z({"file.txt": b"safe"})
        out = tmp_path / "out"
        out.mkdir()
        with patch.object(
            py7zr.SevenZipFile, "getnames", return_value=["../escape.txt", "file.txt"]
        ):
            _7z_extract_all(buf, out)
        assert not (tmp_path / "escape.txt").exists()
        assert (out / "file.txt").exists()

    def test_progress_bar_shown(self, tmp_path: Path) -> None:
        buf = self._make_7z({"file.txt": b"x"})
        out = tmp_path / "out"
        out.mkdir()
        with patch("esphome.framework_helpers.ProgressBar") as mock_pb:
            _7z_extract_all(buf, out, progress_header="Unpacking 7z")
        mock_pb.assert_called_once_with("Unpacking 7z")
        mock_pb.return_value.update.assert_called()

    def test_absolute_path_in_names_skipped(self, tmp_path: Path) -> None:
        """Names that resolve as absolute are silently skipped."""
        import py7zr

        buf = self._make_7z({"file.txt": b"safe"})
        out = tmp_path / "out"
        out.mkdir()

        original_is_absolute = Path.is_absolute

        def patched_is_absolute(self: Path) -> bool:
            if str(self).startswith("C:"):
                return True
            return original_is_absolute(self)

        with (
            patch.object(
                py7zr.SevenZipFile, "getnames", return_value=["C:/evil.txt", "file.txt"]
            ),
            patch.object(Path, "is_absolute", patched_is_absolute),
        ):
            _7z_extract_all(buf, out)
        # Avoid `out / "C:"` here: pathlib treats "C:" as a drive (always
        # "exists" on Windows). Assert on the actual extracted files instead.
        extracted = sorted(p.name for p in out.rglob("*") if p.is_file())
        assert extracted == ["file.txt"]

    def test_dispatched_via_archive_extract_all(self, tmp_path: Path) -> None:
        """archive_extract_all dispatches 7z archives to _7z_extract_all."""
        buf = self._make_7z({"hello.txt": b"world"})
        data = buf.read()
        assert data[:6] == b"\x37\x7a\xbc\xaf\x27\x1c"
        archive = tmp_path / "test.7z"
        archive.write_bytes(data)
        out = tmp_path / "out"
        out.mkdir()
        archive_extract_all(archive, out)
        assert (out / "hello.txt").exists()


# ---------------------------------------------------------------------------
# get_project_compile_flags / get_project_link_flags
# ---------------------------------------------------------------------------


def _make_core(flags: set[str]):
    core = MagicMock()
    core.build_flags = flags
    return core


class TestGetProjectCompileFlags:
    def test_returns_define_flags(self) -> None:
        with patch("esphome.core.CORE", _make_core({"-DFOO", "-DBAR=1"})):
            assert get_project_compile_flags() == ["-DBAR=1", "-DFOO"]

    def test_returns_warning_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-Wno-error", "-Wall"}),
        ):
            assert get_project_compile_flags() == ["-Wall", "-Wno-error"]

    def test_excludes_linker_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-DFOO", "-Wl,--gc-sections", "-Wl,-Map=output.map"}),
        ):
            assert get_project_compile_flags() == ["-DFOO"]

    def test_excludes_other_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-O2", "-std=gnu++20", "-DFOO"}),
        ):
            assert get_project_compile_flags() == ["-DFOO"]

    def test_empty_build_flags(self) -> None:
        with patch("esphome.core.CORE", _make_core(set())):
            assert get_project_compile_flags() == []

    def test_result_is_sorted(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-DZFLAG", "-DAFLAG", "-Wno-unused"}),
        ):
            result = get_project_compile_flags()
            assert result == sorted(result)


class TestGetProjectLinkFlags:
    def test_returns_linker_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-Wl,--gc-sections", "-Wl,-Map=output.map"}),
        ):
            assert get_project_link_flags() == [
                "-Wl,--gc-sections",
                "-Wl,-Map=output.map",
            ]

    def test_excludes_compile_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-DFOO", "-Wall", "-Wl,--gc-sections"}),
        ):
            assert get_project_link_flags() == ["-Wl,--gc-sections"]

    def test_empty_build_flags(self) -> None:
        with patch("esphome.core.CORE", _make_core(set())):
            assert get_project_link_flags() == []

    def test_result_is_sorted(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core({"-Wl,-z", "-Wl,-a", "-Wl,-m"}),
        ):
            result = get_project_link_flags()
            assert result == sorted(result)
