"""Tests for image configuration validation."""

from __future__ import annotations

from collections.abc import Callable
import logging
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

from PIL import Image as PILImage
import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_BYTE_ORDER
from esphome.components.file import image as file_image
from esphome.components.file.image import validate_image_final, write_image
from esphome.components.image import (
    CONF_ALPHA_CHANNEL,
    CONF_INVERT_ALPHA,
    CONF_OPAQUE,
    CONF_TRANSPARENCY,
    PLATFORM_FILE,
    _expand_platform_entry,
    _flatten_legacy_image_config,
    _is_legacy_image_format,
    _is_new_image_format,
    _migrate_legacy_image_config,
    expand_platform_config,
    get_all_image_metadata,
    get_image_metadata,
)
from esphome.const import (
    CONF_DEFAULTS,
    CONF_DITHER,
    CONF_FILE,
    CONF_FILES,
    CONF_ID,
    CONF_PLATFORM,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
)
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {"id": "image_id", "type": "rgb565"},
            r"required key not provided @ data\['file'\]",
            id="missing_file",
        ),
        pytest.param(
            {"file": "image.png", "type": "rgb565"},
            r"required key not provided @ data\['id'\]",
            id="missing_id",
        ),
        pytest.param(
            {"id": "image_id", "file": "image.png"},
            r"required key not provided @ data\['type'\]",
            id="missing_type",
        ),
        pytest.param(
            {"id": "mdi_id", "file": "mdi:weather-##", "type": "rgb565"},
            "Could not parse mdi icon name",
            id="invalid_mdi_icon",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "transparency": "alpha_channel",
            },
            "Image format 'BINARY' cannot have transparency",
            id="binary_with_transparency",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "rgb565",
                "transparency": "chroma_key",
                "invert_alpha": True,
            },
            "No alpha channel to invert",
            id="invert_alpha_without_alpha_channel",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "byte_order": "big_endian",
            },
            "Image format 'BINARY' does not support byte order configuration",
            id="binary_with_byte_order",
        ),
        pytest.param(
            {"id": "image_id", "file": "bad.png", "type": "binary"},
            "File can't be opened as image",
            id="invalid_image_file",
        ),
    ],
)
def test_file_platform_configuration_errors(
    config: Any,
    error_match: str,
) -> None:
    """Invalid single-entry ``platform: file`` configs are rejected."""
    with pytest.raises(cv.Invalid, match=error_match):
        file_image.CONFIG_SCHEMA(config)


def test_file_platform_configuration_success() -> None:
    """A fully-specified ``platform: file`` entry validates and keeps its keys."""
    result = file_image.CONFIG_SCHEMA(
        {
            "id": "image_id",
            "file": "image.png",
            "type": "rgb565",
            "transparency": "chroma_key",
            "byte_order": "little_endian",
            "dither": "FloydSteinberg",
            "resize": "100x100",
            "invert_alpha": False,
        }
    )
    for key in (CONF_TYPE, CONF_ID, CONF_TRANSPARENCY, CONF_RAW_DATA_ID):
        assert key in result, f"Missing key {key} in validated image configuration"


# ---------------------------------------------------------------------------
# Legacy `image:` config migration -- REMOVE these tests after 2027.1.0 together
# with the migration shim in esphome/components/image/__init__.py.
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        pytest.param(
            [{CONF_PLATFORM: "file", "id": "a"}], True, id="new_platform_list"
        ),
        pytest.param([], True, id="empty_list"),
        pytest.param([{"id": "a", "file": "x.png"}], False, id="legacy_bare_list"),
        pytest.param([{CONF_PLATFORM: "file"}, {"id": "a"}], False, id="mixed_list"),
        pytest.param(
            [{CONF_PLATFORM: "file"}, "not-a-dict"], False, id="non_dict_entry"
        ),
        pytest.param({"defaults": {}}, False, id="legacy_dict"),
    ],
)
def test_is_new_image_format(config: object, expected: bool) -> None:
    assert _is_new_image_format(config) is expected


