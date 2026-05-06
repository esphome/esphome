Import("env")  # noqa: F821

import itertools  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import pathlib  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402
from glob import glob  # noqa: E402


def _parse_sdkconfig(sdkconfig_path):
    """Parse sdkconfig file and return a dict of CONFIG_ options."""
    options = {}
    try:
        for line in sdkconfig_path.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, _, value = line.partition("=")
                # Strip surrounding quotes from string values
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                options[key] = value
    except FileNotFoundError:
        pass
    return options


def _generate_v1_verification_key(env):
    """Generate the V1 ECDSA verification key binary and assembly source file.

    Secure Boot V1 embeds the public verification key directly in the app binary
    as a compiled object (via a .S assembly file). The ESP-IDF CMake build generates
    these files via custom commands, but PlatformIO's SCons bridge does not execute
    them. This function replicates that logic:
      1. Extracts the raw public key from the PEM signing key using espsecure.
      2. Generates the .S assembly source that embeds the key bytes.
    """
    build_dir = pathlib.Path(env.subst("$BUILD_DIR"))
    project_dir = pathlib.Path(env.subst("$PROJECT_DIR"))
    pioenv = env.subst("$PIOENV")
    sdkconfig = _parse_sdkconfig(project_dir / f"sdkconfig.{pioenv}")

    if sdkconfig.get("CONFIG_SECURE_SIGNED_APPS_ECDSA_SCHEME") != "y":
        return

    bin_path = build_dir / "signature_verification_key.bin"
    asm_path = build_dir / "signature_verification_key.bin.S"

    # Determine the source of the verification key
    if sdkconfig.get("CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES") == "y":
        # Extract public key from the signing key
        signing_key = sdkconfig.get("CONFIG_SECURE_BOOT_SIGNING_KEY")
        if not signing_key:
            return
        signing_key_path = pathlib.Path(signing_key)
        if not signing_key_path.exists():
            print(f"Error: V1 ECDSA signing key not found: {signing_key_path}")
            env.Exit(1)
            return

        if not bin_path.exists() or bin_path.stat().st_mtime < signing_key_path.stat().st_mtime:
            python_exe = env.subst("$PYTHONEXE")
            result = subprocess.run(
                [python_exe, "-m", "espsecure", "extract_public_key",
                 "--keyfile", str(signing_key_path), str(bin_path)],
                capture_output=True, text=True,
            )
            if result.returncode != 0:
                print(f"Error extracting V1 verification key: {result.stderr}")
                env.Exit(1)
                return
            print(f"Extracted V1 ECDSA verification key from {signing_key_path.name}")
    else:
        # User-provided verification key -- should already be a raw binary file
        verification_key = sdkconfig.get("CONFIG_SECURE_BOOT_VERIFICATION_KEY")
        if not verification_key:
            return
        verification_key_path = pathlib.Path(verification_key)
        if not verification_key_path.exists():
            print(f"Error: Verification key not found: {verification_key_path}")
            env.Exit(1)
            return
        shutil.copyfile(str(verification_key_path), str(bin_path))

    if not bin_path.exists():
        return

    # Generate the .S assembly file from the binary key data.
    # Replicates ESP-IDF's data_file_embed_asm.cmake with RENAME_TO=signature_verification_key_bin.
    # The file is needed in both the app build dir and the bootloader build dir, since
    # the bootloader also embeds the verification key when CONFIG_SECURE_SIGNED_ON_BOOT_NO_SECURE_BOOT
    # is enabled. PlatformIO's SCons bridge does not execute the CMake custom commands that
    # normally generate these files.
    data = bin_path.read_bytes()
    varname = "signature_verification_key_bin"

    lines = []
    lines.append(f"/* Data converted from {bin_path.name} */")
    lines.append(".data")
    lines.append("#if !defined (__APPLE__) && !defined (__linux__)")
    lines.append(".section .rodata.embedded")
    lines.append("#endif")
    lines.append(f"\n.global {varname}")
    lines.append(f"{varname}:")
    lines.append(f"\n.global _binary_{varname}_start")
    lines.append(f"_binary_{varname}_start: /* for objcopy compatibility */")

    # Format binary data as .byte lines (16 bytes per line)
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hex_bytes = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f".byte {hex_bytes}")

    lines.append(f"\n.global _binary_{varname}_end")
    lines.append(f"_binary_{varname}_end: /* for objcopy compatibility */")
    lines.append(f"\n.global {varname}_length")
    lines.append(f"{varname}_length:")
    lines.append(f".long {len(data)}")
    lines.append("")
    lines.append('#if defined (__linux__)')
    lines.append('.section .note.GNU-stack,"",@progbits')
    lines.append("#endif")

    asm_content = "\n".join(lines) + "\n"

    # Write to app build dir and bootloader build dir
    asm_path.write_text(asm_content)
    bootloader_dir = build_dir / "bootloader"
    if bootloader_dir.is_dir():
        bootloader_bin = bootloader_dir / "signature_verification_key.bin"
        bootloader_asm = bootloader_dir / "signature_verification_key.bin.S"
        shutil.copyfile(str(bin_path), str(bootloader_bin))
        bootloader_asm.write_text(asm_content)


