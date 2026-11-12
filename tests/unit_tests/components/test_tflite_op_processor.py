"""Tests for the tflite_op_processor operator manifest helpers."""

from collections.abc import Callable, Sequence
from functools import partial
from pathlib import Path
import struct

import pytest

from esphome.components import tflite_op_processor
from esphome.components.tflite_op_processor import CONF_OPERATORS
from esphome.components.tflite_op_processor.builtin_codes import BUILTIN_OPERATOR_NAMES
from esphome.components.tflite_op_processor.model_reader import (
    ModelReadError,
    estimate_tensor_arena,
    read_model_operators,
)
from esphome.components.tflite_op_processor.op_manifest import (
    OpTable,
    build_selective_op_defines,
    extract_model_ops,
    parse_op_table,
)
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError


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
    operator_codes: Sequence[tuple[int | None, int | None]],
    *,
    subgraph: bool = True,
    tensors: Sequence[tuple[Sequence[int], int, int]] = (),
    buffers: Sequence[bytes | None] = (),
    operators: Sequence[tuple[Sequence[int], Sequence[int]]] = (),
) -> bytes:
    """Build a minimal .tflite model.

    ``operator_codes`` holds (deprecated_builtin_code, builtin_code) per entry,
    with None for a field the writer leaves out. ``tensors`` holds (shape,
    TensorType, buffer index) per tensor, ``buffers`` the data of each buffer
    (None for an empty one) and ``operators`` the (inputs, outputs) per operator.
    """
    writer = _FlatBufferWriter()

    def write_opcode(deprecated: int | None, builtin: int | None) -> int:
        fields: dict[int, int] = {}
        if deprecated is not None:
            fields[0] = deprecated
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
            **({2: partial(writer.table_vector, [write_subgraph])} if subgraph else {}),
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
def test_schema_rejects_an_unavailable_operator(operator: str) -> None:
    """A typo must be a config error, not a traceback from code generation."""
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
