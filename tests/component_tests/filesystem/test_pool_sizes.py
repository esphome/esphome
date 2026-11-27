"""Tests for the filesystem handle pool sizes."""

import asyncio

import pytest

from esphome.components import filesystem
from esphome.core import CORE, ID, EsphomeError
from esphome.cpp_generator import MockObj


@pytest.fixture(autouse=True)
def _clear_data() -> None:
    CORE.data.pop(filesystem.DOMAIN, None)


def _add_filesystem(name: str, files: int, dirs: int) -> ID:
    fs_id = ID(name, is_declaration=True, type=filesystem.Filesystem)
    filesystem._get_data().pools[str(fs_id)] = filesystem._Pools(
        MockObj(name), files, dirs
    )
    return fs_id


@pytest.mark.parametrize(
    ("own", "requested"),
    [((32, 0), (1, 0)), ((0, 8), (0, 1))],
)
def test_too_many_handles_raises(
    own: tuple[int, int], requested: tuple[int, int]
) -> None:
    fs_id = _add_filesystem("fs_full", *own)
    filesystem.request_open_handles(fs_id, files=requested[0], dirs=requested[1])
    with pytest.raises(EsphomeError, match="fs_full"):
        asyncio.run(filesystem._emit_pool_sizes())
