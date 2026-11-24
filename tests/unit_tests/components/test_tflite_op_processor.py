"""Tests for the tflite_op_processor operator manifest helpers."""

import asyncio
from collections.abc import Callable, Sequence
from functools import partial
from pathlib import Path
import struct
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import tflite_op_processor
from esphome.components.tflite_model_io.builtin_codes import BUILTIN_OPERATOR_NAMES
from esphome.components.tflite_model_io.model_reader import (
    ModelReadError,
    estimate_tensor_arena,
    read_model_input,
    read_model_io,
    read_model_operators,
)
from esphome.components.tflite_op_processor import CONF_OPERATORS
from esphome.components.tflite_op_processor.op_manifest import (
    OpTable,
    build_selective_op_defines,
    extract_model_ops,
    parse_op_table,
)
import esphome.config_validation as cv
from esphome.core import CORE, Define, EsphomeError


class _FlatBufferWriter:
    """Writes the few flatbuffer objects the model reader tests need.

    Objects are appended in file order, so an offset always points forward, as
    the format requires. A table field holds an int for a scalar or a callable
    that writes a child object and returns its position.
    """

    def __init__(self) -> None:
        # Root offset and file identifier, patched once the root table exists.
        self.data = bytearray(b"\x00\x00\x00\x00TFL3")

    def _pad(self, alignment: int) -> None:
        while len(self.data) % alignment:
            self.data.append(0)

    def _patch(self, position: int, value: int) -> None:
        self.data[position : position + 4] = struct.pack("<I", value)

    def table(self, fields: dict[int, int | Callable[[], int]]) -> int:
        """Write a table and return its position. Fields are 4 bytes each."""
        count = max(fields) + 1 if fields else 0
        self._pad(2)
        vtable = len(self.data)
        self.data += struct.pack("<HH", 4 + 2 * count, 4 + 4 * count)
        for field_id in range(count):
            offset = 4 + 4 * field_id if field_id in fields else 0
            self.data += struct.pack("<H", offset)
        self._pad(4)
        table = len(self.data)
        self.data += struct.pack("<i", table - vtable) + bytes(4 * count)
        for field_id, value in fields.items():
            position = table + 4 + 4 * field_id
            if callable(value):
                self._patch(position, value() - position)
            else:
                self.data[position : position + 4] = struct.pack("<i", value)
        return table

    def i32_vector(self, values: Sequence[int]) -> int:
        self._pad(4)
        position = len(self.data)
        self.data += struct.pack("<I", len(values))
        self.data += struct.pack(f"<{len(values)}i", *values)
        return position

    def u8_vector(self, values: bytes) -> int:
        position = len(self.data)
        self.data += struct.pack("<I", len(values)) + values
        return position

    def string(self, value: str | bytes) -> int:
        """Write a length-prefixed string and return its position."""
        self._pad(4)
        position = len(self.data)
        encoded = value.encode("utf-8") if isinstance(value, str) else value
        self.data += struct.pack("<I", len(encoded)) + encoded + b"\x00"
        return position

    def table_vector(self, build: Sequence[Callable[[], int]]) -> int:
        self._pad(4)
        position = len(self.data)
        self.data += struct.pack("<I", len(build))
        elements = len(self.data)
        self.data += bytes(4 * len(build))
        for index, write in enumerate(build):
            element = elements + 4 * index
            self._patch(element, write() - element)
        return position

    def finish(self, root: int) -> bytes:
        self._patch(0, root)
        return bytes(self.data)