def sign_firmware(source, target, env):
    """
    Sign the firmware binary using espsecure.py if signed OTA verification is enabled.
    Reads signing configuration from sdkconfig.
    """
    build_dir = pathlib.Path(env.subst("$BUILD_DIR"))
    project_dir = pathlib.Path(env.subst("$PROJECT_DIR"))
    pioenv = env.subst("$PIOENV")
    sdkconfig = _parse_sdkconfig(project_dir / f"sdkconfig.{pioenv}")

    if sdkconfig.get("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT") != "y":
        return

    if sdkconfig.get("CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES") != "y":
        print("Signed OTA verification enabled but build-time signing disabled.")
        print("You must sign the firmware externally before flashing.")
        return

    signing_key = sdkconfig.get("CONFIG_SECURE_BOOT_SIGNING_KEY")
    if not signing_key:
        print("Error: CONFIG_SECURE_BOOT_SIGNING_KEY not set in sdkconfig")
        env.Exit(1)
        return

    signing_key_path = pathlib.Path(signing_key)
    if not signing_key_path.exists():
        print(f"Error: Signing key not found: {signing_key_path}")
        env.Exit(1)
        return

    # Determine espsecure signature version from the signing scheme:
    # V1 ECDSA (Secure Boot V1) uses --version 1, V2 RSA/ECDSA use --version 2.
    if sdkconfig.get("CONFIG_SECURE_SIGNED_APPS_ECDSA_SCHEME") == "y":
        sign_version = "1"
    else:
        sign_version = "2"

    firmware_name = os.path.basename(env.subst("$PROGNAME")) + ".bin"
    firmware_path = build_dir / firmware_name

    if not firmware_path.exists():
        print(f"Error: Firmware binary not found: {firmware_path}")
        env.Exit(1)
        return

    python_exe = f'"{env.subst("$PYTHONEXE")}"'
    unsigned_path = firmware_path.with_suffix(".unsigned.bin")

    # Keep a copy of the unsigned binary
    shutil.copyfile(str(firmware_path), str(unsigned_path))

    cmd = [
        python_exe,
        "-m",
        "espsecure",
        "sign-data",
        "--version",
        sign_version,
        "--keyfile",
        str(signing_key_path),
        "--output",
        str(firmware_path),
        str(unsigned_path),
    ]

    print(f"Signing firmware with key: {signing_key_path.name}")
    result = env.Execute(
        env.VerboseAction(" ".join(cmd), "Signing firmware with espsecure")
    )

    if result == 0:
        print("Successfully signed firmware")
    else:
        print(f"Error: espsecure sign_data failed with code {result}")
        # Restore unsigned binary on failure
        shutil.copyfile(str(unsigned_path), str(firmware_path))
        env.Exit(1)


