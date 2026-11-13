"""Unit tests for script/move_shared_constants.py."""

from collections.abc import Callable
import json
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

SCRIPT_DIR = (Path(__file__).parent / ".." / ".." / "script").resolve()
sys.path.insert(0, str(SCRIPT_DIR))

import move_shared_constants as mod  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent

CORE_CONST = 'CONF_AAA = "aaa"\nCONF_ZZZ = "zzz"\n'
COMPONENT_CONST = 'CONF_BBB = "bbb"\n'
CI_CUSTOM = "CONST_PY_MAX_CONF = 5\n"

Writer = Callable[[str, str], None]


def _git(root: Path, *args: str) -> None:
    subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True)


@pytest.fixture(name="repo")
def repo_fixture(tmp_path: Path) -> Path:
    """A tiny git repository with the two homes and ci-custom.py."""
    _git(tmp_path, "init", "-q")
    shutil.copy(REPO_ROOT / "pyproject.toml", tmp_path / "pyproject.toml")
    write(tmp_path, "esphome/const.py", CORE_CONST)
    write(tmp_path, "esphome/components/const/__init__.py", COMPONENT_CONST)
    write(tmp_path, "script/ci-custom.py", CI_CUSTOM)
    return tmp_path


def read(root: Path, rel: str) -> str:
    return (root / rel).read_bytes().decode("utf-8")


def write(root: Path, rel: str, content: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content.encode("utf-8"))
    _git(root, "add", rel)


def add_definers(root: Path, name: str, value: str, count: int = 3) -> None:
    for i in range(count):
        write(
            root,
            f"esphome/components/c{i}/__init__.py",
            f'import os\n\n{name} = "{value}"\n\nX = os, {name}\n',
        )


