"""Tests for the script that writes nRF54L RRAM through pyOCD."""

import importlib
from pathlib import Path
import sys
import types

from intelhex import IntelHex
import pytest

RRAMC = 0x5004E000


class FakeCtrlAp:
    def __init__(self, target: "FakeTarget") -> None:
        self.target = target

    def write_reg(self, reg: int, value: int) -> None:
        if reg == 0 and value == 2:
            self.target.resets.append(self.target.halt_on_reset())


class FakeTarget:
    """RRAM that only takes writes while the RRAM controller has writes enabled."""

    def __init__(self, protected: bool = False) -> None:
        self.memory = bytearray(b"\xff" * 0x3000)
        self.regs = {0xE000EDFC: 0, RRAMC + 0x400: 1, RRAMC + 0x500: 0}
        self.protected = protected
        self.ctrl_ap = FakeCtrlAp(self)
        self.resets: list[bool] = []
        self.committed = False

    def halt_on_reset(self) -> bool:
        return bool(self.regs[0xE000EDFC] & 1)

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

    def resume(self) -> None:
        """Running already."""


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
            """Nothing to close."""

    def session_with_chosen_probe(**kwargs) -> Session:
        state.unique_id = kwargs["unique_id"]
        return Session()

    helpers = types.ModuleType("pyocd.core.helpers")
    helpers.ConnectHelper = types.SimpleNamespace(
        session_with_chosen_probe=session_with_chosen_probe
    )
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
    # Halted at reset before writing, then started without halting
    assert target.resets == [True, False]
    assert loader.unique_id == "CB1D0165"


def test_protected_rram_fails_verify(
    loader, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    loader.target.protected = True
    image = _hex(tmp_path / "app.hex", 0x0, b"\x01\x02\x03\x04")
    assert _run(loader, monkeypatch, image) == 1
    assert loader.target.resets == [True]  # not started