def test_flatten_bare_list_filters_non_dicts() -> None:
    out = _flatten_legacy_image_config(
        [{"id": "a", "file": "x.png", "type": "binary"}, "not-a-dict"]
    )
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]


def test_flatten_non_dict_non_list_yields_nothing() -> None:
    assert _flatten_legacy_image_config("a string") == []


def test_flatten_single_dict_with_id() -> None:
    config = {"id": "a", "file": "x.png", "type": "binary"}
    assert _flatten_legacy_image_config(config) == [config]


def test_flatten_single_dict_with_file_only() -> None:
    config = {"file": "x.png", "type": "binary"}
    assert _flatten_legacy_image_config(config) == [config]


def test_flatten_defaults_images_list() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"type": "rgb565", "byte_order": "little_endian"},
            "images": [{"id": "a", "file": "x.png"}],
        }
    )
    assert out == [
        {
            "id": "a",
            "file": "x.png",
            "type": "rgb565",
            "byte_order": "little_endian",
        }
    ]


def test_flatten_defaults_images_single_dict() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"type": "rgb565"},
            "images": {"id": "a", "file": "x.png"},
        }
    )
    assert out == [{"id": "a", "file": "x.png", "type": "rgb565"}]


def test_flatten_type_grouped_list() -> None:
    out = _flatten_legacy_image_config({"binary": [{"id": "a", "file": "x.png"}]})
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]


def test_flatten_type_grouped_transparency_list() -> None:
    out = _flatten_legacy_image_config(
        {"rgb565": {"alpha_channel": [{"id": "a", "file": "x.png"}]}}
    )
    assert out == [
        {
            "id": "a",
            "file": "x.png",
            "type": "rgb565",
            "transparency": "alpha_channel",
        }
    ]


def test_flatten_type_grouped_transparency_single_dict() -> None:
    out = _flatten_legacy_image_config(
        {"rgb565": {"alpha_channel": {"id": "a", "file": "x.png"}}}
    )
    assert out == [
        {
            "id": "a",
            "file": "x.png",
            "type": "rgb565",
            "transparency": "alpha_channel",
        }
    ]


def test_flatten_type_grouped_dict_without_transparency() -> None:
    out = _flatten_legacy_image_config({"binary": {"id": "a", "file": "x.png"}})
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]


def test_flatten_drops_byte_order_for_non_endian_type() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"byte_order": "little_endian"},
            "binary": [{"id": "a", "file": "x.png"}],
        }
    )
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]
    assert CONF_BYTE_ORDER not in out[0]


def test_flatten_keeps_byte_order_for_endian_type() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"byte_order": "little_endian"},
            "rgb565": [{"id": "a", "file": "x.png"}],
        }
    )
    assert out[0][CONF_BYTE_ORDER] == "little_endian"


def test_flatten_drops_byte_order_written_directly_on_legacy_entry() -> None:
    """The legacy flattener drops an incompatible byte_order even when written directly on the entry."""
    out = _flatten_legacy_image_config(
        {"binary": [{"id": "a", "file": "x.png", "byte_order": "little_endian"}]}
    )
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]
    assert CONF_BYTE_ORDER not in out[0]


def test_flatten_skips_meta_and_unknown_keys() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"type": "binary"},
            "images": [],
            "not_a_type": [{"id": "a", "file": "x.png"}],
        }
    )
    assert out == []


def test_flatten_images_list_skips_non_dict_entries() -> None:
    out = _flatten_legacy_image_config(
        {
            "defaults": {"type": "binary"},
            "images": [{"id": "a", "file": "x.png"}, "not-a-dict"],
        }
    )
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]


def test_flatten_type_grouped_list_skips_non_dict_entries() -> None:
    out = _flatten_legacy_image_config(
        {"binary": [{"id": "a", "file": "x.png"}, "not-a-dict"]}
    )
    assert out == [{"id": "a", "file": "x.png", "type": "binary"}]