def test_find_threshold(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    add_definers(repo, "CONF_MANY", "many", 3)
    write(repo, "esphome/components/few1/__init__.py", 'CONF_FEW = "few"\n')
    write(repo, "esphome/components/few2/__init__.py", 'CONF_FEW = "few"\n')
    assert mod.main(["find", "--root", str(repo)]) == 0
    assert capsys.readouterr().out == '["CONF_MANY"]\n'


def test_find_filters_invalid_and_homed(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    add_definers(repo, "CONF_BAD", "Wrong")
    add_definers(repo, "CONF_AAA", "aaa")
    add_definers(repo, "CONF_BBB", "bbb")
    add_definers(repo, "CONF_DOT_VAL", "dot.val")
    mod.cmd_find(repo)
    assert json.loads(capsys.readouterr().out) == ["CONF_DOT_VAL"]


def test_move_to_component_const(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    add_definers(repo, "CONF_MANY", "many")
    assert mod.main(["move", "CONF_MANY", "--root", str(repo)]) == 0
    assert capsys.readouterr().out == "esphome/components/const/__init__.py\n"
    home = read(repo, "esphome/components/const/__init__.py")
    assert home == 'CONF_BBB = "bbb"\nCONF_MANY = "many"\n'
    assert read(repo, "script/ci-custom.py") == CI_CUSTOM
    text = read(repo, "esphome/components/c0/__init__.py")
    assert "CONF_MANY =" not in text
    assert "from esphome.components.const import CONF_MANY" in text


def test_move_to_core_const_bumps_max(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/core_user.py",
        "from esphome.const import CONF_AAA\n\nX = CONF_MANY\n",
    )
    mod.main(["move", "CONF_MANY", "--root", str(repo)])
    assert capsys.readouterr().out == "esphome/const.py\n"
    assert read(repo, "esphome/const.py") == (
        'CONF_AAA = "aaa"\nCONF_MANY = "many"\nCONF_ZZZ = "zzz"\n'
    )
    assert read(repo, "script/ci-custom.py") == "CONST_PY_MAX_CONF = 6\n"
    assert "from esphome.const import CONF_MANY" in read(
        repo, "esphome/components/c1/__init__.py"
    )


def test_move_to_core_const_inserts_last(repo: Path) -> None:
    add_definers(repo, "CONF_ZZZZ", "zzzz")
    write(repo, "esphome/core_user.py", "X = CONF_ZZZZ\n")
    mod.cmd_move(repo, "CONF_ZZZZ")
    assert read(repo, "esphome/const.py").endswith(
        'CONF_ZZZ = "zzz"\nCONF_ZZZZ = "zzzz"\n'
    )


def test_move_into_empty_home(repo: Path) -> None:
    write(repo, "esphome/components/const/__init__.py", '"""Doc."""\n')
    add_definers(repo, "CONF_MANY", "many")
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/const/__init__.py") == (
        '"""Doc."""\n\nCONF_MANY = "many"\n'
    )


def test_import_merged_into_existing_block(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/__init__.py",
        "from esphome.const import (\n    CONF_ID,\n    CONF_NAME,\n)\n\n"
        'CONF_MANY = "many"\n\nX = CONF_ID, CONF_MANY\n',
    )
    write(
        repo,
        "esphome/components/c1/__init__.py",
        "from esphome.components.const import CONF_BBB\n\n"
        'CONF_MANY = "many"\n\nX = CONF_BBB, CONF_MANY\n',
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c1/__init__.py") == (
        "from esphome.components.const import CONF_BBB, CONF_MANY\n\nX = CONF_BBB, CONF_MANY\n"
    )
    text = read(repo, "esphome/components/c0/__init__.py")
    assert "from esphome.components.const import CONF_MANY\n" in text
    assert "CONF_ID,\n    CONF_NAME,\n" in text or "CONF_ID, CONF_NAME" in text


def test_no_imports_with_docstring(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/__init__.py",
        '"""Doc."""\n\nCONF_MANY = "many"\n\nX = CONF_MANY\n',
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py") == (
        '"""Doc."""\n\nfrom esphome.components.const import CONF_MANY\n\nX = CONF_MANY\n'
    )


def test_no_imports_without_docstring(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo, "esphome/components/c0/__init__.py", 'CONF_MANY = "many"\nX = CONF_MANY\n'
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py") == (
        "from esphome.components.const import CONF_MANY\n\nX = CONF_MANY\n"
    )


def test_non_string_first_statement_is_not_docstring(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/__init__.py",
        'X = 1\nCONF_MANY = "many"\nY = CONF_MANY\n',
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py") == (
        "from esphome.components.const import CONF_MANY\n\nX = 1\nY = CONF_MANY\n"
    )


def test_expression_first_statement_is_not_docstring(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/__init__.py",
        'print(CONF_MANY)\nCONF_MANY = "many"\n',
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py").startswith(
        "from esphome.components.const import CONF_MANY\n"
    )


def test_unused_import_gets_noqa(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(repo, "esphome/components/c0/__init__.py", 'CONF_MANY = "many"\nX = 1\n')
    write(
        repo,
        "esphome/components/other/user.py",
        "from esphome.components.c0 import CONF_MANY\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py") == (
        "from esphome.components.const import CONF_MANY  # noqa: F401  # pylint: disable=unused-import\n\nX = 1\n"
    )


def test_reexport_in_block_with_existing_noqa(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/const.py",
        "from esphome.components.const import CONF_BBB  # noqa: F401\n\n"
        'CONF_MANY = "many"\n',
    )
    write(
        repo,
        "esphome/components/other/user.py",
        "from esphome.components.c0.const import CONF_MANY\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    text = read(repo, "esphome/components/c0/const.py")
    assert "CONF_BBB,  # noqa: F401\n" in text
    assert "    CONF_MANY,  # noqa: F401  # pylint: disable=unused-import\n" in text
    assert (
        subprocess.run(
            [
                sys.executable,
                "-m",
                "ruff",
                "check",
                "--select",
                "F401",
                str(repo / "esphome/components/c0/const.py"),
            ],
            capture_output=True,
            check=False,
        ).returncode
        == 0
    )


def test_noqa_on_same_line_is_kept(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(
        repo,
        "esphome/components/c0/const.py",
        "from esphome.components.const import CONF_BBB  # noqa: F401\n\n"
        'CONF_MANY = "many"\n',
    )
    write(
        repo,
        "esphome/components/other/user.py",
        "from esphome.components.c0.const import CONF_MANY\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    mod.mark_reexport(repo, "esphome/components/c0/const.py", "CONF_MANY")
    text = read(repo, "esphome/components/c0/const.py")
    assert text.count("noqa") == 2


def test_conflicting_values(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(repo, "esphome/components/c0/__init__.py", 'CONF_MANY = "other"\n')
    assert mod.cmd_move(repo, "CONF_MANY") == 1
    assert "conflicting values" in capsys.readouterr().err


def test_invalid_value(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    add_definers(repo, "CONF_MANY", "Many")
    assert mod.cmd_move(repo, "CONF_MANY") == 1
    assert "not a valid constant" in capsys.readouterr().err


def test_no_definers(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert mod.cmd_move(repo, "CONF_NOPE") == 1
    assert "not defined in any file" in capsys.readouterr().err


def test_invalid_name(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert mod.cmd_move(repo, "conf_bad") == 1
    assert "invalid constant name" in capsys.readouterr().err


def test_already_in_home(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    add_definers(repo, "CONF_AAA", "aaa")
    before = read(repo, "esphome/components/c0/__init__.py")
    assert mod.cmd_move(repo, "CONF_AAA") == 0
    captured = capsys.readouterr()
    assert captured.out == ""
    assert "nothing to do" in captured.err
    assert read(repo, "esphome/components/c0/__init__.py") == before


def test_script_runs_as_program(repo: Path) -> None:
    add_definers(repo, "CONF_MANY", "many")
    script = SCRIPT_DIR / "move_shared_constants.py"
    result = subprocess.run(
        [sys.executable, str(script), "find", "--root", str(repo)],
        check=True,
        capture_output=True,
        text=True,
    )
    assert result.stdout == '["CONF_MANY"]\n'


def test_main_module_entry(repo: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    import runpy

    add_definers(repo, "CONF_MANY", "many")
    monkeypatch.setattr(sys, "argv", ["x", "find", "--root", str(repo)])
    with pytest.raises(SystemExit) as exc:
        runpy.run_path(
            str(SCRIPT_DIR / "move_shared_constants.py"), run_name="__main__"
        )
    assert exc.value.code == 0


def test_find_unit_constants(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    add_definers(repo, "UNIT_DEGREE", "\u00b0C", 3)
    for i, name in enumerate(["UNIT_BAD", "UNIT_BAD", "UNIT_BAD"]):
        write(repo, f"esphome/components/bad{i}/__init__.py", f'{name} = "{i}"\n')
    for i in range(3):
        write(repo, f"esphome/components/low{i}/__init__.py", 'UNIT_lower = "x"\n')
    mod.cmd_find(repo)
    assert json.loads(capsys.readouterr().out) == ["UNIT_DEGREE"]


def test_find_unit_two_files_not_reported(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    add_definers(repo, "UNIT_TWO", "two", 2)
    mod.cmd_find(repo)
    assert capsys.readouterr().out == "[]\n"


def test_find_value_agreement_uses_parsed_string(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    write(repo, "esphome/components/c0/__init__.py", "UNIT_SAME = 'same'\n")
    write(repo, "esphome/components/c1/__init__.py", 'UNIT_SAME = "same"\n')
    write(repo, "esphome/components/c2/__init__.py", 'UNIT_SAME = "same"\n')
    mod.cmd_find(repo)
    assert json.loads(capsys.readouterr().out) == ["UNIT_SAME"]


def test_literal_value_only_accepts_strings() -> None:
    assert mod.literal_value("'abc'") == "abc"
    assert mod.literal_value('"a" "b"') == "ab"
    assert mod.literal_value('"a" + SUFFIX + "b"') is None
    assert mod.literal_value('"a" "b" +') is None
    assert mod.literal_value("b'abc'") is None


def test_find_skips_expression_values(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    for i in range(3):
        write(
            repo,
            f"esphome/components/e{i}/__init__.py",
            'SUFFIX = "x"\nUNIT_EXPR = "a" + SUFFIX + "b"\n',
        )
    mod.cmd_find(repo)
    assert capsys.readouterr().out == "[]\n"


def test_move_rejects_expression_values(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    for i in range(3):
        write(
            repo,
            f"esphome/components/e{i}/__init__.py",
            'SUFFIX = "x"\nUNIT_EXPR = "a" + SUFFIX + "b"\n',
        )
    assert mod.cmd_move(repo, "UNIT_EXPR") == 1
    assert "not a valid constant" in capsys.readouterr().err
    assert "UNIT_EXPR" not in read(repo, "esphome/components/const/__init__.py")


def test_move_unit_to_component_const_keeps_unicode(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    write(
        repo,
        "esphome/components/const/__init__.py",
        'CONF_BBB = "bbb"\n\nUNIT_ALPHA = "a"\nUNIT_OMEGA = "o"\n',
    )
    add_definers(repo, "UNIT_MICRO", "\u00b5g/m\u00b3")
    assert mod.cmd_move(repo, "UNIT_MICRO") == 0
    assert capsys.readouterr().out == "esphome/components/const/__init__.py\n"
    assert (repo / "esphome/components/const/__init__.py").read_bytes() == (
        'CONF_BBB = "bbb"\n\nUNIT_ALPHA = "a"\nUNIT_MICRO = "\u00b5g/m\u00b3"\n'
        'UNIT_OMEGA = "o"\n'
    ).encode()
    assert "from esphome.components.const import UNIT_MICRO" in read(
        repo, "esphome/components/c0/__init__.py"
    )


def test_move_unit_to_core_const_does_not_bump_max(repo: Path) -> None:
    add_definers(repo, "UNIT_DEG", "\u00b0C")
    write(repo, "esphome/core_user.py", "X = UNIT_DEG\n")
    mod.cmd_move(repo, "UNIT_DEG")
    assert read(repo, "esphome/const.py") == (CORE_CONST + '\nUNIT_DEG = "\u00b0C"\n')
    assert read(repo, "script/ci-custom.py") == CI_CUSTOM


def test_prefix_block_missing_in_middle_of_home(repo: Path) -> None:
    write(
        repo,
        "esphome/components/const/__init__.py",
        'CONF_BBB = "bbb"\n\nICON_X = "mdi:x"\n',
    )
    add_definers(repo, "UNIT_NEW", "new")
    mod.cmd_move(repo, "UNIT_NEW")
    assert read(repo, "esphome/components/const/__init__.py") == (
        'CONF_BBB = "bbb"\n\nICON_X = "mdi:x"\n\nUNIT_NEW = "new"\n'
    )


def test_prefix_block_missing_before_other_content(repo: Path) -> None:
    write(
        repo,
        "esphome/components/const/__init__.py",
        'CONF_BBB = "bbb"\nOTHER = 1\n',
    )
    add_definers(repo, "UNIT_NEW", "new")
    mod.cmd_move(repo, "UNIT_NEW")
    assert read(repo, "esphome/components/const/__init__.py") == (
        'CONF_BBB = "bbb"\n\nUNIT_NEW = "new"\n\nOTHER = 1\n'
    )


def test_prefix_block_missing_before_blank_line(repo: Path) -> None:
    write(
        repo,
        "esphome/components/const/__init__.py",
        'CONF_BBB = "bbb"\n\nOTHER = 1\n',
    )
    add_definers(repo, "UNIT_NEW", "new")
    mod.cmd_move(repo, "UNIT_NEW")
    assert read(repo, "esphome/components/const/__init__.py") == (
        'CONF_BBB = "bbb"\n\nUNIT_NEW = "new"\n\nOTHER = 1\n'
    )


def test_unit_conflicting_values(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    add_definers(repo, "UNIT_X", "a")
    write(repo, "esphome/components/c0/__init__.py", 'UNIT_X = "b"\n')
    assert mod.cmd_move(repo, "UNIT_X") == 1
    assert "conflicting values" in capsys.readouterr().err


def only_definer(repo: Path, rel: str, content: str) -> None:
    add_definers(repo, "CONF_MANY", "many")
    write(repo, rel, content)


def test_unused_and_not_imported_is_only_deleted(repo: Path) -> None:
    only_definer(
        repo, "esphome/components/c0/__init__.py", 'CONF_MANY = "many"\nX = 1\n'
    )
    write(
        repo,
        "esphome/components/c1/user.py",
        "# CONF_MANY is mentioned only here\nfrom os import path as CONF_MANY2\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/__init__.py") == "X = 1\n"


def test_unused_but_imported_absolute_keeps_import(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/const.py", 'CONF_MANY = "many"\n')
    write(
        repo,
        "esphome/components/other/user.py",
        "from esphome.components.c0.const import CONF_MANY, CONF_BBB\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/const.py") == (
        "from esphome.components.const import CONF_MANY  # noqa: F401  # pylint: disable=unused-import\n"
    )


def test_unused_but_imported_relative_same_package(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/const.py", 'CONF_MANY = "many"\n')
    write(repo, "esphome/components/c0/user.py", "from .const import CONF_MANY\n")
    mod.cmd_move(repo, "CONF_MANY")
    assert "noqa: F401" in read(repo, "esphome/components/c0/const.py")


def test_unused_but_imported_relative_parent_package(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/const.py", 'CONF_MANY = "many"\n')
    write(
        repo, "esphome/components/other/user.py", "from ..c0.const import CONF_MANY\n"
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert "noqa: F401" in read(repo, "esphome/components/c0/const.py")


def test_unused_package_init_imported_via_package(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/__init__.py", 'CONF_MANY = "many"\n')
    write(repo, "esphome/components/c0/sub.py", "from . import CONF_MANY\n")
    mod.cmd_move(repo, "CONF_MANY")
    assert "noqa: F401" in read(repo, "esphome/components/c0/__init__.py")


def test_relative_import_from_package_init(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/const.py", 'CONF_MANY = "many"\n')
    write(repo, "esphome/components/c0/__init__.py", "from .const import CONF_MANY\n")
    mod.cmd_move(repo, "CONF_MANY")
    assert "noqa: F401" in read(repo, "esphome/components/c0/const.py")


def test_unparseable_file_mentioning_name_is_skipped(repo: Path) -> None:
    only_definer(repo, "esphome/components/c0/const.py", 'CONF_MANY = "many"\n')
    write(repo, "esphome/components/c1/broken.py", "CONF_MANY = (\n")
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/c0/const.py") == ""


OPENTHERM_CONST = 'CONF_MANY = "many"\n'


def module_user(
    repo: Path, content: str, rel: str = "esphome/components/c1/user.py"
) -> None:
    only_definer(repo, "esphome/components/c0/const.py", OPENTHERM_CONST)
    write(repo, rel, content)
    mod.cmd_move(repo, "CONF_MANY")


@pytest.mark.parametrize(
    ("rel", "content"),
    [
        ("esphome/components/c0/user.py", "from . import const\nX = const.CONF_MANY\n"),
        (
            "esphome/components/c1/user.py",
            "from .. import c0\nfrom ..c0 import const as c\nX = c.CONF_MANY\n",
        ),
        (
            "esphome/components/c1/user.py",
            "from esphome.components.c0 import const\nX = const.CONF_MANY\n",
        ),
        (
            "esphome/components/c1/user.py",
            "import esphome.components.c0.const\nX = esphome.components.c0.const.CONF_MANY\n",
        ),
        (
            "esphome/components/c1/user.py",
            "import esphome.components.c0.const as k\nX = k.CONF_MANY\n",
        ),
    ],
)
def test_module_qualified_consumer_keeps_reexport(
    repo: Path, rel: str, content: str
) -> None:
    module_user(repo, content, rel)
    assert read(repo, "esphome/components/c0/const.py") == (
        "from esphome.components.const import CONF_MANY  # noqa: F401  # pylint: disable=unused-import\n"
    )


def test_opentherm_pattern(repo: Path) -> None:
    write(
        repo,
        "esphome/components/ot/const.py",
        'CONF_MANY = "many"\nCONF_OTHER = "other"\n',
    )
    write(
        repo,
        "esphome/components/ot/sensor.py",
        "from . import const\n\nX = {const.CONF_MANY: const.CONF_OTHER}\n",
    )
    mod.cmd_move(repo, "CONF_MANY")
    assert read(repo, "esphome/components/ot/const.py") == (
        "from esphome.components.const import CONF_MANY  # noqa: F401  # pylint: disable=unused-import\n\n"
        'CONF_OTHER = "other"\n'
    )


def test_unrelated_attribute_access_does_not_keep_import(repo: Path) -> None:
    module_user(
        repo,
        "from . import const\nimport os\n"
        "X = const.OTHER, os.path.CONF_MANY, foo().CONF_MANY, other.CONF_MANY\n",
    )
    assert read(repo, "esphome/components/c0/const.py") == ""


def test_two_moves_in_a_row_share_files(
    repo: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    for i in range(3):
        write(
            repo,
            f"esphome/components/c{i}/__init__.py",
            'CONF_MANY = "many"\nCONF_MORE = "more"\n\nX = CONF_MANY, CONF_MORE\n',
        )
    write(repo, "esphome/core_user.py", "X = CONF_MANY, CONF_MORE\n")
    mod.cmd_move(repo, "CONF_MANY")
    mod.cmd_move(repo, "CONF_MORE")
    assert capsys.readouterr().out == "esphome/const.py\nesphome/const.py\n"
    assert read(repo, "esphome/const.py") == (
        'CONF_AAA = "aaa"\nCONF_MANY = "many"\nCONF_MORE = "more"\nCONF_ZZZ = "zzz"\n'
    )
    assert read(repo, "script/ci-custom.py") == "CONST_PY_MAX_CONF = 7\n"
    assert read(repo, "esphome/components/c0/__init__.py") == (
        "from esphome.const import CONF_MANY, CONF_MORE\n\nX = CONF_MANY, CONF_MORE\n"
    )
