"""Resolve and encode static images on the build host, without device conversion."""

from collections.abc import Generator
from pathlib import Path
from unittest.mock import patch

from PIL import Image
import pytest

from esphome.components.display import add_metadata
from esphome.components.file.image import (
    resolve_image_type,
    validate_file_image_settings,
    validate_image_final,
    write_image,
)
from esphome.components.image import IMAGE_TYPE, validate_type
from esphome.config import Config
from esphome.config_validation import Invalid
from esphome.core import CORE, ID
from esphome.final_validate import full_config


@pytest.fixture(autouse=True)
def clean_core() -> Generator[None]:
    CORE.reset()
    yield
    CORE.reset()


def target(depth: int | None, ui_depth: int | None = None) -> None:
    config = Config()
    config["display"] = [{"id": ID("panel", True)}]
    add_metadata(ID("panel"), 3, 1, native_color_depth=depth)
    if ui_depth is not None:
        config["lvgl"] = [{"displays": [ID("panel")], "color_depth": ui_depth}]
    full_config.set(config)


@pytest.mark.parametrize(
    ("depth", "ui_depth", "expected"),
    [(16, None, "RGB565"), (24, None, "RGB"), (24, 16, "RGB565"), (None, 16, "RGB565")],
)
def test_auto_target(depth: int | None, ui_depth: int | None, expected: str) -> None:
    target(depth, ui_depth)
    config = {"id": "asset", "type": "AUTO"}
    validate_image_final(config)
    assert config["type"] == expected
    # Resolution is idempotent and does not retain state across builds.
    validate_image_final(config)
    assert config["type"] == expected


def test_auto_unknown() -> None:
    target(None)
    with pytest.raises(Invalid, match="Cannot determine.*panel"):
        resolve_image_type({"id": "asset", "type": "AUTO"})


def test_auto_without_display() -> None:
    full_config.set(Config())
    with pytest.raises(Invalid, match="needs target_display"):
        resolve_image_type({"id": "asset", "type": "AUTO"})


def test_auto_rejects_byte_order_for_rgb888() -> None:
    target(24)
    config = {"id": "asset", "type": "AUTO", "byte_order": "LITTLE_ENDIAN"}
    validate_file_image_settings(config)
    with pytest.raises(Invalid, match="does not support byte order"):
        validate_image_final(config)


def test_runtime_image_auto_has_actionable_error() -> None:
    with pytest.raises(Invalid, match="explicit type for runtime images"):
        validate_type(IMAGE_TYPE)("AUTO")


def test_auto_ambiguous_and_explicit() -> None:
    target(24)
    full_config.get()["display"].append({"id": ID("second", True)})
    config = {"id": "asset", "type": "AUTO"}
    with pytest.raises(Invalid, match="target_display"):
        resolve_image_type(config)
    config["target_display"] = ID("panel")
    resolve_image_type(config)
    assert config["type"] == "RGB"


@pytest.mark.parametrize("kind", ["RGB", "RGB565", "BINARY", "GRAYSCALE"])
def test_explicit_types_unchanged(kind: str) -> None:
    config = {"id": "asset", "type": kind}
    resolve_image_type(config)
    assert config["type"] == kind
    with pytest.raises(Invalid, match="only valid with type: AUTO"):
        validate_file_image_settings({**config, "target_display": ID("panel")})


@pytest.mark.parametrize(
    ("depth", "alpha", "expected"),
    [
        (16, False, [0xAA, 0x11, 0x00, 0xF8]),
        (24, False, [0x56, 0x34, 0x12, 0, 0, 255]),
        (16, True, [0xAA, 0x11, 0x00, 0xF8, 128, 255]),
        (24, True, [0x56, 0x34, 0x12, 128, 0, 0, 255, 255]),
    ],
)
@pytest.mark.asyncio
async def test_auto_encoded_bytes(
    tmp_path: Path, depth: int, alpha: bool, expected: list[int]
) -> None:
    target(depth)
    path = tmp_path / "source.png"
    image = Image.new("RGBA", (2, 1))
    image.putdata([(0x12, 0x34, 0x56, 128), (255, 0, 0, 255)])
    image.save(path)
    config = {
        "id": "asset",
        "type": "AUTO",
        "file": str(path),
        "dither": "NONE",
        "invert_alpha": False,
        "transparency": "alpha_channel" if alpha else "opaque",
        "raw_data_id": "data",
    }
    validate_file_image_settings(config)
    validate_image_final(config)
    with patch("esphome.components.file.image.cg.progmem_array") as array:
        await write_image(config)
    assert list(array.call_args.args[1]) == expected


@pytest.mark.parametrize("depth", [16, 24])
@pytest.mark.parametrize("platform", ["file", "animation"])
@pytest.mark.parametrize("image_first", [False, True])
def test_auto_full_config(
    tmp_path: Path, depth: int, platform: str, image_first: bool
) -> None:
    """Real validation resolves AUTO independent of YAML order and generated IDs."""
    from esphome.config import read_config

    source = tmp_path / "source.png"
    Image.new("RGB", (3, 1), (18, 52, 86)).save(source)
    displays = f"""
display:
  - platform: snapshot
    dimensions: 17x9
    color_depth: {depth}
    auto_clear_enabled: false
lvgl:
  color_depth: {depth}
  widgets:
    - image:
        src: asset
"""
    images = f"""
image:
  - platform: {platform}
    defaults:
      type: AUTO
    files:
      - id: asset
        file: {source}
"""
    path = tmp_path / "test.yaml"
    path.write_text(
        "esphome:\n  name: auto-test\nhost:\n"
        + (images + displays if image_first else displays + images)
    )
    CORE.config_path = path
    config = read_config({})
    assert config is not None
    assert config["image"][0]["type"] == ("RGB565" if depth == 16 else "RGB")


@pytest.mark.parametrize("depth", [16, 24])
@pytest.mark.asyncio
async def test_auto_animation_frames(tmp_path: Path, depth: int) -> None:
    target(depth)
    path = tmp_path / "frames.png"
    first = Image.new("RGBA", (1, 1), (255, 0, 0, 128))
    first.save(
        path,
        save_all=True,
        append_images=[Image.new("RGBA", (1, 1), (0, 0, 255, 255))],
        duration=100,
    )
    config = {
        "id": "asset",
        "type": "AUTO",
        "file": str(path),
        "dither": "NONE",
        "invert_alpha": False,
        "transparency": "alpha_channel",
        "raw_data_id": "data",
    }
    validate_image_final(config)
    with patch("esphome.components.file.image.cg.progmem_array") as array:
        *_, frames = await write_image(config, all_frames=True)
    assert frames == 2
    expected = (
        [0, 248, 128, 31, 0, 255] if depth == 16 else [0, 0, 255, 128, 255, 0, 0, 255]
    )
    assert list(array.call_args.args[1]) == expected