def test_flatten_type_grouped_scalar_value_is_ignored() -> None:
    # A known type key whose value is neither a list nor a dict yields nothing.
    assert _flatten_legacy_image_config({"binary": "not-a-list-or-dict"}) == []


def test_flatten_type_grouped_transparency_skips_non_dict_entries() -> None:
    out = _flatten_legacy_image_config(
        {"rgb565": {"alpha_channel": [{"id": "a", "file": "x.png"}, "not-a-dict"]}}
    )
    assert out == [
        {
            "id": "a",
            "file": "x.png",
            "type": "rgb565",
            "transparency": "alpha_channel",
        }
    ]


def test_migrate_returns_none_for_new_format() -> None:
    assert _migrate_legacy_image_config([{CONF_PLATFORM: "file", "id": "a"}]) is None


def test_migrate_legacy_warns_and_prepends_platform(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        out = _migrate_legacy_image_config(
            [{"id": "a", "file": "x.png", "type": "binary"}]
        )
    assert out == [
        {CONF_PLATFORM: PLATFORM_FILE, "id": "a", "file": "x.png", "type": "binary"}
    ]
    assert "deprecated" in caplog.text
    assert f"platform: {PLATFORM_FILE}" in caplog.text


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        # Recognised legacy shapes -> migrate.
        pytest.param([{"id": "a", "file": "x.png"}], True, id="bare_list_of_dicts"),
        pytest.param({"id": "a", "file": "x.png"}, True, id="single_image_dict"),
        pytest.param({"file": "x.png"}, True, id="single_dict_file_only"),
        pytest.param({"defaults": {}, "images": []}, True, id="defaults_images"),
        pytest.param({"rgb565": [{"id": "a"}]}, True, id="type_grouped"),
        # Shapes the legacy schema never accepted -> not migrated.
        pytest.param([], False, id="empty_list"),
        pytest.param(["bad"], False, id="list_with_non_dict"),
        pytest.param([{"id": "a"}, "bad"], False, id="list_mixed_dict_and_non_dict"),
        pytest.param(
            [{CONF_PLATFORM: "file", "id": "a"}], False, id="already_platform_tagged"
        ),
        pytest.param({"foo": 1}, False, id="dict_unknown_keys"),
        pytest.param("a string", False, id="scalar"),
        # A `platform:`-tagged dict is the new format written without list brackets.
        pytest.param(
            {CONF_PLATFORM: "file", "id": "a", "file": "x.png"},
            False,
            id="platform_tagged_flat_dict",
        ),
        pytest.param(
            {
                CONF_PLATFORM: "file",
                "defaults": {"type": "rgb565"},
                "files": [{"id": "a", "file": "x.png"}],
            },
            False,
            id="platform_tagged_defaults_files_dict",
        ),
        # `files:` without `platform:` is not legacy either -- the flattener has no branch for it.
        pytest.param(
            {
                "defaults": {"type": "rgb565"},
                "files": [{"id": "a", "file": "x.png"}],
            },
            False,
            id="defaults_files_dict_without_platform",
        ),
        # Same as above in a list -- without this exclusion it would be silently
        # migrated to a hard-coded `platform: file` instead of raising the error.
        pytest.param(
            [
                {
                    "defaults": {"type": "rgb565"},
                    "files": [{"id": "a", "file": "x.png"}],
                }
            ],
            False,
            id="defaults_files_list_entry_without_platform",
        ),
    ],
)
def test_is_legacy_image_format(config: object, expected: bool) -> None:
    assert _is_legacy_image_format(config) is expected


@pytest.mark.parametrize(
    "config",
    [
        pytest.param(["bad"], id="list_with_non_dict"),
        pytest.param([{"id": "a"}, "bad"], id="list_mixed"),
        pytest.param({"foo": 1}, id="dict_unknown_keys"),
    ],
)
def test_migrate_returns_none_for_invalid_legacy_shapes(
    config: object, caplog: pytest.LogCaptureFixture
) -> None:
    """Unrecognised shapes are not migrated (and emit no warning), so normal platform validation reports them."""
    with caplog.at_level(logging.WARNING):
        assert _migrate_legacy_image_config(config) is None
    assert "deprecated" not in caplog.text


