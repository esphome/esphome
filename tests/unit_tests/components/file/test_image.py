"""Tests for the file image platform's prefetch extraction."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

from PIL import Image
import pytest

from esphome import yaml_util
from esphome.components.animation.image import ANIMATION_CONFIG_SCHEMA
from esphome.components.file import image as file_image
from esphome.components.image import (
    CONF_ALPHA_CHANNEL,
    CONF_CHROMA_KEY,
    CONF_OPAQUE,
    CONF_TRANSPARENCY,
    validate_background_color,
    validate_settings,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BACKGROUND_COLOR,
    CONF_DITHER,
    CONF_FILE,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
)
from esphome.core import CORE, ID
from esphome.external_files import RemoteFile, url_cache_key
from esphome.loader import get_component, get_platform


def test_extract_mdi_shorthand(setup_core: Path) -> None:
    ref = file_image._extract_file_ref("mdi:home")
    assert ref is not None
    assert ref.url == file_image.MDI_SOURCES["mdi"] + "home.svg"
    assert ref.path.name == "home.svg"
    assert ref.path.parent.name == "mdi"


def test_extract_web_url(setup_core: Path) -> None:
    url = "https://example.com/img.png"
    ref = file_image._extract_file_ref(url)
    assert ref == RemoteFile(url, file_image.compute_local_image_path(url))


def test_extract_typed_dicts(setup_core: Path) -> None:
    url = "https://example.com/img.png"
    assert file_image._extract_file_ref({"source": "web", "url": url}) == RemoteFile(
        url, file_image.compute_local_image_path(url)
    )
    ref = file_image._extract_file_ref({"source": "mdil", "icon": "home"})
    assert ref is not None
    assert ref.url == file_image.MDI_SOURCES["mdil"] + "home.svg"


def test_extract_skips_local_and_garbage(setup_core: Path) -> None:
    assert file_image._extract_file_ref("images/local.png") is None
    assert file_image._extract_file_ref("mdi:not a valid icon!") is None
    assert file_image._extract_file_ref({"source": "local", "path": "x.png"}) is None
    assert file_image._extract_file_ref(42) is None
    assert file_image._extract_file_ref(None) is None


def test_prefetch_files_yields_remote_refs(setup_core: Path) -> None:
    entries = [
        {"file": "mdi:home"},
        {"file": "images/local.png"},
        {"file": "https://example.com/img.png"},
        {"no_file_key": True},
    ]
    [files] = list(file_image.PREFETCH_FILES(entries))
    assert len(files) == 2
    assert files[0].url.endswith("home.svg")
    assert files[1].url == "https://example.com/img.png"


def test_validated_file_values_hash_alike_across_data_dirs(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A CLI and an add-on data dir dump validated image files identically."""
    url = "https://example.com/img.png"
    (setup_core / "img.png").touch()
    dumps: list[str] = []
    for data_dir in (
        setup_core / ".esphome",
        setup_core.parent / f"{setup_core.name}-data",
    ):
        monkeypatch.setenv("ESPHOME_DATA_DIR", str(data_dir))
        with patch("esphome.components.file.image.external_files.download_content"):
            config = {
                "remote": file_image.validate_file_shorthand(url),
                "mdi": file_image.validate_file_shorthand("mdi:home"),
                "local": file_image.validate_file_shorthand("img.png"),
                "local_schema": file_image.LOCAL_SCHEMA({CONF_PATH: "img.png"}),
            }
        dumps.append(
            yaml_util.dump(
                config,
                sort_keys=True,
                relative_to=CORE.config_dir,
                data_dir=CORE.data_dir,
            )
        )
    assert dumps[0] == dumps[1]
    assert dumps[0].splitlines() == [
        "local: img.png",
        "local_schema: img.png",
        "mdi: .esphome/image/mdi/home.svg",
        f"remote: .esphome/image/{url_cache_key(url)}",
    ]


def test_extractor_matches_validator_path(setup_core: Path) -> None:
    """The path the validator downloads to equals the extractor's path."""
    with patch(
        "esphome.components.file.image.external_files.download_content"
    ) as mock_download:
        file_image.validate_file_shorthand("mdi:home")

    validated_path = mock_download.call_args[0][1]
    assert validated_path == file_image._extract_file_ref("mdi:home").path


def test_hook_is_wired_to_both_animation_domains() -> None:
    """Both animation entry points expose the shared image hook."""

    assert get_component("animation").prefetch_files is file_image.PREFETCH_FILES
    assert (
        get_platform("image", "animation").prefetch_files is file_image.PREFETCH_FILES
    )


