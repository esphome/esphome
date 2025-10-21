Import("env")  # noqa: F821

import itertools  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import pathlib  # noqa: E402
import shutil  # noqa: E402


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


# Run merge first, then ota copy second
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_factory_bin)  # noqa: F821
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_copy_ota_bin)  # noqa: F821
