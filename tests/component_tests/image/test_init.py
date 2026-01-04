"""Tests for image configuration validation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import subprocess
import sys
from typing import Any
from unittest.mock import MagicMock, patch

from PIL import UnidentifiedImageError
import pytest

from esphome import config_validation as cv
import esphome.components.image as image_module
from esphome.components.image import CONF_TRANSPARENCY, CONFIG_SCHEMA
from esphome.const import CONF_ID, CONF_RAW_DATA_ID, CONF_TYPE


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            "a string",
            "Badly formed image configuration, expected a list or a dictionary",
            id="invalid_string_config",
        ),
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
        pytest.param(
            {"defaults": {}, "images": [{"id": "image_id", "file": "image.png"}]},
            "Type is required either in the image config or in the defaults",
            id="missing_type_in_defaults",
        ),
    ],
)
def test_image_configuration_errors(
    config: Any,
    error_match: str,
) -> None:
    """Test detection of invalid configuration."""
    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    "config",
    [
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "rgb565",
                "transparency": "chroma_key",
                "byte_order": "little_endian",
                "dither": "FloydSteinberg",
                "resize": "100x100",
                "invert_alpha": False,
            },
            id="single_image_all_options",
        ),
        pytest.param(
            [
                {
                    "id": "image_id",
                    "file": "image.png",
                    "type": "binary",
                }
            ],
            id="list_of_images",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "rgb565",
                    "transparency": "chroma_key",
                    "byte_order": "little_endian",
                    "dither": "FloydSteinberg",
                    "resize": "100x100",
                    "invert_alpha": False,
                },
                "images": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                    }
                ],
            },
            id="images_with_defaults",
        ),
        pytest.param(
            {
                "rgb565": {
                    "alpha_channel": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                            "transparency": "alpha_channel",
                            "byte_order": "little_endian",
                            "dither": "FloydSteinberg",
                            "resize": "100x100",
                            "invert_alpha": False,
                        }
                    ]
                },
                "binary": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                        "transparency": "opaque",
                        "dither": "FloydSteinberg",
                        "resize": "100x100",
                        "invert_alpha": False,
                    }
                ],
            },
            id="type_based_organization",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "binary",
                    "transparency": "chroma_key",
                    "byte_order": "little_endian",
                    "dither": "FloydSteinberg",
                    "resize": "100x100",
                    "invert_alpha": False,
                },
                "rgb565": {
                    "alpha_channel": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                            "transparency": "alpha_channel",
                            "dither": "none",
                        }
                    ]
                },
                "binary": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                        "transparency": "opaque",
                    }
                ],
            },
            id="type_based_with_defaults",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "rgb565",
                    "transparency": "alpha_channel",
                },
                "binary": {
                    "opaque": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                        }
                    ],
                },
            },
            id="binary_with_defaults",
        ),
    ],
)
def test_image_configuration_success(
    config: dict[str, Any] | list[dict[str, Any]],
) -> None:
    """Test successful configuration validation."""
    result = CONFIG_SCHEMA(config)
    # All valid configurations should return a list of images
    assert isinstance(result, list)
    for key in (CONF_TYPE, CONF_ID, CONF_TRANSPARENCY, CONF_RAW_DATA_ID):
        assert all(key in x for x in result), (
            f"Missing key {key} in image configuration"
        )


def test_image_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test image generation configuration."""

    main_cpp = generate_main(component_config_path("image_test.yaml"))
    assert "uint8_t_id[] PROGMEM = {0x24, 0x21, 0x24, 0x21" in main_cpp
    assert (
        "cat_img = new image::Image(uint8_t_id, 32, 24, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);"
        in main_cpp
    )


