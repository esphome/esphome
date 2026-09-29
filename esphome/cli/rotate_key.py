"""``esphome rotate-key``."""

from __future__ import annotations

import argparse
import sys

from esphome import espota2
from esphome.components.noise import (
    generate_encryption_key,
    static_encryption_key,
    validate_encryption_key,
)
from esphome.config_validation import Invalid
from esphome.const import CONF_API, CONF_PASSWORD, CONF_PORT
from esphome.core import CORE, EsphomeError
from esphome.log import AnsiFore, color
from esphome.ota_key_edit import locate_key_edits, old_key_edit
from esphome.types import ConfigType
from esphome.upload_targets import PortType, get_port_type
from esphome.util import (
    ESPHOME_COMMAND,
    read_secret_line,
    run_external_process,
    safe_input,
    safe_print,
)
from esphome.yaml_edit import RestoreError, apply_line_edits, restore_files

PRECHECK_TIMEOUT = 15.0


def command_rotate_key(args: argparse.Namespace, config: ConfigType) -> int | None:
    """Write a new key, build with it, upload with the current one, confirm."""
    # The device helpers live with the other upload commands
    from esphome.__main__ import (
        Purpose,
        _esphome_ota_conf,
        _resolve_network_devices,
        choose_upload_log_host,
    )

    def fail(message: str) -> int:
        safe_print(color(AnsiFore.BOLD_RED, message))
        return 1

    ota_conf = _esphome_ota_conf(config)
    if ota_conf is None:
        return fail("rotate-key needs the esphome OTA platform in the configuration")
    old_key = static_encryption_key(ota_conf)
    if old_key is None:
        return fail(
            "rotate-key needs 'encryption:' under the esphome OTA platform with a "
            "key in the yaml; a key provisioned at runtime is rotated from Home "
            "Assistant"
        )
    if CORE.is_host:
        return fail("rotate-key is for devices updated over the air")
    old_key = str(old_key)
    remote_port = int(ota_conf[CONF_PORT])

    devices = choose_upload_log_host(
        default=args.device, check_default=None, purpose=Purpose.UPLOADING
    )
    if get_port_type(devices[0]) in (PortType.SERIAL, PortType.BOOTSEL):
        return fail(
            "rotate-key works over the air; on a serial connection change the "
            "key in the yaml and run 'esphome run' instead"
        )
    network_devices = _resolve_network_devices(devices, config, args)

    if getattr(args, "prompt_new_key", False):
        new_key = read_secret_line("New OTA encryption key: ")
    else:
        new_key = generate_encryption_key()
    try:
        validate_encryption_key(new_key)
    except Invalid as err:
        return fail(f"Invalid new key: {err}")
    if new_key == old_key:
        return fail("The new key is the same as the current one")

    try:
        # The previous key stays in the config as old_key so an install still
        # reaches the device if it ends up running either key
        edits = [*locate_key_edits(old_key, new_key), *old_key_edit(old_key)]
    except EsphomeError as err:
        return fail(str(err))

    warnings = []
    # With the recommended shape the ota inherits the api key, so rotating
    # it changes what Home Assistant connects with
    if str(static_encryption_key(config.get(CONF_API) or {})) == old_key:
        warnings.append(
            "The api encryption key changes too; Home Assistant will ask for "
            "the new key after the install."
        )
    shared: dict[str, set[str]] = {}
    for edit in edits:
        what = f"secret '{edit.secret}'" if edit.secret else edit.path.name
        for path in edit.shared_with:
            shared.setdefault(
                str(path.relative_to(CORE.config_dir.resolve())), set()
            ).add(what)
    if unchecked := sorted({what for edit in edits for what in edit.unchecked}):
        warnings.append(
            "Could not check every configuration for shared use: "
            + "; ".join(unchecked)
        )
    if shared:
        warnings.append(
            "Other configurations share what is rewritten: "
            + "; ".join(
                f"{p} ({', '.join(sorted(w))})" for p, w in sorted(shared.items())
            )
            + ". Their devices keep the previous key until their next install; "
            "make sure those configurations carry it as old_key."
        )
    if warnings:
        for warning in warnings:
            safe_print(color(AnsiFore.BOLD_YELLOW, warning))
        if not getattr(args, "yes", False):
            if not sys.stdin.isatty():
                return fail("Confirm with --yes when there is no terminal")
            if safe_input("Continue? [y/N] ").strip().lower() not in ("y", "yes"):
                return 1

    safe_print("Checking that the device accepts the current key...")
    if not espota2.probe_ota_key(
        network_devices,
        remote_port,
        old_key,
        timeout=PRECHECK_TIMEOUT,
        retry_rejected=False,
    ):
        return fail(
            "The device did not complete an encrypted handshake with the current "
            "key (see the warning above for the reason). If it is reachable, it "
            "has to run the key in the yaml on ESPHome 2026.9.0 or newer; install "
            "the current configuration first, then rotate."
        )

    # Both keys are shown only when a file could not be put back
    keys = f"Previous key: {old_key}\nNew key: {new_key}"
    try:
        originals = apply_line_edits(edits)
    except RestoreError as err:
        safe_print(keys)
        return fail(str(err))
    except EsphomeError as err:
        return fail(str(err))
    # From here every exit restores or reports, so nothing sits outside the try
    uploaded = False
    try:
        safe_print(
            "Wrote the new key to "
            + ", ".join(color(AnsiFore.CYAN, str(p)) for p in originals)
        )
        # Global options go before the subcommand; the compile parser is strict
        cli_args = ["--dashboard"] if CORE.dashboard else []
        if toolchain := getattr(args, "toolchain", None):
            cli_args += ["--toolchain", str(toolchain)]
        for key, value in getattr(args, "substitution", None) or []:
            cli_args += ["-s", key, value]
        cli_args += ["compile", str(CORE.config_path)]
        if run_external_process(*ESPHOME_COMMAND, *cli_args) != 0:
            return fail("Compiling with the new key failed")

        def device_reached() -> None:
            # The upload may commit from here on; the edited config reaches
            # the device on either key
            nonlocal uploaded
            uploaded = True

        rc, _ = espota2.run_ota(
            network_devices,
            remote_port,
            ota_conf.get(CONF_PASSWORD),
            CORE.firmware_bin,
            noise_psk=old_key,
            plaintext_fallback=False,
            on_connect=device_reached,
        )
        if rc != 0:
            return fail(
                "Uploading with the current key failed. The configuration keeps "
                "the new key with the previous one as old_key, so the next "
                "install reaches the device whichever key it runs."
                if uploaded
                else "The upload did not reach the device."
            )
        safe_print("Waiting for the device to come back with the new key...")
        if not espota2.probe_ota_key(network_devices, remote_port, new_key):
            return fail(
                "The device did not answer with the new key. The configuration "
                "keeps it, with the previous key as old_key, so the next install "
                "reaches the device either way."
            )
    except KeyboardInterrupt:
        return fail(
            "Interrupted; the device's key was not confirmed"
            if uploaded
            else "Interrupted before the upload"
        )
    finally:
        # Before the upload nothing changed on the device; after it the device
        # most likely runs the new key, and old_key covers the other case
        if not uploaded:
            try:
                restore_files(originals)
            except EsphomeError as err:
                safe_print(color(AnsiFore.BOLD_RED, str(err)))
                safe_print(keys)
            else:
                safe_print(
                    color(
                        AnsiFore.BOLD_YELLOW,
                        "Restored the previous key in "
                        + ", ".join(str(p) for p in originals),
                    )
                )
        else:
            safe_print(f"New OTA encryption key: {color(AnsiFore.CYAN, new_key)}")

    safe_print(color(AnsiFore.BOLD_GREEN, "SUCCESS"))
    return 0