def test_validate_background_color() -> None:
    """Background color accepts 0xRRGGBB, #RRGGBB, #RGB, and CSS color names."""
    assert validate_background_color(0x0F1929) == 0x0F1929
    assert validate_background_color("0x0F1929") == 0x0F1929
    assert validate_background_color("0X0f1929") == 0x0F1929
    assert validate_background_color("#0F1929") == 0x0F1929
    assert validate_background_color(" #abc ") == 0xAABBCC
    assert validate_background_color("white") == 0xFFFFFF
    assert validate_background_color(" White ") == 0xFFFFFF
    for invalid in (
        "#12345",
        "#+12345",
        "#12_456",
        "# 12345",
        "#gggggg",
        "notacolor",
        "",
        -1,
        0x1000000,
        True,
        False,
        None,
        1.5,
    ):
        with pytest.raises(cv.Invalid):
            validate_background_color(invalid)


def test_validate_settings_background_color_rejects_alpha_channel() -> None:
    """background_color is rejected for alpha_channel and allowed for opaque/chroma_key."""
    base = {CONF_TYPE: "RGB565", CONF_BACKGROUND_COLOR: 0x0F1929}
    validate_settings({**base, CONF_TRANSPARENCY: CONF_OPAQUE})
    validate_settings({**base, CONF_TRANSPARENCY: CONF_CHROMA_KEY})
    with pytest.raises(cv.Invalid, match="alpha_channel"):
        validate_settings({**base, CONF_TRANSPARENCY: CONF_ALPHA_CHANNEL})


def test_animation_schema_supports_background_color(setup_core: Path) -> None:
    """Animation platform shares image_schema and validate_settings for background_color."""
    gif_path = setup_core / "anim.png"
    Image.new("RGBA", (2, 2), (255, 0, 0, 0)).save(gif_path)

    validated = ANIMATION_CONFIG_SCHEMA(
        {
            CONF_ID: "my_anim",
            CONF_FILE: "anim.png",
            CONF_TYPE: "RGB565",
            CONF_BACKGROUND_COLOR: "#112233",
        }
    )
    assert validated[CONF_BACKGROUND_COLOR] == 0x112233

    with pytest.raises(cv.Invalid, match="alpha_channel"):
        ANIMATION_CONFIG_SCHEMA(
            {
                CONF_ID: "my_anim",
                CONF_FILE: "anim.png",
                CONF_TYPE: "RGB565",
                CONF_TRANSPARENCY: CONF_ALPHA_CHANNEL,
                CONF_BACKGROUND_COLOR: "#112233",
            }
        )


def test_apply_background_color_opaque_and_chroma_key() -> None:
    """Semi-transparent edges blend onto background_color for opaque and chroma_key."""
    # 3 pixels: fully transparent (a=0), semi-transparent white (a=128), opaque red (a=255)
    src = Image.new("RGBA", (3, 1))
    src.putdata([(255, 255, 255, 0), (255, 255, 255, 128), (255, 0, 0, 255)])
    bg = 0x000080  # (0, 0, 128)

    opaque_out = list(
        file_image._apply_background_color(src, bg, CONF_OPAQUE).get_flattened_data()
    )
    assert opaque_out[0] == (0, 0, 128, 255)
    assert opaque_out[1][3] == 255
    assert 120 <= opaque_out[1][0] <= 135  # blended white over (0, 0, 128)
    assert opaque_out[1][2] > 180
    assert opaque_out[2] == (255, 0, 0, 255)

    chroma_out = list(
        file_image._apply_background_color(
            src, bg, CONF_CHROMA_KEY
        ).get_flattened_data()
    )
    assert chroma_out[0][3] == 0  # remains transparent for chroma_key
    assert chroma_out[1] == opaque_out[1]  # a >= 128 blended and made opaque
    assert chroma_out[2] == (255, 0, 0, 255)