def test_migrate_returns_none_for_mapping_form_defaults_files() -> None:
    """A `platform:`-tagged `defaults:`/`files:` mapping must not be swallowed by the legacy migrator."""
    config = {
        CONF_PLATFORM: "file",
        "defaults": {"type": "rgb565"},
        "files": [{"id": "a", "file": "a.png"}],
    }
    assert _migrate_legacy_image_config(config) is None


def test_migrate_returns_none_for_defaults_files_dict_without_platform() -> None:
    """`defaults:`/`files:` without `platform:` must not be swallowed either -- the flattener has
    no `files:` branch and would silently return `[]`."""
    config = {
        "defaults": {"type": "rgb565"},
        "files": [{"id": "a", "file": "a.png"}],
    }
    assert _migrate_legacy_image_config(config) is None


def test_migrate_returns_none_for_defaults_files_list_entry_without_platform() -> None:
    """Same, in a list -- previously the list branch migrated it to a hard-coded
    `platform: file` instead of raising a missing-platform error."""
    config = [
        {
            "defaults": {"type": "rgb565"},
            "files": [{"id": "a", "file": "a.png"}],
        }
    ]
    assert _migrate_legacy_image_config(config) is None


# --------------------------- end legacy migration --------------------------


def test_expand_platform_entry_passes_through_plain_entry() -> None:
    entry = {CONF_PLATFORM: "file", "id": "a", "file": "x.png"}
    assert _expand_platform_entry(0, entry) == [entry]


def test_expand_platform_entry_expands_files_with_defaults() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "RGB565", "transparency": "opaque"},
        CONF_FILES: [
            {"id": "img1", "file": "foo.png"},
            {"id": "img2", "file": "bar.png", "type": "GRAYSCALE"},
        ],
    }
    assert _expand_platform_entry(0, entry) == [
        {
            CONF_PLATFORM: "file",
            "id": "img1",
            "file": "foo.png",
            "type": "RGB565",
            "transparency": "opaque",
        },
        {
            CONF_PLATFORM: "file",
            "id": "img2",
            "file": "bar.png",
            "type": "GRAYSCALE",
            "transparency": "opaque",
        },
    ]


def test_expand_platform_entry_files_without_defaults() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_FILES: [{"id": "img1", "file": "foo.png"}],
    }
    assert _expand_platform_entry(0, entry) == [
        {CONF_PLATFORM: "file", "id": "img1", "file": "foo.png"}
    ]


def test_expand_platform_entry_preserves_source_range() -> None:
    """A merged entry keeps the source range of its `files:` item so whole-entry errors anchor there."""
    from esphome import yaml_util

    file_entry = yaml_util.make_data_base({"id": "img1", "file": "foo.png"})
    file_entry._esp_range = "sentinel-range"
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "RGB565"},
        CONF_FILES: [file_entry],
    }
    [out] = _expand_platform_entry(0, entry)
    assert isinstance(out, yaml_util.ESPHomeDataBase)
    assert out.esp_range == "sentinel-range"


def test_expand_platform_entry_plain_dict_file_entry_has_no_source_range() -> None:
    """Plain-dict `files:` items must not crash -- `from_database` reads `.esp_range` unconditionally."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_FILES: [{"id": "img1", "file": "foo.png"}],
    }
    [out] = _expand_platform_entry(0, entry)
    assert out == {CONF_PLATFORM: "file", "id": "img1", "file": "foo.png"}


def test_expand_platform_entry_per_file_overrides_win() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "RGB565"},
        CONF_FILES: [{"id": "img1", "file": "foo.png", "type": "BINARY"}],
    }
    [out] = _expand_platform_entry(0, entry)
    assert out["type"] == "BINARY"


def test_expand_platform_entry_drops_byte_order_for_non_endian_override() -> None:
    """A `byte_order` default merged into a non-endian override is dropped, as the legacy flattener did."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "rgb565", "byte_order": "little_endian"},
        CONF_FILES: [
            {"id": "a", "file": "x.png"},
            {"id": "b", "file": "y.png", "type": "binary"},
        ],
    }
    out = _expand_platform_entry(0, entry)
    assert out[0]["byte_order"] == "little_endian"
    assert "byte_order" not in out[1]


