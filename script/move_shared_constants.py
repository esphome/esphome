#!/usr/bin/env python3
"""Move CONF_ and UNIT_ constants that are defined in many files into a shared home.

script/ci-custom.py fails when a CONF_ or UNIT_ constant is defined in three
or more Python files. This script finds those constants and performs the move so a
bot can open one pull request per constant.

Subcommands:

    find --root DIR   Print a compact JSON list of constants that should be
                      moved. DIR may be an untrusted checkout: its files are
                      only read as text, nothing in it is imported or run.
    move NAME         Move NAME into esphome/const.py (when core code outside
                      esphome/components/ uses it) or into
                      esphome/components/const/__init__.py. Prints the chosen
                      home path to stdout only when files were changed. When
                      NAME is already in a home, nothing is changed and
                      nothing is printed to stdout (exit code 0). A file that
                      defined NAME keeps an import of it only when it still
                      uses NAME or another file imports NAME from it.
"""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path
import re
import subprocess
import sys

DEF_RE = re.compile(
    r'^((?:CONF|UNIT)_[a-zA-Z0-9_]+)\s*=\s*([\'"].*?[\'"])\s*?$', re.MULTILINE
)
NAME_RE = re.compile(r"^(?:CONF|UNIT)_[A-Z0-9_]+$")
VALUE_RE = re.compile(r"^[a-z0-9_.]+$")
MAX_CONF_RE = re.compile(r"^(CONST_PY_MAX_CONF = )(\d+)$", re.MULTILINE)

CORE_HOME = "esphome/const.py"
COMPONENT_HOME = "esphome/components/const/__init__.py"
CORE_MODULE = "esphome.const"
COMPONENT_MODULE = "esphome.components.const"
HOMES = (CORE_HOME, COMPONENT_HOME)
CI_CUSTOM = "script/ci-custom.py"
MIN_DEFINITIONS = 3
PREFIXES = ("CONF_", "ICON_", "UNIT_")
REEXPORT_NOQA = "  # noqa: F401  # pylint: disable=unused-import"


def tracked_py_files(root: Path) -> list[str]:
    """Return the tracked Python files of the repository at root."""
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "*.py"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return out.splitlines()


def read_text(root: Path, rel: str) -> str:
    """Read a tracked file as text."""
    return (root / rel).read_text(encoding="utf-8")


def literal_value(literal: str) -> str:
    """Return the string a source literal evaluates to (the literal itself if it does not)."""
    try:
        value = ast.literal_eval(literal)
    except (ValueError, SyntaxError):
        return literal
    return value if isinstance(value, str) else literal


def is_valid_constant(name: str, values: set[str]) -> bool:
    """Check the name, that all definitions agree, and that CONF_ values match the name."""
    if NAME_RE.match(name) is None or len(values) != 1:
        return False
    if name.startswith("UNIT_"):
        return True
    value = next(iter(values))
    return (
        VALUE_RE.match(value) is not None
        and value.replace(".", "_") == name[5:].lower()
    )


def homed_names(root: Path) -> set[str]:
    """Return the constant names already defined in either home."""
    return {
        m.group(1) for home in HOMES for m in DEF_RE.finditer(read_text(root, home))
    }


def find_definitions(root: Path, files: list[str]) -> dict[str, dict[str, set[str]]]:
    """Map each constant name to {file: {source literals}} for files outside the homes."""
    result: dict[str, dict[str, set[str]]] = {}
    for rel in files:
        if rel in HOMES:
            continue
        for m in DEF_RE.finditer(read_text(root, rel)):
            result.setdefault(m.group(1), {}).setdefault(rel, set()).add(m.group(2))
    return result


def parsed_values(files: dict[str, set[str]]) -> set[str]:
    """Return the distinct string values across all definitions."""
    return {literal_value(lit) for literals in files.values() for lit in literals}


def cmd_find(root: Path) -> int:
    """Print the JSON list of constants that CI wants moved."""
    existing = homed_names(root)
    names = [
        name
        for name, files in find_definitions(root, tracked_py_files(root)).items()
        if len(files) >= MIN_DEFINITIONS
        and name not in existing
        and is_valid_constant(name, parsed_values(files))
    ]
    print(json.dumps(sorted(names), separators=(",", ":")))
    return 0


def choose_home(root: Path, name: str, files: list[str]) -> str:
    """Pick esphome/const.py when core code outside components uses name."""
    word = re.compile(rf"\b{name}\b")
    for rel in files:
        if (
            rel.startswith("esphome/")
            and not rel.startswith("esphome/components/")
            and word.search(read_text(root, rel))
        ):
            return CORE_HOME
    return COMPONENT_HOME


def sort_key(line: str) -> str:
    """Sort key used by lint_const_ordered in script/ci-custom.py."""
    return line.replace("_", " ")


def add_to_home(root: Path, home: str, name: str, literal: str) -> None:
    """Insert the definition into the home in sorted position within its prefix block."""
    new_line = f"{name} = {literal}"
    lines = read_text(root, home).splitlines()
    positions = [i for i, line in enumerate(lines) if line.startswith(name[:5])]
    if positions:
        at = next(
            (i for i in positions if sort_key(lines[i]) > sort_key(new_line)),
            positions[-1] + 1,
        )
        lines.insert(at, new_line)
    elif const_lines := [
        i for i, line in enumerate(lines) if line.startswith(PREFIXES)
    ]:
        at = const_lines[-1] + 1
        block = ["", new_line]
        if at < len(lines) and lines[at].strip():
            block.append("")
        lines[at:at] = block
    else:
        lines.append(new_line)
    (root / home).write_text("\n".join(lines) + "\n", encoding="utf-8")