class TestRenderSvgSafely:
    """Tests for the _render_svg_safely function."""

    def test_successful_svg_rendering_without_resize(self) -> None:
        """Test successful SVG rendering without resize parameters."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data"

        with patch(
            "esphome.components.image.subprocess.run", return_value=mock_result
        ) as mock_run:
            result = image_module._render_svg_safely(Path("/path/to/test.svg"), None)

            assert result == b"fake_png_data"
            mock_run.assert_called_once()
            args = mock_run.call_args
            assert args[0][0] == [sys.executable, "-c", args[0][0][2]]
            # Verify the script doesn't contain resize args
            assert "width=" not in args[0][0][2]
            assert "height=" not in args[0][0][2]

    def test_successful_svg_rendering_with_resize(self) -> None:
        """Test successful SVG rendering with resize parameters."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data_resized"

        with patch(
            "esphome.components.image.subprocess.run", return_value=mock_result
        ) as mock_run:
            result = image_module._render_svg_safely(
                Path("/path/to/test.svg"), (100, 200)
            )

            assert result == b"fake_png_data_resized"
            mock_run.assert_called_once()
            args = mock_run.call_args
            script = args[0][0][2]
            # Verify the script contains resize args
            assert "width=100" in script
            assert "height=200" in script

    def test_path_sanitization_unix_path(self) -> None:
        """Test path sanitization for Unix-style paths."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data"

        with patch(
            "esphome.components.image.subprocess.run", return_value=mock_result
        ) as mock_run:
            image_module._render_svg_safely(Path("/path/to/my test.svg"), None)

            args = mock_run.call_args
            script = args[0][0][2]
            # Verify the path is properly escaped in the script
            assert "with open('" in script
            assert "', encoding=" in script

    def test_path_sanitization_windows_path(self) -> None:
        """Test path sanitization for Windows-style paths with backslashes."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data"

        with (
            patch(
                "esphome.components.image.subprocess.run", return_value=mock_result
            ) as mock_run,
            patch("esphome.components.image.Path") as mock_path_class,
        ):
            # Simulate a Windows path
            mock_path = MagicMock()
            mock_path.resolve.return_value = Path("C:\\Users\\test\\image.svg")
            mock_path_class.return_value = mock_path

            image_module._render_svg_safely("C:\\Users\\test\\image.svg", None)

            args = mock_run.call_args
            script = args[0][0][2]
            # Verify backslashes are properly escaped
            assert "\\\\" in script

    def test_path_with_quotes(self) -> None:
        """Test path sanitization for paths containing quotes."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data"

        with patch(
            "esphome.components.image.subprocess.run", return_value=mock_result
        ) as mock_run:
            # Use a real Path object since the function doesn't call Path() on the input
            test_path = Path("/path/to/my'test.svg")

            image_module._render_svg_safely(test_path, None)

            args = mock_run.call_args
            script = args[0][0][2]
            # Verify quotes are properly escaped (looking for escaped single quote)
            assert "\\'" in script

    def test_error_handling_subprocess_failure(self) -> None:
        """Test error handling when subprocess fails without stderr."""
        with patch("esphome.components.image.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(
                returncode=1, cmd="test", stderr=None
            )

            with pytest.raises(
                UnidentifiedImageError, match="resvg failed to render SVG"
            ):
                image_module._render_svg_safely(Path("/path/to/test.svg"), None)

    def test_error_handling_rust_panic_with_err_value(self) -> None:
        """Test error handling when resvg panics with Err value message."""
        stderr_output = b"""thread '<unnamed>' (11042810) panicked at src/rust/lib.rs:417:70:
called `Result::unwrap()` on an `Err` value: "expected a quote not '1' at 1:47"
stack backtrace:
"""

        with patch("esphome.components.image.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(
                returncode=1, cmd="test", stderr=stderr_output
            )

            with pytest.raises(
                UnidentifiedImageError,
                match='resvg failed to render SVG: "expected a quote not.*1:47"',
            ):
                image_module._render_svg_safely(Path("/path/to/malformed.svg"), None)

    def test_error_handling_rust_panic_at(self) -> None:
        """Test error handling when resvg panics with 'panicked at' message."""
        stderr_output = b"""thread '<unnamed>' panicked at src/rust/lib.rs:100:50:
assertion failed
stack backtrace:
"""

        with patch("esphome.components.image.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(
                returncode=1, cmd="test", stderr=stderr_output
            )

            with pytest.raises(
                UnidentifiedImageError,
                match="resvg failed to render SVG:.*src/rust/lib.rs",
            ):
                image_module._render_svg_safely(Path("/path/to/malformed.svg"), None)

    def test_error_handling_generic_stderr(self) -> None:
        """Test error handling with generic stderr message."""
        stderr_output = b"Some generic error message"

        with patch("esphome.components.image.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(
                returncode=1, cmd="test", stderr=stderr_output
            )

            with pytest.raises(
                UnidentifiedImageError,
                match="resvg failed to render SVG: Some generic error message",
            ):
                image_module._render_svg_safely(Path("/path/to/malformed.svg"), None)

    def test_error_handling_python_exception_in_subprocess(self) -> None:
        """Test error handling when Python exception occurs in subprocess."""
        stderr_output = b"FileNotFoundError: [Errno 2] No such file or directory: '/path/to/missing.svg'"

        with patch("esphome.components.image.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(
                returncode=1, cmd="test", stderr=stderr_output
            )

            with pytest.raises(
                UnidentifiedImageError,
                match="resvg failed to render SVG:.*FileNotFoundError",
            ):
                image_module._render_svg_safely(Path("/path/to/missing.svg"), None)

    def test_subprocess_capture_output_flag(self) -> None:
        """Test that subprocess is called with capture_output=True."""
        mock_result = MagicMock()
        mock_result.stdout = b"fake_png_data"

        with patch(
            "esphome.components.image.subprocess.run", return_value=mock_result
        ) as mock_run:
            image_module._render_svg_safely(Path("/path/to/test.svg"), None)

            mock_run.assert_called_once()
            call_kwargs = mock_run.call_args[1]
            assert call_kwargs["capture_output"] is True
            assert call_kwargs["check"] is True