def test_expand_platform_entry_invalid_byte_order_in_defaults_raises() -> None:
    """A dropped `byte_order` inherited from `defaults:` is still validated, so a typo raises."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "rgb565", "byte_order": "little_andian"},
        CONF_FILES: [{"id": "a", "file": "x.png", "type": "binary"}],
    }
    with pytest.raises(cv.Invalid, match="did you mean") as excinfo:
        _expand_platform_entry(0, entry)
    assert excinfo.value.path == [0]


def test_expand_platform_entry_keeps_byte_order_for_endian_override() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "rgb565", "byte_order": "big_endian"},
        CONF_FILES: [{"id": "a", "file": "x.png", "type": "rgb565"}],
    }
    [out] = _expand_platform_entry(0, entry)
    assert out["byte_order"] == "big_endian"


def test_expand_platform_entry_keeps_explicit_byte_order_conflict() -> None:
    """A `byte_order` written directly on the entry is kept so validate_settings raises the normal error."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {"type": "rgb565"},
        CONF_FILES: [
            {
                "id": "a",
                "file": "x.png",
                "type": "binary",
                "byte_order": "little_endian",
            }
        ],
    }
    [out] = _expand_platform_entry(0, entry)
    assert out["byte_order"] == "little_endian"


def test_expand_platform_entry_defaults_without_files_raises() -> None:
    entry = {CONF_PLATFORM: "file", CONF_DEFAULTS: {"type": "RGB565"}}
    with pytest.raises(cv.Invalid, match="may only be used together with") as excinfo:
        _expand_platform_entry(0, entry)
    assert excinfo.value.path == [0]