def test_apply_background_color_source_modes() -> None:
    """_apply_background_color handles P, L, and LA source images cleanly."""
    bg = 0x102030  # (16, 32, 48)

    # Palette (P) image with palette index 0 transparent and index 1 opaque red
    p_img = Image.new("P", (2, 1), 0)
    p_img.putpalette([0, 0, 0, 255, 0, 0] + [0] * (254 * 3))
    p_img.info["transparency"] = 0
    p_img.putpixel((1, 0), 1)
    p_out = list(
        file_image._apply_background_color(p_img, bg, CONF_OPAQUE).get_flattened_data()
    )
    assert p_out == [(16, 32, 48, 255), (255, 0, 0, 255)]

    # Grayscale (L) image: opaque, unchanged RGB
    l_img = Image.new("L", (1, 1), 200)
    assert list(
        file_image._apply_background_color(l_img, bg, CONF_OPAQUE).get_flattened_data()
    ) == [(200, 200, 200, 255)]

    # Grayscale + Alpha (LA) image with chroma_key
    la_img = Image.new("LA", (2, 1))
    la_img.putdata([(200, 0), (200, 255)])
    la_out = list(
        file_image._apply_background_color(
            la_img, bg, CONF_CHROMA_KEY
        ).get_flattened_data()
    )
    assert la_out[0][3] == 0
    assert la_out[1] == (200, 200, 200, 255)


@pytest.mark.asyncio
async def test_write_image_background_color_png_and_svg(setup_core: Path) -> None:
    """write_image composites background_color end-to-end for PNG and SVG across formats."""
    png_path = setup_core / "test.png"
    src = Image.new("RGBA", (2, 1))
    src.putdata([(0, 0, 0, 0), (255, 0, 0, 255)])
    src.save(png_path)

    svg_path = setup_core / "test.svg"
    svg_path.write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="2" height="1">'
        '<rect x="1" y="0" width="1" height="1" fill="#ff0000"/>'
        "</svg>"
    )

    async def _run_write(
        file_path: Path,
        image_type: str,
        transparency: str,
        bg_color: int,
        idx: int,
    ) -> list[int]:
        config = {
            CONF_FILE: file_path,
            CONF_TYPE: image_type,
            CONF_TRANSPARENCY: transparency,
            CONF_DITHER: "NONE",
            file_image.CONF_INVERT_ALPHA: False,
            CONF_BACKGROUND_COLOR: bg_color,
            CONF_RAW_DATA_ID: ID(f"raw_{idx}", is_declaration=True, type="uint8_t"),
        }
        with patch(
            "esphome.components.file.image.cg.progmem_array",
            side_effect=lambda _id, rhs: rhs,
        ):
            prog_arr, width, height, _, _, _ = await file_image.write_image(config)
        assert (width, height) == (2, 1)
        return [int(x) for x in prog_arr]

    # RGB565 opaque on blue (0x0000FF -> RGB565 0x001F -> LE [0x1F, 0x00])
    # Red (0xFF0000 -> RGB565 0xF800 -> LE [0x00, 0xF8])
    assert await _run_write(png_path, "RGB565", CONF_OPAQUE, 0x0000FF, 1) == [
        0x1F,
        0x00,
        0x00,
        0xF8,
    ]
    # RGB565 chroma_key: transparent pixel stays chroma key (r=0,g=1,b=0 -> 0x0020 -> LE [0x20, 0x00])
    assert await _run_write(png_path, "RGB565", CONF_CHROMA_KEY, 0x0000FF, 2) == [
        0x20,
        0x00,
        0x00,
        0xF8,
    ]
    # RGB opaque (BGR order per pixel)
    assert await _run_write(png_path, "RGB", CONF_OPAQUE, 0x112233, 3) == [
        0x33,
        0x22,
        0x11,
        0x00,
        0x00,
        0xFF,
    ]
    # RGB111 opaque: blue (0,0,1) and red (1,0,0) -> 0b001100_00 = 0x30
    assert await _run_write(png_path, "RGB111", CONF_OPAQUE, 0x0000FF, 4) == [0x30]
    # GRAYSCALE opaque: white background (255) + red (76 in ITU-R 601)
    gs_opaque = await _run_write(png_path, "GRAYSCALE", CONF_OPAQUE, 0xFFFFFF, 5)
    assert gs_opaque[0] == 255
    assert 70 <= gs_opaque[1] <= 80
    # GRAYSCALE chroma_key: transparent pixel becomes chroma key 1
    gs_chroma = await _run_write(png_path, "GRAYSCALE", CONF_CHROMA_KEY, 0xFFFFFF, 6)
    assert gs_chroma[0] == 1
    assert gs_chroma[1] == gs_opaque[1]
    # BINARY opaque: white background (1) + black foreground -> 0b10_000000 = 0x80
    assert await _run_write(png_path, "BINARY", CONF_OPAQUE, 0xFFFFFF, 7) == [0x80]
    # SVG opaque on blue
    assert await _run_write(svg_path, "RGB565", CONF_OPAQUE, 0x0000FF, 8) == [
        0x1F,
        0x00,
        0x00,
        0xF8,
    ]
