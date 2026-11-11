"""Tests for the script that writes nRF54L RRAM through pyOCD."""

import importlib
from pathlib import Path
import sys
import types

from intelhex import IntelHex
import pytest

RRAMC = 0x5004E000
AIRCR = 0xE000ED0C


class FakeTarget:
    """RRAM that only takes writes while the RRAM controller has writes enabled."""

    def __init__(self, protected: bool = False) -> None:
        self.memory = bytearray(b"\xff" * 0x3000)
        self.regs = {0xE000EDFC: 0, RRAMC + 0x400: 1, RRAMC + 0x500: 0}
        self.protected = protected
        self.committed = False
        self.fault_after_reset = False
        self.transfer_error = Exception

    def read32(self, address: int) -> int:
        return self.regs[address]

    def write32(self, address: int, value: int) -> None:
        if address == RRAMC + 0x008:
            self.committed = True
        self.regs[address] = value

    def read_memory_block8(self, address: int, size: int) -> list[int]:
        return list(self.memory[address : address + size])

    def write_memory_block32(self, address: int, words: list[int]) -> None:
        assert address % 4 == 0
        if self.protected or self.regs[RRAMC + 0x500] != 0x101:
            return
        for i, word in enumerate(words):
            self.memory[address + 4 * i : address + 4 * i + 4] = word.to_bytes(
                4, "little"
            )

    def halt(self) -> None:
        """Halted already."""

    def flush(self) -> None:
        if self.regs.get(AIRCR) and self.fault_after_reset:
            raise self.transfer_error()


@pytest.fixture
def loader(monkeypatch: pytest.MonkeyPatch):
    """The script, with pyOCD replaced by a session holding a fake target."""
    state = types.SimpleNamespace(target=FakeTarget(), unique_id=None)

    class Session:
        target = None

        def __enter__(self):
            self.target = state.target
            return self

        def __exit__(self, *args) -> None:
            if state.target.fault_after_reset and state.target.regs.get(AIRCR):
                raise state.target.transfer_error()

    def session_with_chosen_probe(**kwargs) -> Session:
        state.unique_id = kwargs["unique_id"]
        return Session()

    helpers = types.ModuleType("pyocd.core.helpers")
    helpers.ConnectHelper = types.SimpleNamespace(
        session_with_chosen_probe=session_with_chosen_probe
    )
    exceptions = types.ModuleType("pyocd.core.exceptions")
    exceptions.TransferError = type("TransferError", (Exception,), {})
    monkeypatch.setitem(sys.modules, "pyocd.core.exceptions", exceptions)
    state.target.transfer_error = exceptions.TransferError
    for name in ("pyocd", "pyocd.core"):
        monkeypatch.setitem(sys.modules, name, types.ModuleType(name))
    monkeypatch.setitem(sys.modules, "pyocd.core.helpers", helpers)
    monkeypatch.delitem(
        sys.modules, "esphome.components.nrf52.pyocd_rram_load", raising=False
    )
    monkeypatch.setattr("time.sleep", lambda _: None)
    # A fresh import, bound to this test's fake pyOCD
    state.module = importlib.import_module("esphome.components.nrf52.pyocd_rram_load")
    return state


def _hex(path: Path, address: int, data: bytes) -> str:
    image = IntelHex()
    image.frombytes(data, offset=address)
    image.write_hex_file(str(path))
    return str(path)


def _run(loader, monkeypatch: pytest.MonkeyPatch, *files: str) -> int:
    monkeypatch.setattr(
        sys, "argv", ["x", "nrf54lm20a", hex(RRAMC), "CB1D0165", *files]
    )
    return loader.module.main()


def test_writes_and_verifies_every_file(
    loader, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    mcuboot = _hex(tmp_path / "mcuboot.hex", 0x0, bytes(range(256)) * 20)
    # An unaligned run keeps the bytes next to it
    app = _hex(tmp_path / "app.hex", 0x2002, b"\x11\x22\x33")
    assert _run(loader, monkeypatch, mcuboot, app) == 0
    target = loader.target
    assert target.memory[: 256 * 20] == bytes(range(256)) * 20
    assert target.memory[0x2000:0x2008] == b"\xff\xff\x11\x22\x33\xff\xff\xff"
    assert target.committed
    assert target.regs[RRAMC + 0x500] == 0  # writes disabled again
    assert target.regs[AIRCR] == 0x05FA0004  # started with a system reset
    assert loader.unique_id == "CB1D0165"


def test_protected_rram_fails_verify(
    loader, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    loader.target.protected = True
    image = _hex(tmp_path / "app.hex", 0x0, b"\x01\x02\x03\x04")
    assert _run(loader, monkeypatch, image) == 1
    assert AIRCR not in loader.target.regs  # not started


def test_fault_after_the_starting_reset_is_success(
    loader, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The reset can fault the debug link; the image was verified before it."""
    loader.target.fault_after_reset = True
    image = _hex(tmp_path / "app.hex", 0x0, b"\x01\x02\x03\x04")
    assert _run(loader, monkeypatch, image) == 0
    assert loader.target.memory[:4] == b"\x01\x02\x03\x04"
