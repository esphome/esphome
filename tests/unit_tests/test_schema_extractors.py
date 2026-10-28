"""Tests for esphome.schema_extractors."""

from collections.abc import Callable, Iterator
from typing import Any

import pytest

from esphome import schema_extractors as se


@pytest.fixture
def extraction_enabled() -> Iterator[None]:
    """Enable schema extraction and restore the module state afterwards."""
    stores = (
        se.extended_schemas,
        se.list_schemas,
        se.registry_schemas,
        se.hidden_schemas,
        se.typed_schemas,
    )
    saved = [dict(store) for store in stores]
    previous = se.EnableSchemaExtraction
    se.EnableSchemaExtraction = True
    try:
        yield
    finally:
        se.EnableSchemaExtraction = previous
        for store, old in zip(stores, saved, strict=True):
            store.clear()
            store.update(old)


def _validator(value: Any) -> Any:
    return value


def test_decorators_are_passthrough_when_disabled(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(se, "EnableSchemaExtraction", False)
    assert se.schema_extractor("name")(_validator) is _validator
    assert se.schema_extractor_registry(object())(_validator) is _validator
    assert se.schema_extractor_extended(_validator) is _validator
    assert se.schema_extractor_list(_validator) is _validator
    assert se.schema_extractor_typed(_validator) is _validator


@pytest.mark.usefixtures("extraction_enabled")
def test_schema_extractor_records_hidden_schema() -> None:
    assert se.schema_extractor("my_validator")(_validator) is _validator
    assert se.hidden_schemas[repr(_validator)] == "my_validator"


@pytest.mark.usefixtures("extraction_enabled")
def test_schema_extractor_registry_records_registry() -> None:
    registry = object()
    assert se.schema_extractor_registry(registry)(_validator) is _validator
    assert se.registry_schemas[repr(_validator)] is registry


@pytest.mark.parametrize(
    ("decorator", "store_name"),
    [
        (se.schema_extractor_extended, "extended_schemas"),
        (se.schema_extractor_list, "list_schemas"),
    ],
)
@pytest.mark.usefixtures("extraction_enabled")
def test_args_recording_decorators(
    decorator: Callable[[Callable[..., Any]], Callable[..., Any]], store_name: str
) -> None:
    def build(*args: Any) -> list[Any]:
        return list(args)

    wrapped = decorator(build)
    result = wrapped(1, 2)

    assert result == [1, 2]
    assert getattr(se, store_name)[repr(result)] == (1, 2)


@pytest.mark.usefixtures("extraction_enabled")
def test_schema_extractor_typed_records_args_and_kwargs() -> None:
    def build(*args: Any, **kwargs: Any) -> list[Any]:
        return [args, kwargs]

    result = se.schema_extractor_typed(build)(1, key="x")

    assert result == [(1,), {"key": "x"}]
    assert se.typed_schemas[repr(result)] == ((1,), {"key": "x"})