def write_model(
    operator_codes: Sequence[tuple[int | str | bytes | None, ...]],
    *,
    subgraph: bool = True,
    extra_subgraphs: int = 0,
    inputs: Sequence[int] = (),
    outputs: Sequence[int] = (),
    tensors: Sequence[tuple[Sequence[int], int, int]] = (),
    buffers: Sequence[bytes | None] = (),
    operators: Sequence[tuple[Sequence[int], Sequence[int]]] = (),
) -> bytes:
    """Build a minimal .tflite model.

    ``operator_codes`` holds (deprecated_builtin_code, builtin_code) per entry, with
    an optional custom_code name third and None for a field the writer leaves out.
    ``inputs`` holds the tensor
    indexes of the subgraph's inputs, ``tensors`` (shape, TensorType, buffer
    index) per tensor, ``buffers`` the data of each buffer (None for an empty one)
    and ``operators`` the (inputs, outputs) per operator. ``extra_subgraphs`` adds
    copies of the subgraph, for the models the reader has to reject.
    """
    writer = _FlatBufferWriter()

    def write_opcode(
        deprecated: int | None, builtin: int | None, custom: str | None = None
    ) -> int:
        fields: dict[int, int | Callable[[], int]] = {}
        if deprecated is not None:
            fields[0] = deprecated
        if custom is not None:
            fields[1] = partial(writer.string, custom)
        if builtin is not None:
            fields[3] = builtin
        return writer.table(fields)

    def write_tensor(shape: Sequence[int], tensor_type: int, buffer: int) -> int:
        return writer.table(
            {0: partial(writer.i32_vector, shape), 1: tensor_type, 2: buffer}
        )

    def write_operator(inputs: Sequence[int], outputs: Sequence[int]) -> int:
        return writer.table(
            {
                1: partial(writer.i32_vector, inputs),
                2: partial(writer.i32_vector, outputs),
            }
        )

    def write_buffer(data: bytes | None) -> int:
        if data is None:
            return writer.table({})
        return writer.table({0: partial(writer.u8_vector, data)})

    def write_subgraph() -> int:
        return writer.table(
            {
                0: partial(
                    writer.table_vector,
                    [partial(write_tensor, *tensor) for tensor in tensors],
                ),
                **({1: partial(writer.i32_vector, inputs)} if inputs else {}),
                **({2: partial(writer.i32_vector, outputs)} if outputs else {}),
                3: partial(
                    writer.table_vector,
                    [partial(write_operator, *operator) for operator in operators],
                ),
            }
        )

    model = writer.table(
        {
            1: partial(
                writer.table_vector,
                [partial(write_opcode, *code) for code in operator_codes],
            ),
            **(
                {
                    2: partial(
                        writer.table_vector,
                        [write_subgraph] * (1 + extra_subgraphs),
                    )
                }
                if subgraph
                else {}
            ),
            4: partial(
                writer.table_vector,
                [partial(write_buffer, data) for data in buffers],
            ),
        }
    )
    return writer.finish(model)


def write_model_file(tmp_path: Path, data: bytes, name: str = "model.tflite") -> Path:
    path = tmp_path / name
    path.write_bytes(data)
    return path


def test_parse_op_table_returns_all_three_sets() -> None:
    table = parse_op_table()

    assert isinstance(table, OpTable)
    assert "SOFTMAX" in table.available
    assert "REDUCE_MIN" in table.conditional
    assert "POW" in table.unavailable


def test_supported_ops_include_conditional_ops() -> None:
    table = parse_op_table()

    assert "REDUCE_MIN" in table.supported
    assert "DYNAMIC_UPDATE_SLICE" in table.supported
    assert "POW" not in table.supported


def test_operator_sets_are_disjoint() -> None:
    table = parse_op_table()

    assert not table.available & table.conditional
    assert not table.available & table.unavailable
    assert not table.conditional & table.unavailable


def test_suffixes_cover_every_table_entry() -> None:
    table = parse_op_table()

    assert table.suffixes == table.available | table.conditional | table.unavailable
    assert len(table.suffixes) == len(table.available) + len(table.conditional) + len(
        table.unavailable
    )


