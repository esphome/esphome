"""Tests for esphome.espidf.clang_tidy tidy-project generation."""

from pathlib import Path

from esphome.espidf.clang_tidy import _Settings, _write_tidy_project

REPO_ROOT = Path(__file__).resolve().parents[2]


def _settings(idf_target: str) -> _Settings:
    return _Settings(
        idf_target=idf_target,
        variant=idf_target.upper(),
        idf_version="5.5.4",
        target_framework="espidf",
        platform_defines=(
            "USE_ESP32",
            f"USE_ESP32_VARIANT_{idf_target.upper()}",
            "USE_ESP_IDF",
        ),
        framework_deps={},
    )


def test_write_tidy_project_copies_base_sdkconfig(tmp_path: Path) -> None:
    """The shared sdkconfig.defaults is always copied; no per-target file for esp32."""
    _write_tidy_project(tmp_path, [], {}, _settings("esp32"))

    assert (tmp_path / "sdkconfig.defaults").is_file()
    # esp32 has no sdkconfig.defaults.esp32, so nothing extra is copied.
    assert not (tmp_path / "sdkconfig.defaults.esp32").exists()


def test_write_tidy_project_copies_per_target_sdkconfig(tmp_path: Path) -> None:
    """A repo-root sdkconfig.defaults.<target> is also copied into the build dir."""
    _write_tidy_project(tmp_path, [], {}, _settings("esp32c6"))

    target = tmp_path / "sdkconfig.defaults.esp32c6"
    assert (tmp_path / "sdkconfig.defaults").is_file()
    assert target.is_file()
    assert target.read_text(encoding="utf-8") == (
        REPO_ROOT / "sdkconfig.defaults.esp32c6"
    ).read_text(encoding="utf-8")
