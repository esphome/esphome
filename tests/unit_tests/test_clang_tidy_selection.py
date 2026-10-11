"""Test the clang-tidy script's selection of host and production sources."""

import os
from pathlib import Path
import runpy
import sys

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    ("source", "selected"),
    [
        ("tests/component_tests/midea/fixtures/purifier_test.cpp", False),
        ("esphome/components/midea/purifier_switch.cpp", True),
    ],
)
@pytest.mark.parametrize("separator", ["/", "\\"])
def test_midea_source_selection(
    monkeypatch: pytest.MonkeyPatch, source: str, selected: bool, separator: str
) -> None:
    """Run real tracked-file selection, stopping before MCU dependency setup."""
    monkeypatch.chdir(REPO_ROOT)
    monkeypatch.syspath_prepend(str(REPO_ROOT / "script"))
    main = runpy.run_path(str(REPO_ROOT / "script/clang-tidy"))["main"]
    relpath = os.path.relpath

    def relative_path(path: str | Path, start: str | Path) -> str:
        return relpath(path, start).replace("\\", "/").replace("/", separator)

    monkeypatch.setattr(os.path, "relpath", relative_path)
    monkeypatch.setattr(sys, "argv", ["clang-tidy", f"^{source}$"])

    class DependencySetupReached(Exception):
        """The selected source requires a compiler environment."""

    def stop_at_dependency_setup(environment: str) -> None:
        raise DependencySetupReached(environment)

    monkeypatch.setitem(main.__globals__, "load_idedata", stop_at_dependency_setup)
    if selected:
        with pytest.raises(DependencySetupReached):
            main()
    else:
        assert main() == 0