def test_selective_defines_flag_only_the_requested_ops() -> None:
    defines = build_selective_op_defines({"CONV_2D", "SOFTMAX"})

    # Unavailable operators read no flag, so they get none.
    assert set(defines) == parse_op_table().supported
    assert {name for name, value in defines.items() if value} == {
        "CONV_2D",
        "SOFTMAX",
    }
    assert set(defines.values()) <= {0, 1}


def test_selective_defines_without_ops_compile_every_kernel_out() -> None:
    defines = build_selective_op_defines([])

    assert defines
    assert not any(defines.values())


def test_selective_defines_reject_unknown_ops() -> None:
    with pytest.raises(ValueError, match="FROBNICATE"):
        build_selective_op_defines({"SOFTMAX", "FROBNICATE"})


def test_selective_defines_reject_unavailable_ops() -> None:
    """A kernel the table marks unavailable cannot be flagged into the build."""
    with pytest.raises(ValueError, match="POW"):
        build_selective_op_defines({"SOFTMAX", "POW"})


def test_schema_accepts_an_operator_list() -> None:
    config = tflite_op_processor.CONFIG_SCHEMA({CONF_OPERATORS: ["SOFTMAX"]})

    assert config[CONF_OPERATORS] == ["SOFTMAX"]


def test_schema_rejects_an_empty_operator_list() -> None:
    with pytest.raises(cv.Invalid):
        tflite_op_processor.CONFIG_SCHEMA({CONF_OPERATORS: []})


@pytest.mark.parametrize("operator", ["softmax", "POW"])
def test_schema_rejects_an_operator_the_table_cannot_register(
    operator: str,
) -> None:
    """A typo or an unavailable kernel is a config error, not a traceback."""
    with pytest.raises(cv.Invalid, match=operator):
        tflite_op_processor.CONFIG_SCHEMA({CONF_OPERATORS: [operator]})


