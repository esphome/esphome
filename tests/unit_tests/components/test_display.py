"""Tests for display.add_writer and display.has_writer."""

from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome import codegen as cg
from esphome.components import display
from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.const import CONF_AUTO_CLEAR_ENABLED, CONF_LAMBDA, CONF_PAGES


@pytest.mark.parametrize(
    ("kwargs", "ref"),
    [({}, display.DisplayRef), ({"ref": "MyDisplayRef"}, "MyDisplayRef")],
)
@pytest.mark.asyncio
async def test_add_writer_sets_the_lambda(kwargs: dict, ref: object) -> None:
    var = MagicMock(name="var")
    lambda_config = MagicMock(name="lambda")
    with (
        patch.object(cg, "process_lambda", AsyncMock(return_value="fn")) as process,
        patch.object(cg, "add") as add,
    ):
        await display.add_writer(var, {CONF_LAMBDA: lambda_config}, **kwargs)

    process.assert_awaited_once_with(lambda_config, [(ref, "it")], return_type=cg.void)
    var.set_writer.assert_called_once_with("fn")
    add.assert_called_once_with(var.set_writer.return_value)


@pytest.mark.asyncio
async def test_add_writer_without_lambda_does_nothing() -> None:
    var = MagicMock(name="var")
    with (
        patch.object(cg, "process_lambda", AsyncMock()) as process,
        patch.object(cg, "add") as add,
    ):
        await display.add_writer(var, {})

    process.assert_not_awaited()
    add.assert_not_called()


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        ({}, False),
        ({CONF_AUTO_CLEAR_ENABLED: True}, True),
        ({CONF_AUTO_CLEAR_ENABLED: False}, False),
        ({CONF_AUTO_CLEAR_ENABLED: "unspecified"}, False),
        ({CONF_PAGES: []}, True),
        ({CONF_LAMBDA: "x"}, True),
        ({CONF_SHOW_TEST_CARD: True}, True),
        ({CONF_SHOW_TEST_CARD: False}, False),
    ],
)
def test_has_writer(config: dict, expected: bool) -> None:
    assert display.has_writer(config) is expected
