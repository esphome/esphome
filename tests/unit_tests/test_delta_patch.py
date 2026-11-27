"""Unit tests for esphome.delta_patch."""

from __future__ import annotations

import hashlib
import io
from pathlib import Path
import random
import runpy
import sys

import detools
import pytest

from esphome import delta_patch
from esphome.core import EsphomeError


def _app_image(body: bytes, signed: bool = False) -> tuple[bytes, bytes]:
    """A structurally valid ESP32 app image and its digest.

    Header, one segment, checksum padding, then the appended SHA-256. A signed image also has padding and a
    signature block after the digest, as secure boot v2 adds.
    """
    header = bytearray(24)
    header[0] = 0xE9
    header[1] = 1
    header[23] = 1
    image = bytes(header) + (0x3C000020).to_bytes(4, "little")
    image += len(body).to_bytes(4, "little") + body
    pad = 16 - len(image) % 16
    image += bytes(pad - 1) + bytes([0xEF])
    digest = hashlib.sha256(image).digest()
    image += digest
    if signed:
        image += bytes(-len(image) % 4096) + bytes(range(256)) * 4
    return image, digest


@pytest.fixture
def images(tmp_path: Path) -> tuple[Path, Path, bytes, bytes, bytes]:
    rng = random.Random(1)
    body = bytes(rng.getrandbits(8) for _ in range(64 * 1024))
    base, base_digest = _app_image(body)
    new, _ = _app_image(body[:30000] + b"new version string" + body[30018:])
    base_path = tmp_path / "base.bin"
    new_path = tmp_path / "new.bin"
    base_path.write_bytes(base)
    new_path.write_bytes(new)
    return base_path, new_path, base, new, base_digest


def test_make_patch_header_and_round_trip(
    images: tuple[Path, Path, bytes, bytes, bytes],
) -> None:
    base_path, new_path, base, new, base_digest = images

    patch = delta_patch.make_patch(base_path, new_path)

    assert int.from_bytes(patch[0:4], "little") == delta_patch.DELTA_MAGIC
    assert patch[4:36] == base_digest
    assert patch[36:52] == hashlib.md5(new).digest()
    assert patch[52:64] == bytes(12)
    assert len(patch) < len(new) // 4
    assert delta_patch.new_image_md5(patch) == hashlib.md5(new).hexdigest()

    rebuilt = io.BytesIO()
    detools.apply_patch(
        io.BytesIO(base), io.BytesIO(patch[delta_patch.HEADER_SIZE :]), rebuilt
    )
    assert rebuilt.getvalue() == new


def test_main_writes_patch_and_prints_hashes(
    images: tuple[Path, Path, bytes, bytes, bytes],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    base_path, new_path, _, _, base_digest = images
    out = tmp_path / "out.delta.bin"

    assert delta_patch.main([str(base_path), str(new_path), str(out)]) == 0

    patch = out.read_bytes()
    sha, md5, size = capsys.readouterr().out.split()
    assert sha == base_digest.hex()
    assert md5 == hashlib.md5(patch).hexdigest()
    assert int(size) == len(patch)


def test_main_usage() -> None:
    assert delta_patch.main(["only-one"]) == 2


def test_run_as_module(monkeypatch: pytest.MonkeyPatch) -> None:
    """Python -m esphome.delta_patch exits with main()'s status."""
    monkeypatch.setattr(sys, "argv", ["delta_patch", "only-one"])
    with pytest.raises(SystemExit) as exc:
        runpy.run_module("esphome.delta_patch", run_name="__main__")
    assert exc.value.code == 2


def test_signed_base_uses_the_digest_before_the_signature(tmp_path: Path) -> None:
    """The device reports the app's digest, not the signature block's last bytes."""
    rng = random.Random(2)
    body = bytes(rng.getrandbits(8) for _ in range(8 * 1024))
    base, base_digest = _app_image(body, signed=True)
    new, _ = _app_image(body[:100] + b"changed" + body[107:], signed=True)
    (tmp_path / "base.bin").write_bytes(base)
    (tmp_path / "new.bin").write_bytes(new)

    patch = delta_patch.make_patch(tmp_path / "base.bin", tmp_path / "new.bin")

    assert patch[4:36] == base_digest
    assert patch[4:36] != base[-32:]


def test_base_without_appended_sha_is_refused(tmp_path: Path) -> None:
    base = tmp_path / "base.bin"
    new = tmp_path / "new.bin"
    base.write_bytes(b"\x00" * 1024)
    new.write_bytes(_app_image(b"x" * 1024)[0])
    with pytest.raises(EsphomeError, match="appended SHA-256"):
        delta_patch.make_patch(base, new)


def test_base_with_a_wrong_digest_is_refused(tmp_path: Path) -> None:
    base = bytearray(_app_image(b"x" * 1024)[0])
    base[-1] ^= 0xFF
    (tmp_path / "base.bin").write_bytes(bytes(base))
    (tmp_path / "new.bin").write_bytes(_app_image(b"y" * 1024)[0])
    with pytest.raises(EsphomeError, match="appended SHA-256"):
        delta_patch.make_patch(tmp_path / "base.bin", tmp_path / "new.bin")


def test_truncated_base_is_refused(tmp_path: Path) -> None:
    header = bytearray(24)
    header[0] = 0xE9
    header[1] = 3
    header[23] = 1
    (tmp_path / "base.bin").write_bytes(bytes(header))
    (tmp_path / "new.bin").write_bytes(_app_image(b"y" * 1024)[0])
    with pytest.raises(EsphomeError, match="truncated"):
        delta_patch.make_patch(tmp_path / "base.bin", tmp_path / "new.bin")


def test_missing_detools_names_the_package(
    images: tuple[Path, Path, bytes, bytes, bytes],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    base_path, new_path, *_ = images
    monkeypatch.setitem(sys.modules, "detools", None)
    with pytest.raises(EsphomeError, match="pip install detools"):
        delta_patch.make_patch(base_path, new_path)


def test_patch_that_does_not_rebuild_is_refused(
    images: tuple[Path, Path, bytes, bytes, bytes],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The local check catches a patch that would rebuild the wrong image on the device."""
    base_path, new_path, *_ = images
    monkeypatch.setattr(
        detools, "apply_patch", lambda base, patch, out: out.write(b"wrong")
    )
    with pytest.raises(EsphomeError, match="does not rebuild"):
        delta_patch.make_patch(base_path, new_path)


def test_new_image_md5_rejects_other_files() -> None:
    with pytest.raises(EsphomeError, match="Not an ESPHome delta patch"):
        delta_patch.new_image_md5(b"\xe9" + bytes(100))
    with pytest.raises(EsphomeError, match="Not an ESPHome delta patch"):
        delta_patch.new_image_md5(b"short")


def test_unreadable_base_is_an_esphome_error(tmp_path: Path) -> None:
    (tmp_path / "new.bin").write_bytes(_app_image(b"y" * 1024)[0])
    with pytest.raises(EsphomeError, match="Cannot read"):
        delta_patch.make_patch(tmp_path / "missing.bin", tmp_path / "new.bin")


def test_main_reports_errors_without_a_traceback(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    out = tmp_path / "out.delta.bin"
    assert (
        delta_patch.main([str(tmp_path / "missing.bin"), str(tmp_path / "x"), str(out)])
        == 1
    )
    assert "Cannot read" in capsys.readouterr().err
    assert not out.exists()