def test_emit_selective_op_defines_reports_an_unavailable_op(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A programmatic caller runs during code generation, so it gets a clean error."""
    monkeypatch.setattr(CORE, "defines", set())

    with pytest.raises(EsphomeError, match="POW"):
        tflite_op_processor.emit_selective_op_defines(["POW"])


def test_emit_selective_op_defines_keeps_the_kernels_of_other_components(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A second operator list widens the flags instead of replacing them."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    tflite_op_processor.emit_selective_op_defines(["CONV_2D"])

    flags = {
        define.name: str(define.value)
        for define in CORE.defines
        if define.name.startswith("TFLM_OP_ENABLED_")
    }
    names = [
        define.name
        for define in CORE.defines
        if define.name.startswith("TFLM_OP_ENABLED_")
    ]
    assert len(names) == len(set(names))
    assert flags["TFLM_OP_ENABLED_SOFTMAX"] == "1"
    assert flags["TFLM_OP_ENABLED_CONV_2D"] == "1"
    assert flags["TFLM_OP_ENABLED_ADD"] == "0"


def test_emit_selective_op_defines_validates_after_other_components(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Flags emitted earlier do not skip validation of a later operator list."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    with pytest.raises(EsphomeError, match="FROBNICATE"):
        tflite_op_processor.emit_selective_op_defines(["FROBNICATE"])


def test_emit_all_ops_cancels_selective_flags(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A component that cannot name its operators keeps every kernel."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    tflite_op_processor.emit_all_ops()

    names = {define.name for define in CORE.defines}
    assert "USE_TFLITE_MICRO_SELECTIVE_OPS" not in names
    assert not [name for name in names if name.startswith("TFLM_OP_ENABLED_")]


def test_emit_all_ops_wins_over_a_later_selective_request(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The result must not depend on the order components run in."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_all_ops()

    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    names = {define.name for define in CORE.defines}
    assert "USE_TFLITE_MICRO_SELECTIVE_OPS" not in names
    assert not [name for name in names if name.startswith("TFLM_OP_ENABLED_")]


def test_emit_selective_op_defines_still_validates_after_emit_all_ops(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The full kernel set must not hide an operator the table cannot register."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_all_ops()

    with pytest.raises(EsphomeError, match="POW"):
        tflite_op_processor.emit_selective_op_defines(["POW"])


def test_unknown_table_line_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "TFLM_OP_MAYBE(FROBNICATE)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unrecognised operator table line"):
        parse_op_table(table)


def test_inline_comments_are_allowed(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "// operator table\n"
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)  // classification\n"
        "TFLM_OP_UNAVAILABLE(POW)\n"
        "#endif  // TFLM_OPERATORS_ACTIVE\n",
        encoding="utf-8",
    )

    parsed = parse_op_table(table)

    assert parsed.available == frozenset({"SOFTMAX"})
    assert parsed.conditional == frozenset()
    assert parsed.unavailable == frozenset({"POW"})


def test_table_without_active_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text("// no operator table here\n", encoding="utf-8")

    with pytest.raises(ValueError, match="TFLM_OPERATORS_ACTIVE"):
        parse_op_table(table)


def test_nested_conditional_is_rejected(tmp_path: Path) -> None:
    """Entries inside a conditional would be reported even when it is false."""
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "#if TFLM_VERSION >= 2\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "#endif\n"
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unsupported preprocessor directive"):
        parse_op_table(table)


def test_operator_entry_outside_the_block_is_rejected(tmp_path: Path) -> None:
    """The C++ table would not expand it, so ignoring it would hide the drift."""
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="operator entry outside"):
        parse_op_table(table)


def test_unsupported_directive_inside_the_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        '#include "other_ops.h"\n'
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unsupported preprocessor directive"):
        parse_op_table(table)


def test_unterminated_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\nTFLM_OP_AVAILABLE(ADD, AddAdd)\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unterminated"):
        parse_op_table(table)


def test_extract_model_ops(tmp_path: Path) -> None:
    model = write_model_file(tmp_path, write_model([(0, 0), (0, 25), (0, 89)]))

    assert extract_model_ops(model) == {"ADD", "SOFTMAX", "REDUCE_MIN"}


def test_extract_model_ops_ignores_a_report_next_to_the_model(tmp_path: Path) -> None:
    """The model is authoritative, so a stale or edited report cannot change it."""
    model = write_model_file(tmp_path, write_model([(0, 25)]))
    model.with_suffix(".txt").write_text(
        "Unique operator types: 1\n  CONV_2D: 1\n\n", encoding="utf-8"
    )

    assert extract_model_ops(model) == {"SOFTMAX"}


@pytest.mark.parametrize(
    ("deprecated", "builtin", "expected"),
    (
        (25, None, "SOFTMAX"),  # older schema: only the byte field is written
        (None, 25, "SOFTMAX"),  # newer schema: only the extended field is written
        (25, 0, "SOFTMAX"),  # builtin_code left at its default
        (127, 151, "DYNAMIC_UPDATE_SLICE"),  # past 127, placeholder in the byte field
    ),
)
def test_read_model_operators_resolves_the_builtin_code(
    tmp_path: Path, deprecated: int | None, builtin: int | None, expected: str
) -> None:
    """Both schema generations must resolve, as TFLM's GetBuiltinCode() does."""
    model = write_model_file(tmp_path, write_model([(deprecated, builtin)]))

    assert read_model_operators(model) == {expected}


def test_read_model_operators_names_a_code_the_table_does_not_have(
    tmp_path: Path,
) -> None:
    """A model from a newer schema is reported by number instead of being dropped."""
    model = write_model_file(tmp_path, write_model([(None, 300)]))

    assert read_model_operators(model) == {"BUILTIN_300"}


def test_read_model_operators_requires_operator_codes(tmp_path: Path) -> None:
    model = write_model_file(tmp_path, write_model([]))

    with pytest.raises(ModelReadError, match="no operator codes"):
        read_model_operators(model)


def test_read_model_operators_keeps_each_custom_name(tmp_path: Path) -> None:
    """Two equal-length names must not collapse into one resolver slot."""
    model = write_model_file(
        tmp_path, write_model([(None, 32, "Foo"), (None, 32, "Bar")])
    )

    assert read_model_operators(model) == frozenset({"CUSTOM:Foo", "CUSTOM:Bar"})


def test_read_model_operators_requires_a_custom_name(tmp_path: Path) -> None:
    """A CUSTOM opcode without its string is a malformed model, not a shared slot."""
    model = write_model_file(tmp_path, write_model([(None, 32)]))

    with pytest.raises(ModelReadError, match="custom_code"):
        read_model_operators(model)


def test_read_model_operators_rejects_a_custom_name_that_is_not_utf8(
    tmp_path: Path,
) -> None:
    """Invalid bytes must not become replacement characters in a resolver name."""
    model = write_model_file(tmp_path, write_model([(None, 32, b"\xff\xfe")]))

    with pytest.raises(ModelReadError, match="UTF-8"):
        read_model_operators(model)


def test_read_model_operators_rejects_a_file_that_is_not_a_model(
    tmp_path: Path,
) -> None:
    model = write_model_file(tmp_path, b"not a tflite model at all")

    with pytest.raises(ModelReadError, match="TFL3"):
        read_model_operators(model)


def test_read_model_operators_rejects_a_truncated_model(tmp_path: Path) -> None:
    data = write_model([(0, 25)])
    model = write_model_file(tmp_path, data[:12])

    with pytest.raises(ModelReadError, match="truncated or corrupt"):
        read_model_operators(model)


def test_read_model_operators_rejects_a_missing_file(tmp_path: Path) -> None:
    with pytest.raises(ModelReadError, match="missing.tflite"):
        read_model_operators(tmp_path / "missing.tflite")


@pytest.mark.parametrize(
    ("shape", "type_code", "expected_type"),
    [
        ((1, 32, 20, 3), 3, "uint8"),
        ((1, 49, 40), 9, "int8"),
        ((1, 8, 8, 1), 0, "float32"),
    ],
)
def test_read_model_input_reports_the_tensor_as_stored(
    tmp_path: Path, shape: tuple[int, ...], type_code: int, expected_type: str
) -> None:
    """The shape is reported as stored, with the type the runtime fills."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0,),
            tensors=((list(shape), type_code, 0),),
            buffers=(None,),
        ),
    )

    model_input = read_model_input(model)

    assert model_input.shape == shape
    assert model_input.type == expected_type


def test_read_model_input_requires_an_input_tensor(tmp_path: Path) -> None:
    model = write_model_file(tmp_path, write_model([(None, 9)]))

    with pytest.raises(ModelReadError, match="no input tensor"):
        read_model_input(model)


def test_read_model_io_lists_every_input_and_output(tmp_path: Path) -> None:
    """A detection or text model has several tensors on both sides."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0, 1),
            outputs=(2, 3),
            tensors=(
                ([1, 32, 20, 3], 3, 0),
                ([1, 2], 2, 0),
                ([1, 4, 4], 0, 0),
                ([1, 1], 0, 0),
            ),
            buffers=(None,),
        ),
    )

    model_io = read_model_io(model)

    assert [spec.shape for spec in model_io.inputs] == [(1, 32, 20, 3), (1, 2)]
    assert [spec.type for spec in model_io.inputs] == ["uint8", "int32"]
    assert [spec.shape for spec in model_io.outputs] == [(1, 4, 4), (1, 1)]
    assert [spec.type for spec in model_io.outputs] == ["float32", "float32"]


