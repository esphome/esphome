"""``esphome rename``."""

from __future__ import annotations

import argparse
from pathlib import Path
import re

from esphome import yaml_edit, yaml_util
from esphome.const import (
    ALLOWED_NAME_CHARS,
    CONF_ESPHOME,
    CONF_NAME,
    CONF_SUBSTITUTIONS,
)
from esphome.core import CORE, EsphomeError
from esphome.log import AnsiFore, color
from esphome.types import ConfigType
from esphome.util import ESPHOME_COMMAND, run_external_process, safe_print


def command_rename(args: argparse.Namespace, config: ConfigType) -> int | None:
    """Rename the device: a new file with the name line rewritten, validated
    and installed, then the old file removed."""
    new_name = args.name
    for c in new_name:
        if c not in ALLOWED_NAME_CHARS:
            safe_print(
                color(
                    AnsiFore.BOLD_RED,
                    f"'{c}' is an invalid character for names. Valid characters are: "
                    f"{ALLOWED_NAME_CHARS} (lowercase, no spaces)",
                )
            )
            return 1

    yaml = yaml_util.load_yaml(CORE.config_path)

    def name_edit() -> tuple[str, yaml_edit.LineEdit]:
        """The name and the line to rewrite: the name's own line, or the
        substitution's line it comes from, as a plain value in this file."""
        esphome_conf = yaml.get(CONF_ESPHOME)
        if not isinstance(esphome_conf, dict) or CONF_NAME not in esphome_conf:
            raise EsphomeError(f"no '{CONF_ESPHOME}: {CONF_NAME}:' in the file")
        old_name = str(esphome_conf[CONF_NAME])
        mapping, field = esphome_conf, CONF_NAME
        if match := re.match(r"^\$\{?([a-zA-Z0-9_]+)\}?$", old_name):
            mapping, field = yaml.get(CONF_SUBSTITUTIONS), match.group(1)
            if not isinstance(mapping, dict) or field not in mapping:
                raise EsphomeError(f"the substitution '{field}' is not in the file")
            old_name = str(mapping[field])
        # Only read here; the rewritten text goes to a new file, so the
        # source may live anywhere the config path points to
        source = yaml_edit.source_of(mapping, field)
        if source is None or source[0].resolve() != CORE.config_path.resolve():
            raise EsphomeError(f"'{field}' was not read from {CORE.config_path}")
        doc, line_no = source
        text = yaml_edit.line_at(doc, line_no)
        if (line_match := yaml_edit.field_line_re(field, old_name).match(text)) is None:
            raise EsphomeError(f"'{field}' is not a plain value on {doc}:{line_no + 1}")
        # The new value is always quoted, whatever the old line had
        return old_name, yaml_edit.LineEdit(
            doc, line_no, text, yaml_edit.rewrite(line_match, new_name, '"')
        )

    try:
        old_name, edit = name_edit()
    except EsphomeError as err:
        safe_print(
            color(
                AnsiFore.BOLD_RED,
                f"Complex YAML files cannot be automatically renamed: {err}",
            )
        )
        return 1

    # ``new_name == old_name`` (after substitution resolution) is
    # a no-op rewrite that would still queue a pointless re-flash.
    # Catch it before the path-equality check below — covers the
    # case where the config filename doesn't match the device name
    # (e.g. ``weird-file.yaml`` whose ``esphome.name`` is
    # ``kitchen``; running ``esphome rename weird-file.yaml kitchen``
    # would otherwise just re-flash the same hostname).
    if new_name == old_name:
        safe_print(
            color(
                AnsiFore.BOLD_RED,
                f"'{new_name}' is already the device's name.",
            )
        )
        return 1

    new_path: Path = CORE.config_dir / (new_name + ".yaml")
    if new_path.resolve() == CORE.config_path.resolve():
        safe_print(
            color(
                AnsiFore.BOLD_RED,
                f"'{new_name}' is already the device's name.",
            )
        )
        return 1
    if new_path.exists():
        safe_print(
            color(
                AnsiFore.BOLD_RED,
                f"Cannot rename: {new_path} already exists. "
                "Refusing to overwrite an existing configuration.",
            )
        )
        return 1
    safe_print(
        f"Updating {color(AnsiFore.CYAN, str(CORE.config_path))} to {color(AnsiFore.CYAN, str(new_path))}"
    )
    print()

    try:
        yaml_edit.write_keeping_mode(
            new_path,
            yaml_edit.rewritten_text(yaml_edit.read_text(CORE.config_path), [edit]),
            like=CORE.config_path,
        )
    except EsphomeError as err:
        safe_print(color(AnsiFore.BOLD_RED, f"Rename failed: {err}"))
        try:
            new_path.unlink(missing_ok=True)
        except OSError as unlink_err:
            safe_print(
                color(AnsiFore.BOLD_RED, f"Could not remove {new_path}: {unlink_err}")
            )
        return 1

    rc = run_external_process(*ESPHOME_COMMAND, "config", str(new_path))
    if rc != 0:
        safe_print(color(AnsiFore.BOLD_RED, "Rename failed. Reverting changes."))
        new_path.unlink()
        return 1

    cli_args = [
        "run",
        str(new_path),
        "--no-logs",
        "--device",
        CORE.address,
    ]

    if args.dashboard:
        cli_args.insert(0, "--dashboard")

    try:
        rc = run_external_process(*ESPHOME_COMMAND, *cli_args)
    except KeyboardInterrupt:
        rc = 1
    if rc != 0:
        new_path.unlink()
        return 1

    if CORE.config_path != new_path:
        CORE.config_path.unlink()

    safe_print(color(AnsiFore.BOLD_GREEN, "SUCCESS"))
    print()
    return 0