def merge_factory_bin(source, target, env):
    """
    Merges all flash sections into a single .factory.bin using esptool.
    Attempts multiple methods to detect image layout: flasher_args.json, FLASH_EXTRA_IMAGES, fallback guesses.
    """
    firmware_name = os.path.basename(env.subst("$PROGNAME")) + ".bin"
    build_dir = pathlib.Path(env.subst("$BUILD_DIR"))
    firmware_path = build_dir / firmware_name
    flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
    chip = env.BoardConfig().get("build.mcu", "esp32")

    sections = []
    flasher_args_path = build_dir / "flasher_args.json"

    # 1. Try flasher_args.json
    if flasher_args_path.exists():
        try:
            with flasher_args_path.open() as f:
                flash_data = json.load(f)
            for addr, fname in sorted(
                flash_data["flash_files"].items(), key=lambda kv: int(kv[0], 16)
            ):
                file_path = pathlib.Path(fname)
                if file_path.exists():
                    sections.append((addr, str(file_path)))
                else:
                    print(f"Info: {file_path.name} not found - skipping")
        except Exception as e:
            print(f"Warning: Failed to parse flasher_args.json - {e}")

    # 2. Try FLASH_EXTRA_IMAGES if flasher_args.json failed or was empty
    if not sections:
        flash_images = env.get("FLASH_EXTRA_IMAGES")
        if flash_images:
            print("Using FLASH_EXTRA_IMAGES from PlatformIO environment")
            # flatten any nested lists
            flat = list(
                itertools.chain.from_iterable(
                    x if isinstance(x, (list, tuple)) else [x] for x in flash_images
                )
            )
            entries = [env.subst(x) for x in flat]
            for i in range(0, len(entries) - 1, 2):
                addr, fname = entries[i], entries[i + 1]
                if isinstance(fname, (list, tuple)):
                    print(
                        f"Warning: Skipping malformed FLASH_EXTRA_IMAGES entry: {fname}"
                    )
                    continue
                file_path = pathlib.Path(str(fname))
                if file_path.exists():
                    sections.append((addr, file_path))
                else:
                    print(f"Info: {file_path.name} not found — skipping")
        if sections:
            # Append main firmware to sections
            sections.append(("0x10000", firmware_path))

    # 3. Final fallback: guess standard image locations
    if not sections:
        print("Fallback: guessing legacy image paths")
        guesses = [
            ("0x0", build_dir / "bootloader" / "bootloader.bin"),
            ("0x8000", build_dir / "partition_table" / "partition-table.bin"),
            ("0xe000", build_dir / "ota_data_initial.bin"),
            ("0x10000", firmware_path),
        ]
        for addr, file_path in guesses:
            if file_path.exists():
                sections.append((addr, file_path))
            else:
                print(f"Info: {file_path.name} not found — skipping")

    # If no valid sections found, skip merge
    if not sections:
        print("No valid flash sections found — skipping .factory.bin creation.")
        return

    output_path = firmware_path.with_suffix(".factory.bin")
    python_exe = f'"{env.subst("$PYTHONEXE")}"'
    cmd = [
        python_exe,
        "-m",
        "esptool",
        "--chip",
        chip,
        "merge-bin",
        "--flash-size",
        flash_size,
        "--output",
        str(output_path),
    ]
    for addr, file_path in sections:
        cmd += [addr, str(file_path)]

    print(f"Merging binaries into {output_path}")
    result = env.Execute(
        env.VerboseAction(" ".join(cmd), "Merging binaries with esptool")
    )

    if result == 0:
        print(f"Successfully created {output_path}")
    else:
        print(f"Error: esptool merge-bin failed with code {result}")


def esp32_copy_ota_bin(source, target, env):
    """
    Copy the main firmware to a .ota.bin file for compatibility with ESPHome OTA tools.
    """
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.ota.bin")
    shutil.copyfile(firmware_name, new_file_name)
    print(f"Copied firmware to {new_file_name}")


# Generate V1 ECDSA verification key files before build starts.
# Workaround for PlatformIO not executing CMake custom commands that extract
# the public key and generate the .S assembly file for Secure Boot V1.
_generate_v1_verification_key(env)  # noqa: F821

# Run signing first, then merge, then ota copy
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign_firmware)  # noqa: F821
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_factory_bin)  # noqa: F821
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_copy_ota_bin)  # noqa: F821

# Find server certificates in managed components and generate .S files.
# Workaround for PlatformIO not processing target_add_binary_data() from managed component CMakeLists.
project_dir = env.subst("$PROJECT_DIR")
managed_components = os.path.join(project_dir, "managed_components")
if os.path.isdir(managed_components):
    for cert_file in glob(os.path.join(managed_components, "**/server_certs/*.crt"), recursive=True):
        try:
            env.FileToAsm(cert_file, FILE_TYPE="TEXT")
        except Exception as e:
            print(f"Error processing {os.path.basename(cert_file)}: {e}")