def test_read_model_io_rejects_a_tensor_index_outside_the_graph(
    tmp_path: Path,
) -> None:
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(4,),
            tensors=(([1, 8], 3, 0),),
            buffers=(None,),
        ),
    )

    with pytest.raises(ModelReadError, match="tensor 4"):
        read_model_io(model)


def test_read_model_input_rejects_a_type_the_runtime_cannot_fill(
    tmp_path: Path,
) -> None:
    """An int32 input has no matching input_type, so the build must say so."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0,),
            tensors=(([1, 8, 8, 1], 2, 0),),
            buffers=(None,),
        ),
    )

    with pytest.raises(ModelReadError, match="not one the runtime can fill"):
        read_model_input(model)


def test_estimate_tensor_arena_excludes_constants(tmp_path: Path) -> None:
    """A tensor whose buffer holds data is read from flash, not from the arena."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            tensors=(
                ([1, 4], 0, 1),  # input, FLOAT32, empty buffer
                ([4], 0, 2),  # weight, FLOAT32, buffer with data
                ([1, 4], 0, 3),  # output, FLOAT32, empty buffer
            ),
            buffers=(None, None, bytes(16), None),
            operators=(([0, 1], [2]),),
        ),
    )

    estimate = estimate_tensor_arena(model)

    assert estimate.tensor_count == 3
    # The 16-byte input and the 16-byte output are live together; the weight is not.
    assert estimate.peak_activation_bytes == 32
    assert estimate.bytes == 1024


