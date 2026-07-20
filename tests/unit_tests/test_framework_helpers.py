"""Tests for esphome.framework_helpers."""

# pylint: disable=protected-access

import hashlib
import importlib.util
import io
import json
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

from esphome import framework_helpers
from esphome.core import EsphomeError
from esphome.framework_helpers import (
    _7z_extract_all,
    _detect_archive_root,
    _rename_with_retry,
    _tar_extract_all,
    _zip_extract_all,
    archive_extract_all,
    create_venv,
    download_from_mirrors,
    download_with_resume,
    get_project_compile_flags,
    get_project_cxx_compile_flags,
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
# download_from_mirrors / download_with_resume
# ---------------------------------------------------------------------------


def _mock_response(content: bytes, ok: bool = True) -> MagicMock:
    r = MagicMock()
    r.__enter__.return_value = r
    r.__exit__.return_value = False
    r.status_code = 200
    r.ok = ok
    if ok:
        r.raise_for_status.return_value = None
    else:
        r.raise_for_status.side_effect = req.HTTPError("503")
    r.headers = {"content-length": "0"}  # suppress ProgressBar
    r.iter_content.return_value = [content] if content else []
    return r


def _interrupted_response(content: bytes, etag: str | None = None) -> MagicMock:
    """A response whose body yields ``content`` and then drops mid-stream.

    ``etag`` makes the response resumable: without a validator the retry
    logic restarts from zero rather than stitching unverified bytes.
    """

    def body(chunk_size):
        yield content
        raise req.exceptions.ChunkedEncodingError("connection dropped")

    r = _mock_response(b"")
    if etag is not None:
        r.headers = {**r.headers, "ETag": etag}
    r.iter_content.side_effect = body
    return r


def _resumed_response(content: bytes) -> MagicMock:
    """An HTTP 206 response continuing an interrupted download."""
    r = _mock_response(content)
    r.status_code = 206
    return r


class TestOpenRanged:
    def test_fresh_download_sends_no_range(self) -> None:
        with patch("requests.get", return_value=_mock_response(b"x")) as mock_get:
            resp, offset = framework_helpers._open_ranged("https://e.com/f", 0, 30)
        assert offset == 0
        assert mock_get.call_args[1]["headers"] == {}
        assert resp is mock_get.return_value

    def test_resume_kept_on_206(self) -> None:
        with patch("requests.get", return_value=_resumed_response(b"x")):
            _, offset = framework_helpers._open_ranged("https://e.com/f", 7, 30)
        assert offset == 7

    def test_resume_downgraded_on_200(self) -> None:
        """A server that ignores the Range header forces a restart."""
        with patch("requests.get", return_value=_mock_response(b"x")):
            _, offset = framework_helpers._open_ranged("https://e.com/f", 7, 30)
        assert offset == 0

    def test_http_error_closes_response_and_raises(self) -> None:
        r = _mock_response(b"", ok=False)
        with (
            patch("requests.get", return_value=r),
            pytest.raises(req.HTTPError),
        ):
            framework_helpers._open_ranged("https://e.com/f", 0, 30)
        r.close.assert_called_once()

    def test_connect_error_propagates(self) -> None:
        with (
            patch("requests.get", side_effect=req.ConnectionError("refused")),
            pytest.raises(req.ConnectionError),
        ):
            framework_helpers._open_ranged("https://e.com/f", 0, 30)


class TestDownloadWithResume:
    def test_downloads_and_renames(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        with patch("requests.get", return_value=_mock_response(b"data")) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"data"
        assert not (tmp_path / "tool.tar.gz.part").exists()
        # a fresh download must not send a Range header
        assert "Range" not in mock_get.call_args[1]["headers"]

    def test_mid_stream_drop_resumes_with_range(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "8"}
        with patch(
            "requests.get",
            side_effect=[first, _resumed_response(b"5678")],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest)
        # earlier bytes were kept, remainder appended conditionally
        assert dest.read_bytes() == b"12345678"
        assert mock_get.call_args_list[1][1]["headers"] == {
            "Range": "bytes=4-",
            "If-Range": '"v1"',
        }

    def test_unverifiable_drop_without_length_restarts(self, tmp_path: Path) -> None:
        """A validator alone is not enough to stitch when nothing can prove
        the stitched file complete (no sha/size and no content-length)."""
        dest = tmp_path / "tool.tar.gz"
        with patch(
            "requests.get",
            side_effect=[
                _interrupted_response(b"1234", etag='"v1"'),
                _mock_response(b"full"),
            ],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"full"
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_resumed_clean_but_short_body_discarded(self, tmp_path: Path) -> None:
        """A resumed stream that ends cleanly but short of the advertised
        total is rejected and re-downloaded, not promoted."""
        dest = tmp_path / "tool.tar.gz"
        first = _interrupted_response(b"abcd", etag='"v1"')
        first.headers = {**first.headers, "content-length": "8"}
        # resume ends cleanly after only 2 of the 4 missing bytes
        short = _resumed_response(b"ef")
        full = _mock_response(b"abcdefgh")
        full.headers = {**full.headers, "content-length": "8"}
        with patch("requests.get", side_effect=[first, short, full]) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"abcdefgh"
        # the short stitch was discarded; the final attempt started fresh
        assert "Range" not in mock_get.call_args_list[2][1]["headers"]

    def test_unverifiable_drop_without_validator_restarts(self, tmp_path: Path) -> None:
        """No sha/size and no server validator: the retry must not stitch."""
        dest = tmp_path / "tool.tar.gz"
        with patch(
            "requests.get",
            side_effect=[_interrupted_response(b"1234"), _mock_response(b"full")],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"full"
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_resume_across_invocations_from_part_file(self, tmp_path: Path) -> None:
        """A .part file left by a previous run is resumed, not restarted,
        when sha/size verification will vouch for the stitched result."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"12345")
        good = hashlib.sha256(b"12345678").hexdigest()
        with patch("requests.get", return_value=_resumed_response(b"678")) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=8)
        assert dest.read_bytes() == b"12345678"
        assert mock_get.call_args[1]["headers"] == {"Range": "bytes=5-"}

    def test_unverifiable_leftover_part_file_ignored(self, tmp_path: Path) -> None:
        """Without sha/size there is no way to vouch for a cross-run stitch,
        so a leftover part file starts over."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"12345")
        with patch("requests.get", return_value=_mock_response(b"fresh")) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"fresh"
        assert "Range" not in mock_get.call_args[1]["headers"]

    def test_server_without_range_support_restarts(self, tmp_path: Path) -> None:
        """HTTP 200 in response to a Range request truncates and restarts."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"sta")
        good = hashlib.sha256(b"fresh").hexdigest()
        with patch("requests.get", return_value=_mock_response(b"fresh")) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=5)
        # the Range request was sent (verifiable resume) and downgraded
        assert mock_get.call_args[1]["headers"] == {"Range": "bytes=3-"}
        assert dest.read_bytes() == b"fresh"

    def test_size_only_leftover_part_restarts(self, tmp_path: Path) -> None:
        """A size alone cannot detect a same-length content change on the
        server, so a cross-run part without sha256 restarts from zero."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"12")
        with patch("requests.get", return_value=_mock_response(b"1234")) as mock_get:
            download_with_resume("https://example.com/t", dest, size=4)
        assert "Range" not in mock_get.call_args[1]["headers"]
        assert dest.read_bytes() == b"1234"

    def test_size_only_in_run_drop_resumes_with_validator(self, tmp_path: Path) -> None:
        """Within a run the If-Range validator proves identity, so size-only
        callers still resume mid-stream drops."""
        dest = tmp_path / "tool.tar.gz"
        with patch(
            "requests.get",
            side_effect=[
                _interrupted_response(b"12", etag='"v1"'),
                _resumed_response(b"34"),
            ],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest, size=4)
        assert dest.read_bytes() == b"1234"
        assert mock_get.call_args_list[1][1]["headers"] == {
            "Range": "bytes=2-",
            "If-Range": '"v1"',
        }

    def test_unverifiable_download_logged(
        self, tmp_path: Path, caplog: pytest.LogCaptureFixture
    ) -> None:
        """No sha, no size, no content-length: the download is promoted with
        a debug note (routine for e.g. the constraints host, so not a
        warning) that completeness could not be verified."""
        dest = tmp_path / "tool.tar.gz"
        with (
            caplog.at_level(logging.DEBUG, logger="esphome.framework_helpers"),
            patch("requests.get", return_value=_mock_response(b"data")),
        ):
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"data"
        assert "without any way to verify completeness" in caplog.text

    def test_416_promotes_complete_part_when_size_unknown(self, tmp_path: Path) -> None:
        """sha256-only caller with a byte-complete part file: the server's
        416 confirms nothing is missing, verification promotes in place, and
        the 416 must not loop as a retryable error."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"data")
        good = hashlib.sha256(b"data").hexdigest()
        r416 = _mock_response(b"", ok=False)
        r416.status_code = 416
        with patch("requests.get", return_value=r416) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good)
        assert mock_get.call_count == 1
        r416.close.assert_called_once()
        assert dest.read_bytes() == b"data"

    def test_416_with_corrupt_part_discards_and_redownloads(
        self, tmp_path: Path
    ) -> None:
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"bad!")
        good = hashlib.sha256(b"data").hexdigest()
        r416 = _mock_response(b"", ok=False)
        r416.status_code = 416
        with patch(
            "requests.get", side_effect=[r416, _mock_response(b"data")]
        ) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good)
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]
        assert dest.read_bytes() == b"data"

    def test_hash_mismatch_discards_and_retries(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        good = hashlib.sha256(b"good").hexdigest()
        with patch(
            "requests.get",
            side_effect=[_mock_response(b"bad!"), _mock_response(b"good")],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        assert dest.read_bytes() == b"good"
        # the corrupt part file was discarded, so the retry starts fresh
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_size_mismatch_discards_part(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        with (
            patch("requests.get", return_value=_mock_response(b"xx")),
            pytest.raises(EsphomeError, match="after 2 attempts"),
        ):
            download_with_resume("https://example.com/t", dest, size=99, attempts=2)
        assert not (tmp_path / "tool.tar.gz.part").exists()
        assert not dest.exists()

    def test_attempts_exhausted_keeps_part_file(self, tmp_path: Path) -> None:
        """Mid-stream failures keep the partial file so a later run resumes."""
        dest = tmp_path / "tool.tar.gz"
        first = _interrupted_response(b"12", etag='"v1"')
        first.headers = {**first.headers, "content-length": "4"}
        second = _interrupted_response(b"34")
        second.status_code = 206
        with (
            patch("requests.get", side_effect=[first, second]),
            pytest.raises(EsphomeError, match="after 2 attempts"),
        ):
            download_with_resume("https://example.com/t", dest, attempts=2)
        assert (tmp_path / "tool.tar.gz.part").read_bytes() == b"1234"

    def test_multiple_drops_accumulate_across_attempts(self, tmp_path: Path) -> None:
        """Each attempt appends its bytes; three partial responses complete
        the file."""
        dest = tmp_path / "tool.tar.gz"
        first = _interrupted_response(b"ab", etag='"v1"')
        first.headers = {**first.headers, "content-length": "6"}
        second = _interrupted_response(b"cd")
        second.status_code = 206
        third = _resumed_response(b"ef")
        with patch(
            "requests.get",
            side_effect=[first, second, third],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"abcdef"
        expected = {"Range": "bytes=2-", "If-Range": '"v1"'}
        assert mock_get.call_args_list[1][1]["headers"] == expected
        expected = {"Range": "bytes=4-", "If-Range": '"v1"'}
        assert mock_get.call_args_list[2][1]["headers"] == expected

    def test_connect_error_then_success(self, tmp_path: Path) -> None:
        """A connect error (no response at all) consumes an attempt and the
        next attempt succeeds."""
        dest = tmp_path / "tool.tar.gz"
        with patch(
            "requests.get",
            side_effect=[req.ConnectionError("refused"), _mock_response(b"data")],
        ):
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"data"

    def test_http_error_keeps_part_file(self, tmp_path: Path) -> None:
        """A transient HTTP error (e.g. 503) must not discard resume state."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"keep")
        error = _mock_response(b"", ok=False)
        error.status_code = 503
        with (
            patch("requests.get", return_value=error),
            pytest.raises(EsphomeError, match="after 1 attempts"),
        ):
            download_with_resume("https://example.com/t", dest, attempts=1)
        assert (tmp_path / "tool.tar.gz.part").read_bytes() == b"keep"

    def test_creates_missing_parent_directories(self, tmp_path: Path) -> None:
        dest = tmp_path / "dist" / "nested" / "tool.tar.gz"
        with patch("requests.get", return_value=_mock_response(b"data")):
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"data"

    def test_verifies_both_size_and_sha(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get", return_value=_mock_response(b"data")):
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        assert dest.read_bytes() == b"data"

    def test_corrupt_partial_resumed_then_discarded_then_redownloaded(
        self, tmp_path: Path
    ) -> None:
        """The full recovery cycle for a corrupted partial download: the
        resume completes it, verification fails, the poisoned part file is
        discarded, and the next attempt re-downloads from scratch."""
        dest = tmp_path / "tool.tar.gz"
        # a previous run left a corrupted 4-byte prefix behind
        (tmp_path / "tool.tar.gz.part").write_bytes(b"BAD!")
        good = hashlib.sha256(b"data66").hexdigest()
        with patch(
            "requests.get",
            side_effect=[
                _resumed_response(b"66"),  # resume "completes" the bad part
                _mock_response(b"data66"),  # clean retry from zero
            ],
        ) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=6)
        # first attempt resumed at the corrupt offset, failed verification;
        # second attempt started fresh (no Range header) and succeeded
        assert mock_get.call_args_list[0][1]["headers"] == {"Range": "bytes=4-"}
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]
        assert dest.read_bytes() == b"data66"
        assert not (tmp_path / "tool.tar.gz.part").exists()

    def test_existing_dest_passing_verification_kept(self, tmp_path: Path) -> None:
        """A dest completed by an earlier run is reused without any request."""
        dest = tmp_path / "tool.tar.gz"
        dest.write_bytes(b"data")
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get") as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        mock_get.assert_not_called()
        assert dest.read_bytes() == b"data"

    @pytest.mark.parametrize(
        "stale",
        [
            pytest.param(b"corrupt!", id="wrong-size"),
            pytest.param(b"bad!", id="right-size-wrong-hash"),
        ],
    )
    def test_existing_dest_failing_verification_redownloaded(
        self, tmp_path: Path, stale: bytes
    ) -> None:
        dest = tmp_path / "tool.tar.gz"
        dest.write_bytes(stale)
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get", return_value=_mock_response(b"data")):
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        assert dest.read_bytes() == b"data"

    def test_existing_dest_with_size_only_kept(self, tmp_path: Path) -> None:
        dest = tmp_path / "tool.tar.gz"
        dest.write_bytes(b"data")
        with patch("requests.get") as mock_get:
            download_with_resume("https://example.com/t", dest, size=4)
        mock_get.assert_not_called()

    def test_existing_dest_with_sha_only_kept(self, tmp_path: Path) -> None:
        """sha-only verification also authorizes reusing a completed dest."""
        dest = tmp_path / "tool.tar.gz"
        dest.write_bytes(b"data")
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get") as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good)
        mock_get.assert_not_called()

    def test_meta_write_failure_is_best_effort(self, tmp_path: Path) -> None:
        """A failure to persist the resume sidecar must not fail the
        download itself."""
        dest = tmp_path / "f.tar.xz"
        first = _mock_response(b"data")
        first.headers = {**first.headers, "ETag": '"v1"', "content-length": "4"}
        with (
            patch("requests.get", return_value=first),
            patch.object(Path, "write_text", side_effect=OSError("read-only")),
        ):
            download_with_resume("https://example.com/f", dest)
        assert dest.read_bytes() == b"data"

    def test_meta_sidecar_written_and_removed(self, tmp_path: Path) -> None:
        """The validator sidecar appears while downloading and is cleaned up
        with the promotion."""
        dest = tmp_path / "f.tar.xz"
        meta = tmp_path / "f.tar.xz.part.meta"
        seen: list[bool] = []
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "8"}
        responses = [first]

        def get(*args: object, **kwargs: object) -> MagicMock:
            if responses:
                return responses.pop(0)
            # the resume request: the sidecar written by the first response
            # must already be on disk at this point
            seen.append(meta.is_file())
            return _resumed_response(b"5678")

        with patch("requests.get", side_effect=get):
            download_with_resume("https://example.com/f", dest)
        assert dest.read_bytes() == b"12345678"
        assert seen == [True]  # sidecar existed during the resume attempt
        assert not meta.exists()  # cleaned up on success

    def test_locked_promotion_keeps_verified_part(self, tmp_path: Path) -> None:
        """A rename that stays blocked (e.g. a long-lived Windows file lock)
        must not delete the verified download; the next attempt retries just
        the rename without touching the network."""
        dest = tmp_path / "tool.tar.gz"
        good = hashlib.sha256(b"data").hexdigest()
        with (
            patch("requests.get", return_value=_mock_response(b"data")) as mock_get,
            patch(
                "esphome.framework_helpers._rename_with_retry",
                side_effect=[PermissionError("locked"), None],
            ) as rename,
        ):
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        # one download; the second attempt only redid the rename
        assert mock_get.call_count == 1
        assert rename.call_count == 2

    def test_locked_promotion_exhausted_keeps_part_for_next_run(
        self, tmp_path: Path
    ) -> None:
        dest = tmp_path / "tool.tar.gz"
        good = hashlib.sha256(b"data").hexdigest()
        with (
            patch("requests.get", return_value=_mock_response(b"data")),
            patch(
                "esphome.framework_helpers._rename_with_retry",
                side_effect=PermissionError("locked"),
            ),
            pytest.raises(EsphomeError, match="after 1 attempts"),
        ):
            download_with_resume(
                "https://example.com/t", dest, sha256=good, size=4, attempts=1
            )
        # the verified bytes survive for the next run
        assert (tmp_path / "tool.tar.gz.part").read_bytes() == b"data"

    def test_meta_sidecar_resumes_across_runs_without_sha(self, tmp_path: Path) -> None:
        """A later run resumes an unfinished download using the validator the
        first run stored — the cross-run fix for the framework tarball."""
        dest = tmp_path / "f.tar.xz"
        (tmp_path / "f.tar.xz.part").write_bytes(b"1234")
        (tmp_path / "f.tar.xz.part.meta").write_text(
            json.dumps(
                {"url": "https://example.com/f", "validator": '"v1"', "total": 8}
            )
        )
        with patch("requests.get", return_value=_resumed_response(b"5678")) as mock_get:
            download_with_resume("https://example.com/f", dest)
        assert dest.read_bytes() == b"12345678"
        assert mock_get.call_args[1]["headers"] == {
            "Range": "bytes=4-",
            "If-Range": '"v1"',
        }

    def test_meta_sidecar_for_other_url_ignored(self, tmp_path: Path) -> None:
        """Metadata from a different mirror URL must not authorize a stitch."""
        dest = tmp_path / "f.tar.xz"
        (tmp_path / "f.tar.xz.part").write_bytes(b"1234")
        (tmp_path / "f.tar.xz.part.meta").write_text(
            json.dumps({"url": "https://other.com/f", "validator": '"v1"', "total": 8})
        )
        full = _mock_response(b"12345678")
        with patch("requests.get", return_value=full) as mock_get:
            download_with_resume("https://example.com/f", dest)
        assert "Range" not in mock_get.call_args[1]["headers"]
        assert dest.read_bytes() == b"12345678"

    def test_complete_part_file_promoted_without_network(self, tmp_path: Path) -> None:
        """A .part holding every byte (killed between write and rename) is
        verified in place and promoted; no request is made, so no 416 loop."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"data")
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get") as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        mock_get.assert_not_called()
        assert dest.read_bytes() == b"data"

    def test_complete_but_corrupt_part_file_redownloaded(self, tmp_path: Path) -> None:
        """A full-size .part with a wrong hash is discarded and re-downloaded
        from scratch."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"bad!")
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get", return_value=_mock_response(b"data")) as mock_get:
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        assert "Range" not in mock_get.call_args[1]["headers"]
        assert dest.read_bytes() == b"data"

    def test_oversized_part_file_discarded(self, tmp_path: Path) -> None:
        """A .part larger than the expected size fails verification and is
        replaced by a fresh download."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"toolong")
        good = hashlib.sha256(b"data").hexdigest()
        with patch("requests.get", return_value=_mock_response(b"data")):
            download_with_resume("https://example.com/t", dest, sha256=good, size=4)
        assert dest.read_bytes() == b"data"

    def test_malformed_content_length_degrades_gracefully(self, tmp_path: Path) -> None:
        """A garbage Content-Length must not crash the attempt; it means
        "unknown", so a drop restarts instead of stitching and a clean
        download still succeeds."""
        dest = tmp_path / "tool.tar.gz"
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "explode"}
        retry = _mock_response(b"full")
        retry.headers = {**retry.headers, "content-length": "explode"}
        with patch("requests.get", side_effect=[first, retry]) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert dest.read_bytes() == b"full"
        # unknown length -> completeness unprovable -> no resume attempted
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_zero_byte_part_file_sends_no_range(self, tmp_path: Path) -> None:
        """An empty leftover part file is a fresh download, not a resume."""
        dest = tmp_path / "tool.tar.gz"
        (tmp_path / "tool.tar.gz.part").write_bytes(b"")
        with patch("requests.get", return_value=_mock_response(b"data")) as mock_get:
            download_with_resume("https://example.com/t", dest)
        assert mock_get.call_args[1]["headers"] == {}
        assert dest.read_bytes() == b"data"


class TestDownloadFromMirrors:
    def test_success_returns_url_and_writes_content(self, tmp_path: Path) -> None:
        target = tmp_path / "out.bin"
        with patch(
            "requests.get",
            return_value=_mock_response(b"filedata"),
        ):
            url = download_from_mirrors(["https://example.com/f"], {}, target)
        assert url == "https://example.com/f"
        assert target.read_bytes() == b"filedata"

    def test_substitutions_applied_to_url(self, tmp_path: Path) -> None:
        with patch(
            "requests.get",
            return_value=_mock_response(b"x"),
        ) as mock_get:
            download_from_mirrors(
                ["https://example.com/{VERSION}.bin"],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        assert mock_get.call_args[0][0] == "https://example.com/1.2.3.bin"

    def test_template_with_missing_substitution_is_skipped(
        self, tmp_path: Path
    ) -> None:
        """A template referencing an unavailable substitution is skipped, not
        formatted into a bogus URL (e.g. SHORT_VERSION only exists for x.y.0
        framework versions)."""
        with patch(
            "requests.get",
            return_value=_mock_response(b"x"),
        ) as mock_get:
            url = download_from_mirrors(
                [
                    "https://example.com/{SHORT_VERSION}.bin",
                    "https://example.com/{VERSION}.bin",
                ],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        assert url == "https://example.com/1.2.3.bin"
        assert mock_get.call_count == 1

    def test_all_templates_skipped_raises_esphome_error(self, tmp_path: Path) -> None:
        with (
            patch("requests.get") as mock_get,
            pytest.raises(EsphomeError, match="No mirror URL template matched") as ei,
        ):
            download_from_mirrors(
                ["https://example.com/{MISSING}.bin"],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        mock_get.assert_not_called()
        # The skipped template and its missing substitution are named
        assert "https://example.com/{MISSING}.bin" in str(ei.value)
        assert "MISSING" in str(ei.value)

    def test_failure_message_includes_skipped_templates(self, tmp_path: Path) -> None:
        """When downloads fail, templates that were skipped for missing
        substitutions are also listed so a typo'd custom mirror is
        attributable."""
        with (
            patch(
                "requests.get",
                return_value=_mock_response(b"", ok=False),
            ),
            pytest.raises(EsphomeError, match="all mirrors") as ei,
        ):
            download_from_mirrors(
                [
                    "https://example.com/{TYPO}.bin",
                    "https://example.com/{VERSION}.bin",
                ],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        message = str(ei.value)
        assert "https://example.com/1.2.3.bin" in message
        assert (
            "https://example.com/{TYPO}.bin\n    not applicable (TYPO not available)"
            in message
        )

    def test_malformed_template_warns_and_is_reported(
        self, tmp_path: Path, caplog: pytest.LogCaptureFixture
    ) -> None:
        """A structurally malformed template is an authoring error: warned
        about even when another mirror succeeds, and named in the aggregate
        error when everything fails."""
        with (
            patch("requests.get", return_value=_mock_response(b"x")),
            caplog.at_level(logging.WARNING, logger="esphome.framework_helpers"),
        ):
            url = download_from_mirrors(
                ["https://example.com/{oops.bin", "https://example.com/{VERSION}.bin"],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        assert url == "https://example.com/1.2.3.bin"
        assert "malformed mirror URL template" in caplog.text

        with (
            patch("requests.get", return_value=_mock_response(b"", ok=False)),
            pytest.raises(EsphomeError, match="all mirrors") as ei,
        ):
            download_from_mirrors(
                ["https://example.com/{oops.bin", "https://example.com/{VERSION}.bin"],
                {"VERSION": "1.2.3"},
                tmp_path / "out.bin",
            )
        assert "https://example.com/{oops.bin\n    skipped (ValueError(" in str(
            ei.value
        )

    def test_falls_back_to_second_mirror(self) -> None:
        buf = io.BytesIO()
        with patch(
            "requests.get",
            side_effect=[_mock_response(b"", ok=False), _mock_response(b"second")],
        ):
            url = download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"],
                {},
                buf,
            )
        assert url == "https://mirror2.com/f"
        assert buf.getvalue() == b"second"

    def test_mid_stream_drop_resumes_same_mirror(self) -> None:
        """A mid-stream failure retries the same mirror with Range and
        If-Range headers, keeping the bytes already received, before falling
        to the next."""
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "8"}
        buf = io.BytesIO()
        with patch(
            "requests.get",
            side_effect=[first, _resumed_response(b"5678")],
        ) as mock_get:
            url = download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"],
                {},
                buf,
            )
        assert url == "https://mirror1.com/f"
        assert buf.getvalue() == b"12345678"
        assert mock_get.call_count == 2
        assert mock_get.call_args_list[1][0][0] == "https://mirror1.com/f"
        # the resume is conditional on the content being unchanged
        assert mock_get.call_args_list[1][1]["headers"] == {
            "Range": "bytes=4-",
            "If-Range": '"v1"',
        }

    def test_mid_stream_drop_without_validator_restarts(self) -> None:
        """A server offering no ETag/Last-Modified cannot be resumed safely;
        the retry restarts from zero instead of stitching unverified bytes."""
        buf = io.BytesIO()
        with patch(
            "requests.get",
            side_effect=[_interrupted_response(b"1234"), _mock_response(b"full")],
        ) as mock_get:
            download_from_mirrors(["https://mirror1.com/f"], {}, buf)
        assert buf.getvalue() == b"full"
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_drop_after_last_byte_recovers_via_416(self) -> None:
        """A connection drop after the final body byte leaves a complete file;
        the retry's 416 answer plus the length check turn it into success
        instead of a wasted refetch."""
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "4"}
        r416 = _mock_response(b"", ok=False)
        r416.status_code = 416
        buf = io.BytesIO()
        with patch("requests.get", side_effect=[first, r416]) as mock_get:
            url = download_from_mirrors(["https://mirror1.com/f"], {}, buf)
        assert url == "https://mirror1.com/f"
        assert buf.getvalue() == b"1234"
        assert mock_get.call_count == 2

    def test_mirror_drop_without_length_restarts(self) -> None:
        """With no content-length there is no way to prove a stitched file
        complete, so the retry restarts even though a validator exists."""
        buf = io.BytesIO()
        with patch(
            "requests.get",
            side_effect=[
                _interrupted_response(b"1234", etag='"v1"'),
                _mock_response(b"full"),
            ],
        ) as mock_get:
            download_from_mirrors(["https://mirror1.com/f"], {}, buf)
        assert buf.getvalue() == b"full"
        assert "Range" not in mock_get.call_args_list[1][1]["headers"]

    def test_path_target_resumes_across_runs(self, tmp_path: Path) -> None:
        """A path target routes through download_with_resume: a part file and
        metadata from a previous run resume instead of restarting."""
        dest = tmp_path / "idf.tar.xz"
        (tmp_path / "idf.tar.xz.part").write_bytes(b"1234")
        (tmp_path / "idf.tar.xz.part.meta").write_text(
            json.dumps(
                {"url": "https://mirror1.com/f", "validator": '"v1"', "total": 8}
            )
        )
        with patch("requests.get", return_value=_resumed_response(b"5678")) as mock_get:
            url = download_from_mirrors(["https://mirror1.com/f"], {}, dest)
        assert url == "https://mirror1.com/f"
        assert dest.read_bytes() == b"12345678"
        assert mock_get.call_args[1]["headers"] == {
            "Range": "bytes=4-",
            "If-Range": '"v1"',
        }

    def test_path_target_falls_back_to_next_mirror(self, tmp_path: Path) -> None:
        dest = tmp_path / "idf.tar.xz"
        with patch(
            "requests.get",
            side_effect=[req.ConnectionError("down"), _mock_response(b"data")],
        ):
            url = download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"], {}, dest
            )
        assert url == "https://mirror2.com/f"
        assert dest.read_bytes() == b"data"

    def test_resumed_short_body_fails_length_check(self) -> None:
        """A stitched file whose final length disagrees with the advertised
        total is rejected instead of reported as success."""
        first = _interrupted_response(b"1234", etag='"v1"')
        first.headers = {**first.headers, "content-length": "8"}
        # the resume ends early (5 of 8 bytes); the poisoned part is then
        # discarded and the fresh retry also delivers a short body
        short_resume = _resumed_response(b"5")
        short_fresh = _mock_response(b"56")
        short_fresh.headers = {**short_fresh.headers, "content-length": "8"}
        buf = io.BytesIO()
        with (
            patch("requests.get", side_effect=[first, short_resume, short_fresh]),
            pytest.raises(EsphomeError, match="all mirrors"),
        ):
            download_from_mirrors(["https://mirror1.com/f"], {}, buf)

    def test_failed_mirror_leftovers_not_kept_for_next_mirror(self) -> None:
        """Bytes from a mirror that failed all attempts must not leak into the
        next mirror's download (no bogus Range request, fresh content)."""
        exhausted = [_interrupted_response(b"AAAA", etag='"a1"')]
        for _ in range(2):
            r = _interrupted_response(b"BB")
            r.status_code = 206
            exhausted.append(r)
        buf = io.BytesIO()
        with patch(
            "requests.get",
            side_effect=exhausted + [_mock_response(b"clean")],
        ) as mock_get:
            url = download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"],
                {},
                buf,
            )
        assert url == "https://mirror2.com/f"
        assert buf.getvalue() == b"clean"
        # the second mirror starts fresh, without a Range header
        assert mock_get.call_args_list[3][0][0] == "https://mirror2.com/f"
        assert "Range" not in mock_get.call_args_list[3][1]["headers"]

    def test_all_mirrors_fail_raises_error_listing_every_attempt(self) -> None:
        with (
            patch(
                "requests.get",
                return_value=_mock_response(b"", ok=False),
            ),
            pytest.raises(EsphomeError, match="all mirrors") as excinfo,
        ):
            download_from_mirrors(
                ["https://mirror1.com/f", "https://mirror2.com/f"],
                {},
                io.BytesIO(),
            )
        # Every attempted URL appears in the message, and the first mirror's
        # exception (the primary URL, usually the one that matters) is chained.
        assert "https://mirror1.com/f" in str(excinfo.value)
        assert "https://mirror2.com/f" in str(excinfo.value)
        assert isinstance(excinfo.value.__cause__, req.HTTPError)

    def test_empty_mirrors_raises_value_error(self, tmp_path: Path) -> None:
        with pytest.raises(ValueError, match="empty mirrors list"):
            download_from_mirrors([], {}, tmp_path / "out.bin")

    def test_invalid_target_type_raises_type_error(self) -> None:
        with pytest.raises(TypeError, match="target must be"):
            download_from_mirrors(["https://example.com/f"], {}, 42)  # type: ignore[arg-type]

    def test_file_like_target_written(self) -> None:
        buf = io.BytesIO()
        with patch(
            "requests.get",
            return_value=_mock_response(b"bytes"),
        ):
            download_from_mirrors(["https://example.com/f"], {}, buf)
        buf.seek(0)
        assert buf.read() == b"bytes"

    def test_progress_bar_shown_when_content_length_known(self, tmp_path: Path) -> None:
        r = _mock_response(b"1234567890")
        r.headers = {"content-length": "10"}
        with (
            patch("requests.get", return_value=r),
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
        with patch("requests.get", return_value=r):
            download_from_mirrors(["https://example.com/f"], {}, target)
        assert target.exists()
        assert target.read_bytes() == b""


def test_importing_framework_helpers_does_not_import_requests() -> None:
    """Importing framework_helpers must not drag in requests.

    requests is a heavy import (~85ms) only needed by download_from_mirrors to
    fetch toolchains during a build. framework_helpers is loaded during config
    validation (esp-idf framework, host platform), so the import is deferred to
    the function that uses it. A fresh interpreter is required because the test
    process has already imported requests.
    """
    result = subprocess.run(
        [
            sys.executable,
            "-c",
            "import sys\nimport esphome.framework_helpers\n"
            "print('\\n'.join(sys.modules))",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "requests" not in result.stdout.split()


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


def _make_core_cxx(flags: set[str]) -> MagicMock:
    core = MagicMock()
    core.cxx_build_flags = flags
    return core


class TestGetProjectCxxCompileFlags:
    def test_returns_sorted_flags(self) -> None:
        with patch(
            "esphome.core.CORE",
            _make_core_cxx({"-Wno-volatile", "-Wno-deprecated"}),
        ):
            assert get_project_cxx_compile_flags() == [
                "-Wno-deprecated",
                "-Wno-volatile",
            ]

    def test_empty_flags(self) -> None:
        with patch("esphome.core.CORE", _make_core_cxx(set())):
            assert get_project_cxx_compile_flags() == []