def bump_max_conf(root: Path) -> None:
    """Increase CONST_PY_MAX_CONF in script/ci-custom.py by one."""
    path = root / CI_CUSTOM
    path.write_text(
        MAX_CONF_RE.sub(
            lambda m: f"{m.group(1)}{int(m.group(2)) + 1}",
            path.read_text(encoding="utf-8"),
            count=1,
        ),
        encoding="utf-8",
    )


def module_name(rel: str) -> str:
    """Return the dotted module name of a repository file (a package for __init__.py)."""
    parts = rel.removesuffix(".py").split("/")
    if parts[-1] == "__init__":
        parts.pop()
    return ".".join(parts)


def imported_from(root: Path, files: list[str], name: str) -> set[str]:
    """Return the modules that some tracked file imports name from."""
    modules: set[str] = set()
    for rel in files:
        text = read_text(root, rel)
        if name not in text:
            continue
        try:
            tree = ast.parse(text)
        except SyntaxError:
            continue
        package = module_name(rel).split(".")
        if not rel.endswith("/__init__.py"):
            package.pop()
        for node in ast.walk(tree):
            if not isinstance(node, ast.ImportFrom) or all(
                alias.name != name for alias in node.names
            ):
                continue
            base = package[: len(package) - node.level + 1] if node.level else []
            modules.add(".".join([*base, *([node.module] if node.module else [])]))
    return modules


def replace_definition(
    root: Path, rel: str, name: str, import_line: str, importers: set[str]
) -> bool:
    """Delete the definition of name from a file and import it if still needed.

    The import is kept when the file uses name or another file imports it from
    this file's module. Returns whether an import was added.
    """
    text = read_text(root, rel)
    text = re.sub(rf"^{name}\s*=.*\n?", "", text, count=1, flags=re.MULTILINE)
    tree = ast.parse(text)
    used = any(isinstance(n, ast.Name) and n.id == name for n in ast.walk(tree))
    if not used and module_name(rel) not in importers:
        (root / rel).write_text(text, encoding="utf-8")
        return False
    lines = text.splitlines()
    body = tree.body
    imports = [n for n in body if isinstance(n, (ast.Import, ast.ImportFrom))]
    if imports:
        at = imports[-1].end_lineno
    elif (
        body
        and isinstance(body[0], ast.Expr)
        and isinstance(body[0].value, ast.Constant)
        and isinstance(body[0].value.value, str)
    ):
        at = body[0].end_lineno
    else:
        at = 0
    lines[at:at] = [import_line] if imports or at == 0 else ["", import_line]
    (root / rel).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return True


def mark_reexport(root: Path, rel: str, name: str) -> None:
    """Add a noqa to the import of name when the file never uses it (a re-export)."""
    text = read_text(root, rel)
    tree = ast.parse(text)
    if any(isinstance(n, ast.Name) and n.id == name for n in ast.walk(tree)):
        return
    lines = text.splitlines()
    for node in ast.walk(tree):
        if not isinstance(node, ast.ImportFrom):
            continue
        for alias in node.names:
            if alias.name == name and "noqa" not in lines[alias.lineno - 1]:
                lines[alias.lineno - 1] += REEXPORT_NOQA
    (root / rel).write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_ruff(root: Path, files: list[str]) -> None:
    """Sort imports and format so the new import merges into existing ones."""
    for args in (
        ["check", "--select", "I", "--fix", "--exit-zero", "--quiet"],
        ["format", "--quiet"],
    ):
        subprocess.run(
            [sys.executable, "-m", "ruff", *args, *files], cwd=root, check=True
        )


def cmd_move(root: Path, name: str) -> int:
    """Move one constant into its home."""
    if not NAME_RE.match(name):
        print(f"error: invalid constant name {name!r}", file=sys.stderr)
        return 1
    if name in homed_names(root):
        print(f"{name} is already in a shared home, nothing to do", file=sys.stderr)
        return 0
    definers = find_definitions(root, tracked_py_files(root)).get(name, {})
    if not definers:
        print(f"error: {name} is not defined in any file", file=sys.stderr)
        return 1
    values = parsed_values(definers)
    if len(values) != 1:
        print(f"error: {name} has conflicting values {sorted(values)}", file=sys.stderr)
        return 1
    if not is_valid_constant(name, values):
        print(
            f"error: {name} = {values.pop()!r} is not a valid constant", file=sys.stderr
        )
        return 1
    literal = min(lit for literals in definers.values() for lit in literals)

    home = choose_home(root, name, tracked_py_files(root))
    add_to_home(root, home, name, literal)
    if home == CORE_HOME:
        if name.startswith("CONF_"):
            bump_max_conf(root)
        import_line = f"from {CORE_MODULE} import {name}"
    else:
        import_line = f"from {COMPONENT_MODULE} import {name}"
    files = tracked_py_files(root)
    importers = imported_from(root, files, name)
    imported = [
        rel
        for rel in definers
        if replace_definition(root, rel, name, import_line, importers)
    ]
    run_ruff(root, [*definers, home])
    for rel in imported:
        mark_reexport(root, rel, name)
    run_ruff(root, [*definers])
    print(home)
    return 0


def main(argv: list[str] | None = None) -> int:
    """Entry point."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n", maxsplit=1)[0])
    sub = parser.add_subparsers(dest="command", required=True)
    find = sub.add_parser("find", help="list constants that should be moved")
    find.add_argument("--root", type=Path, required=True)
    move = sub.add_parser("move", help="move one constant to its shared home")
    move.add_argument("name")
    move.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args(argv)
    if args.command == "find":
        return cmd_find(args.root)
    return cmd_move(args.root, args.name)


if __name__ == "__main__":
    sys.exit(main())