def test_estimate_tensor_arena_rejects_a_buffer_the_model_lacks(
    tmp_path: Path,
) -> None:
    """An index past the buffer list is corrupt, not a tensor that lives in the arena."""
    model = write_model_file(
        tmp_path,
        write_model([(None, 9)], tensors=(([1, 4], 0, 5),), buffers=(None,)),
    )

    with pytest.raises(ModelReadError, match="buffer 5"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_rejects_a_subgraph_without_operators(
    tmp_path: Path,
) -> None:
    """Nothing would be live, so a zero peak would understate the arena."""
    model = write_model_file(
        tmp_path,
        write_model([(None, 9)], tensors=(([1, 4], 0, 1),), buffers=(None, None)),
    )

    with pytest.raises(ModelReadError, match="no operators"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_names_the_model_in_its_errors(
    tmp_path: Path,
) -> None:
    """A configuration with several models must say which one cannot be estimated."""
    model = write_model_file(tmp_path, write_model([(None, 9)]))

    with pytest.raises(ModelReadError) as raised:
        estimate_tensor_arena(model)

    assert str(model) in str(raised.value)


def test_estimate_tensor_arena_ignores_an_omitted_optional_tensor(
    tmp_path: Path,
) -> None:
    """TFLite writes -1 for an omitted optional input, such as a bias."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            tensors=(([1, 4], 0, 1), ([1, 4], 0, 2)),
            buffers=(None, None, None),
            operators=(([0, -1], [1]),),
        ),
    )

    estimate = estimate_tensor_arena(model)

    assert estimate.tensor_count == 2


def test_estimate_tensor_arena_rejects_a_model_with_several_subgraphs(
    tmp_path: Path,
) -> None:
    """The estimate covers one subgraph, so a control-flow model must be rejected."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            extra_subgraphs=1,
            tensors=(([1, 4], 0, 1),),
            buffers=(None, None),
            operators=(([0], [0]),),
        ),
    )

    with pytest.raises(ModelReadError, match="2 subgraphs"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_finds_the_peak_of_the_graph(tmp_path: Path) -> None:
    """Only the tensors live at the same operator are counted together."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            tensors=(
                ([1, 4], 0, 1),  # 0 input
                ([4], 0, 2),  # 1 weight, constant
                ([1, 4], 0, 3),  # 2
                ([1, 4], 0, 4),  # 3
                ([1, 4], 0, 5),  # 4
                ([1, 4], 0, 6),  # 5
            ),
            buffers=(None, None, bytes(16), None, None, None, None),
            operators=(
                ([0, 1], [2, 3]),
                ([2], [4]),
                ([3], [5]),
            ),
        ),
    )

    estimate = estimate_tensor_arena(model)

    # Tensors 0, 2 and 3 are live at the first operator, two per operator after it.
    assert estimate.peak_activation_bytes == 48


def test_estimate_tensor_arena_sizes_a_tensor_by_its_type(tmp_path: Path) -> None:
    """INT8 elements are one byte, not the four of the default FLOAT32."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            tensors=(
                ([1, 8], 9, 1),  # input, INT8
                ([1, 8], 9, 2),  # output, INT8
            ),
            buffers=(None, None, None),
            operators=(([0], [1]),),
        ),
    )

    assert estimate_tensor_arena(model).peak_activation_bytes == 16


