"""Tests for esphome.components.nrf52.clang_tidy CMakeLists generation."""

from esphome.components.nrf52.clang_tidy import TIDY_PROJECT_NAME, _tidy_cmakelists


def test_tidy_cmakelists_includes_main_cpp_and_all_sources() -> None:
    content = _tidy_cmakelists("", ["a.cpp", "b.cpp"])

    assert '"main.cpp"' in content
    assert '"a.cpp"' in content
    assert '"b.cpp"' in content


def test_tidy_cmakelists_includes_library_include_dirs() -> None:
    content = _tidy_cmakelists('  "/some/library/include"', [])

    assert '"/some/library/include"' in content


def test_tidy_cmakelists_names_the_project() -> None:
    content = _tidy_cmakelists("", [])

    assert f"project({TIDY_PROJECT_NAME})" in content


def test_tidy_cmakelists_sets_expected_defines() -> None:
    content = _tidy_cmakelists("", [])

    for define in ("USE_ZEPHYR", "USE_NRF52", "CLANG_TIDY"):
        assert define in content