def test_expand_platform_entry_null_files_raises_not_empty() -> None:
    """A `files:` key with no value parses to `None` and must be reported clearly."""
    entry = {CONF_PLATFORM: "file", CONF_DEFAULTS: {"type": "RGB565"}, CONF_FILES: None}
    with pytest.raises(cv.Invalid, match="must not be empty"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_empty_files_list_raises_not_empty() -> None:
    """An explicit `files: []` must not silently drop the whole platform entry."""
    entry = {CONF_PLATFORM: "file", CONF_FILES: []}
    with pytest.raises(cv.Invalid, match="must not be empty"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_files_with_stray_key_raises() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_FILES: [{"id": "a", "file": "x.png"}],
        "extra": 1,
    }
    with pytest.raises(cv.Invalid, match="cannot be combined with"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_id_in_defaults_raises() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {CONF_ID: "a"},
        CONF_FILES: [{"file": "x.png"}],
    }
    with pytest.raises(cv.Invalid, match="not allowed inside"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_platform_in_defaults_raises() -> None:
    """`platform:` inside `defaults:` would silently reassign every file's platform."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: {CONF_PLATFORM: "animation"},
        CONF_FILES: [{"id": "a", "file": "x.png"}],
    }
    with pytest.raises(cv.Invalid, match="not allowed inside"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_platform_in_file_entry_raises() -> None:
    """`platform:` on a `files:` item must not silently override the entry's platform."""
    entry = {
        CONF_PLATFORM: "file",
        CONF_FILES: [{"id": "a", "file": "x.png", CONF_PLATFORM: "animation"}],
    }
    with pytest.raises(cv.Invalid, match="not allowed inside"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_files_not_list_raises() -> None:
    entry = {CONF_PLATFORM: "file", CONF_FILES: "not-a-list"}
    with pytest.raises(cv.Invalid, match="must be a list"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_defaults_not_mapping_raises() -> None:
    entry = {
        CONF_PLATFORM: "file",
        CONF_DEFAULTS: "not-a-mapping",
        CONF_FILES: [{"id": "a", "file": "x.png"}],
    }
    with pytest.raises(cv.Invalid, match="must be a mapping"):
        _expand_platform_entry(0, entry)


def test_expand_platform_entry_file_item_not_mapping_raises() -> None:
    entry = {CONF_PLATFORM: "file", CONF_FILES: [1, 2]}
    with pytest.raises(cv.Invalid, match="must be a mapping"):
        _expand_platform_entry(0, entry)


def test_expand_platform_config_mixes_plain_and_expanded_entries() -> None:
    config = [
        {
            CONF_PLATFORM: "file",
            CONF_DEFAULTS: {"type": "RGB565"},
            CONF_FILES: [
                {"id": "img1", "file": "foo.png"},
                {"id": "img2", "file": "bar.png"},
            ],
        },
        {CONF_PLATFORM: "file", "id": "plain", "file": "baz.png", "type": "BINARY"},
    ]
    out = expand_platform_config(config)
    assert [entry["id"] for entry in out] == ["img1", "img2", "plain"]


def test_expand_platform_config_ignores_non_platform_entries() -> None:
    # Not expanded here -- legacy_config_migrate runs before this hook and is
    # responsible for tagging/flattening pre-platform shapes.
    config = ["not-a-platform-entry"]
    assert expand_platform_config(config) == config


# --------------------- end defaults/files expansion -------------------------


def test_validate_image_final_defaults_to_little_endian() -> None:
    config = {CONF_FILE: "x.png"}
    validate_image_final(config)
    assert config[CONF_BYTE_ORDER] == "LITTLE_ENDIAN"


def test_validate_image_final_keeps_little_endian(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = {CONF_FILE: "x.png", CONF_BYTE_ORDER: "LITTLE_ENDIAN"}
    with caplog.at_level(logging.WARNING):
        validate_image_final(config)
    assert config[CONF_BYTE_ORDER] == "LITTLE_ENDIAN"
    assert "big-endian" not in caplog.text


def test_validate_image_final_warns_on_big_endian(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = {CONF_FILE: "x.png", CONF_BYTE_ORDER: "BIG_ENDIAN"}
    with caplog.at_level(logging.WARNING):
        validate_image_final(config)
    assert config[CONF_BYTE_ORDER] == "BIG_ENDIAN"
    assert "big-endian" in caplog.text


def test_image_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test image generation configuration."""

    main_cpp = generate_main(component_config_path("image_test.yaml"))
    assert "uint8_t_id[] PROGMEM = {0x24, 0x21, 0x24, 0x21" in main_cpp
    assert (
        "alignas(image::Image) static unsigned char image__cat_img__pstorage[sizeof(image::Image)];"
        in main_cpp
    )
    assert (
        "static image::Image *const cat_img = reinterpret_cast<image::Image *>(image__cat_img__pstorage);"
        in main_cpp
    )
    assert (
        "new(cat_img) image::Image(uint8_t_id, 32, 24, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);"
        in main_cpp
    )


def test_image_to_code_defines_and_core_data(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that to_code() sets USE_IMAGE define and stores image metadata."""
    # Generate the main cpp which will call to_code
    generate_main(component_config_path("image_test.yaml"))

    # Verify USE_IMAGE define was added
    assert any(d.name == "USE_IMAGE" for d in CORE.defines), (
        "USE_IMAGE define should be set when images are configured"
    )

    # Use the public API to get image metadata
    # The test config has an image with id 'cat_img'
    cat_img_metadata = get_image_metadata("cat_img")

    assert cat_img_metadata is not None, (
        "Image metadata should be retrievable via get_image_metadata()"
    )

    # Verify the metadata has the expected attributes
    assert hasattr(cat_img_metadata, "width"), "Metadata should have width attribute"
    assert hasattr(cat_img_metadata, "height"), "Metadata should have height attribute"
    assert hasattr(cat_img_metadata, "image_type"), (
        "Metadata should have image_type attribute"
    )
    assert hasattr(cat_img_metadata, "transparency"), (
        "Metadata should have transparency attribute"
    )

    # Verify the values are correct (from the test image)
    assert cat_img_metadata.width == 32, "Width should be 32"
    assert cat_img_metadata.height == 24, "Height should be 24"
    assert cat_img_metadata.image_type == "RGB565", "Type should be RGB565"
    assert cat_img_metadata.transparency == "opaque", "Transparency should be opaque"


def test_image_to_code_multiple_images(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that to_code() stores metadata for multiple images."""
    generate_main(component_config_path("image_test.yaml"))

    # Use the public API to get all image metadata
    all_metadata = get_all_image_metadata()

    assert isinstance(all_metadata, dict), (
        "get_all_image_metadata() should return a dictionary"
    )

    # Verify that at least one image is present
    assert len(all_metadata) > 0, "Should have at least one image metadata entry"

    # Each image ID should map to an ImageMetaData object
    for image_id, metadata in all_metadata.items():
        assert isinstance(image_id, str), "Image IDs should be strings"

        # Verify it's an ImageMetaData object with all required attributes
        assert hasattr(metadata, "width"), (
            f"Metadata for '{image_id}' should have width"
        )
        assert hasattr(metadata, "height"), (
            f"Metadata for '{image_id}' should have height"
        )
        assert hasattr(metadata, "image_type"), (
            f"Metadata for '{image_id}' should have image_type"
        )
        assert hasattr(metadata, "transparency"), (
            f"Metadata for '{image_id}' should have transparency"
        )

        # Verify values are valid
        assert isinstance(metadata.width, int), (
            f"Width for '{image_id}' should be an integer"
        )
        assert isinstance(metadata.height, int), (
            f"Height for '{image_id}' should be an integer"
        )
        assert isinstance(metadata.image_type, str), (
            f"Type for '{image_id}' should be a string"
        )
        assert isinstance(metadata.transparency, str), (
            f"Transparency for '{image_id}' should be a string"
        )
        assert metadata.width > 0, f"Width for '{image_id}' should be positive"
        assert metadata.height > 0, f"Height for '{image_id}' should be positive"


def test_get_image_metadata_nonexistent() -> None:
    """Test that get_image_metadata returns None for non-existent image IDs."""
    # This should return None when no images are configured or ID doesn't exist
    metadata = get_image_metadata("nonexistent_image_id")
    assert metadata is None, (
        "get_image_metadata should return None for non-existent IDs"
    )


def test_get_all_image_metadata_empty() -> None:
    """Test that get_all_image_metadata returns empty dict when no images configured."""
    # When CORE hasn't been initialized with images, should return empty dict
    all_metadata = get_all_image_metadata()
    assert isinstance(all_metadata, dict), (
        "get_all_image_metadata should always return a dict"
    )
    # Length could be 0 or more depending on what's in CORE at test time


@pytest.fixture
def mock_progmem_array():
    """Mock progmem_array to avoid needing a proper ID object in tests."""
    with patch("esphome.components.file.image.cg.progmem_array") as mock_progmem:
        mock_progmem.return_value = MagicMock()
        yield mock_progmem


@pytest.mark.asyncio
async def test_svg_with_mm_dimensions_succeeds(
    component_config_path: Callable[[str], Path],
    mock_progmem_array: MagicMock,
) -> None:
    """Test that SVG files with dimensions in mm are successfully processed."""
    # Create a config for write_image without CONF_RESIZE
    config = {
        CONF_FILE: component_config_path("mm_dimensions.svg"),
        CONF_TYPE: "BINARY",
        CONF_TRANSPARENCY: CONF_OPAQUE,
        CONF_DITHER: "NONE",
        CONF_INVERT_ALPHA: False,
        CONF_RAW_DATA_ID: "test_raw_data_id",
    }

    # This should succeed without raising an error
    result = await write_image(config)

    # Verify that write_image returns the expected tuple
    assert isinstance(result, tuple), "write_image should return a tuple"
    assert len(result) == 6, "write_image should return 6 values"

    prog_arr, width, height, image_type, trans_value, frame_count = result

    # Verify the dimensions are positive integers
    # At 100 DPI, 10mm = ~39 pixels (10mm * 100dpi / 25.4mm_per_inch)
    assert isinstance(width, int), "Width should be an integer"
    assert isinstance(height, int), "Height should be an integer"
    assert width > 0, "Width should be positive"
    assert height > 0, "Height should be positive"
    assert frame_count == 1, "Single image should have frame_count of 1"
    # Verify we got reasonable dimensions from the mm-based SVG
    assert 30 < width < 50, (
        f"Width should be around 39 pixels for 10mm at 100dpi, got {width}"
    )
    assert 30 < height < 50, (
        f"Height should be around 39 pixels for 10mm at 100dpi, got {height}"
    )


@pytest.mark.asyncio
async def test_rgb565_alpha_animation_layout_per_frame(
    tmp_path: Path,
    mock_progmem_array: MagicMock,
) -> None:
    """RGB565+alpha animations must store each frame as a self-contained
    [RGB plane | alpha plane] block. Animation::update_data_start_ steps frames
    with a single per-frame stride, so any cross-frame layout (all RGB then all
    alpha) makes the C++ alpha read land in the next frame's RGB bytes — that
    was the regression behind issue #15999.
    """
    # Build a 2-frame APNG where each frame is a solid color with a known
    # alpha. APNG preserves full RGBA per pixel (GIF only has 1-bit alpha so
    # round-tripping mid-range alpha values does not work). Frame 0 is fully
    # opaque red, frame 1 is fully transparent blue.
    width = 4
    height = 3
    frame0 = PILImage.new("RGBA", (width, height), (255, 0, 0, 0xFF))
    frame1 = PILImage.new("RGBA", (width, height), (0, 0, 255, 0x00))
    apng_path = tmp_path / "anim.png"
    frame0.save(
        apng_path,
        format="PNG",
        save_all=True,
        append_images=[frame1],
        duration=100,
        loop=0,
    )

    config = {
        CONF_FILE: str(apng_path),
        CONF_TYPE: "RGB565",
        CONF_TRANSPARENCY: CONF_ALPHA_CHANNEL,
        CONF_DITHER: "NONE",
        CONF_INVERT_ALPHA: False,
        CONF_RAW_DATA_ID: "test_raw_data_id",
    }

    _, _, _, _, _, frame_count = await write_image(config, all_frames=True)
    assert frame_count == 2

    # Recover the bytes handed to progmem_array. Signature is (id_, rhs).
    _, raw_data = mock_progmem_array.call_args.args
    data = [int(x) for x in raw_data]

    rgb_size = width * height * 2
    alpha_size = width * height
    frame_size = rgb_size + alpha_size
    assert len(data) == frame_size * frame_count, (
        "RGB565+alpha animation buffer must be (RGB + alpha) per frame, not "
        "all RGB followed by all alpha"
    )

    # Frame 0: RGB plane is red, alpha plane is 0xFF. Frame 1: alpha plane is
    # 0x00. If the layout regresses to [all RGB | all alpha], the alpha bytes
    # would all land at the tail of the buffer and the per-frame slices below
    # would point at RGB565 noise instead.
    frame0_alpha = data[rgb_size : rgb_size + alpha_size]
    frame1_alpha = data[frame_size + rgb_size : frame_size + rgb_size + alpha_size]
    assert all(a == 0xFF for a in frame0_alpha), (
        f"Frame 0 alpha plane should be opaque, got {frame0_alpha}"
    )
    assert all(a == 0x00 for a in frame1_alpha), (
        f"Frame 1 alpha plane should be transparent, got {frame1_alpha}"
    )