def test_estimate_tensor_arena_requires_a_subgraph(tmp_path: Path) -> None:
    model = write_model_file(tmp_path, write_model([(None, 9)], subgraph=False))

    with pytest.raises(ModelReadError, match="no subgraph"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_rejects_an_unknown_tensor_type(
    tmp_path: Path,
) -> None:
    """An element size the reader does not know would under-estimate the arena."""
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0,),
            tensors=(([1, 8, 8, 3], 99, 0),),
            buffers=(None,),
        ),
    )

    with pytest.raises(ModelReadError, match="tensor type 99"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_rejects_a_negative_dimension(
    tmp_path: Path,
) -> None:
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0,),
            tensors=(([1, -4, 3], 3, 0),),
            buffers=(None,),
        ),
    )

    with pytest.raises(ModelReadError, match="negative"):
        estimate_tensor_arena(model)


def test_estimate_tensor_arena_rejects_an_operator_outside_the_graph(
    tmp_path: Path,
) -> None:
    model = write_model_file(
        tmp_path,
        write_model(
            [(None, 9)],
            inputs=(0,),
            tensors=(([1, 8, 8, 3], 3, 0),),
            buffers=(None,),
            operators=(([0], [7]),),
        ),
    )

    with pytest.raises(ModelReadError, match="tensor 7"):
        estimate_tensor_arena(model)


def test_every_table_operator_has_a_builtin_code() -> None:
    """A suffix the code table cannot name would be reported by number instead."""
    assert not parse_op_table().suffixes - set(BUILTIN_OPERATOR_NAMES.values())


@pytest.mark.parametrize(
    ("code", "name"),
    (
        (0, "ADD"),
        (25, "SOFTMAX"),
        (51, "DELEGATE"),
        (89, "REDUCE_MIN"),
        (114, "QUANTIZE"),
        (126, "BATCH_MATMUL"),
        (127, "PLACEHOLDER_FOR_GREATER_OP_CODES"),
        (128, "CUMSUM"),
        (140, "REDUCE_ALL"),
        (151, "DYNAMIC_UPDATE_SLICE"),
    ),
)
def test_builtin_operator_codes_match_the_schema(code: int, name: str) -> None:
    assert BUILTIN_OPERATOR_NAMES[code] == name


def test_a_larger_operator_count_widens_an_existing_define(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Another component may have sized the resolver already; the larger value wins."""
    monkeypatch.setattr(
        CORE, "defines", {Define(tflite_op_processor.MAX_OPERATORS_DEFINE, 12)}
    )

    tflite_op_processor._emit_max_operators(47)

    values = [
        int(str(define.value))
        for define in CORE.defines
        if define.name == tflite_op_processor.MAX_OPERATORS_DEFINE
    ]
    assert values == [47]


def test_a_smaller_operator_count_keeps_an_existing_define(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(
        CORE, "defines", {Define(tflite_op_processor.MAX_OPERATORS_DEFINE, 60)}
    )

    with patch("esphome.codegen.add_define") as add_define:
        tflite_op_processor._emit_max_operators(47)

    add_define.assert_not_called()


def test_a_non_numeric_operator_define_is_an_error(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Silently keeping a value that cannot be compared would hide the problem."""
    monkeypatch.setattr(
        CORE, "defines", {Define(tflite_op_processor.MAX_OPERATORS_DEFINE, "lots")}
    )

    with pytest.raises(EsphomeError, match="lots"):
        tflite_op_processor._emit_max_operators(47)


def test_the_resolver_is_sized_by_the_widest_model(setup_core: Path) -> None:
    """One resolver slot per distinct operator code, with no margin to spare."""
    with (
        patch.object(
            tflite_op_processor,
            "extract_model_ops",
            side_effect=[{"ADD", "SOFTMAX"}, {"ADD"}],
        ),
        patch.object(tflite_op_processor, "emit_selective_op_defines"),
        patch("esphome.codegen.add_define") as add_define,
    ):
        tflite_op_processor.add_model_operator_defines(
            [Path("a.tflite"), Path("b.tflite")]
        )

    add_define.assert_called_once_with(tflite_op_processor.MAX_OPERATORS_DEFINE, 2)


def test_no_models_keep_the_full_operator_table(setup_core: Path) -> None:
    with (
        patch.object(tflite_op_processor, "emit_all_ops") as emit_all,
        patch.object(
            tflite_op_processor, "emit_selective_op_defines"
        ) as emit_selective,
    ):
        tflite_op_processor.add_model_operator_defines([])

    emit_all.assert_called_once_with()
    emit_selective.assert_not_called()


def test_operator_defines_are_emitted_once(setup_core: Path) -> None:
    with (
        patch.object(tflite_op_processor, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflite_op_processor, "emit_selective_op_defines"),
        patch("esphome.codegen.add_define") as add_define,
    ):
        tflite_op_processor.add_model_operator_defines([Path("a.tflite")])
        tflite_op_processor.add_model_operator_defines([Path("a.tflite")])

    assert [call.args[0] for call in add_define.call_args_list] == [
        tflite_op_processor.MAX_OPERATORS_DEFINE
    ]


def test_validate_model_operators_accepts_a_supported_model(
    setup_core: Path,
) -> None:
    with (
        patch.object(tflite_op_processor, "extract_model_ops", return_value={"ADD"}),
        patch.object(
            tflite_op_processor,
            "parse_op_table",
            return_value=MagicMock(supported={"ADD"}),
        ),
    ):
        tflite_op_processor.validate_model_operators(Path("digit.tflite"))


def test_validate_model_operators_rejects_an_unsupported_operator(
    setup_core: Path,
) -> None:
    with (
        patch.object(tflite_op_processor, "extract_model_ops", return_value={"WHERE"}),
        patch.object(
            tflite_op_processor,
            "parse_op_table",
            return_value=MagicMock(supported={"ADD"}),
        ),
        pytest.raises(cv.Invalid, match="WHERE"),
    ):
        tflite_op_processor.validate_model_operators(Path("digit.tflite"))


def test_validate_model_operators_reports_an_unreadable_model(
    setup_core: Path,
) -> None:
    """A model whose operators cannot be read is a config error, not a load failure."""
    with (
        patch.object(
            tflite_op_processor,
            "extract_model_ops",
            side_effect=ValueError("not a model"),
        ),
        pytest.raises(cv.Invalid, match="not a model"),
    ):
        tflite_op_processor.validate_model_operators(Path("digit.tflite"))


def test_a_pinned_operator_list_sizes_the_resolver(setup_core: Path) -> None:
    """A replacement model loads against the same resolver, so the list widens it."""
    with (
        patch.object(tflite_op_processor, "esp32"),
        patch.object(tflite_op_processor, "emit_selective_op_defines"),
        patch.object(tflite_op_processor, "_emit_max_operators") as emit_max,
    ):
        asyncio.run(
            tflite_op_processor.to_code({CONF_OPERATORS: ["ADD", "MUL", "ADD"]})
        )

    emit_max.assert_called_once_with(2)
