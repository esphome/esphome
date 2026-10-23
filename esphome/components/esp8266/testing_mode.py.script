import os
import re

# pylint: disable=E0602
Import("env")  # noqa


# Memory sizes for testing mode (allow larger builds for CI component grouping)
TESTING_IRAM_SIZE = "0x200000"  # 2MB
TESTING_DRAM_SIZE = "0x200000"  # 2MB
TESTING_FLASH_SIZE = "0x2000000"  # 32MB


def patch_segment_size(content, segment_name, new_size, label):
    """Patch a memory segment's length in linker script.

    Args:
        content: Linker script content
        segment_name: Name of the segment (e.g., 'iram1_0_seg')
        new_size: New size as hex string (e.g., '0x200000')
        label: Human-readable label for logging (e.g., 'IRAM')

    Returns:
        Tuple of (patched_content, was_patched)
    """
    # Match: segment_name : org = 0x..., len = 0x...
    pattern = rf"({segment_name}\s*:\s*org\s*=\s*0x[0-9a-fA-F]+\s*,\s*len\s*=\s*)0x[0-9a-fA-F]+"
    new_content = re.sub(pattern, rf"\g<1>{new_size}", content)
    return new_content, new_content != content


def apply_memory_patches(content):
    """Apply IRAM, DRAM, and Flash patches to linker script content.

    Args:
        content: Linker script content as string

    Returns:
        Patched content as string
    """
    patches_applied = []

    # Patch IRAM (for larger code in IRAM)
    content, patched = patch_segment_size(content, "iram1_0_seg", TESTING_IRAM_SIZE, "IRAM")
    if patched:
        patches_applied.append("IRAM")

    # Patch DRAM (for larger BSS/data sections)
    content, patched = patch_segment_size(content, "dram0_0_seg", TESTING_DRAM_SIZE, "DRAM")
    if patched:
        patches_applied.append("DRAM")

    # Patch Flash (for larger code sections)
    content, patched = patch_segment_size(content, "irom0_0_seg", TESTING_FLASH_SIZE, "Flash")
    if patched:
        patches_applied.append("Flash")

    if patches_applied:
        iram_mb = int(TESTING_IRAM_SIZE, 16) // (1024 * 1024)
        dram_mb = int(TESTING_DRAM_SIZE, 16) // (1024 * 1024)
        flash_mb = int(TESTING_FLASH_SIZE, 16) // (1024 * 1024)
        print(f"  Patched memory segments: {', '.join(patches_applied)} (IRAM/DRAM: {iram_mb}MB, Flash: {flash_mb}MB)")

    return content


def patch_linker_script_file(filepath, description):
    """Patch a linker script file in the build directory with enlarged memory segments.

    This function modifies linker scripts in the build directory only (never SDK files).
    It patches IRAM, DRAM, and Flash segments to allow larger builds in testing mode.

    Args:
        filepath: Path to the linker script file in the build directory
        description: Human-readable description for logging

    Returns:
        True if the file was patched, False if already patched or not found
    """
    if not os.path.exists(filepath):
        print(f"ESPHome: {description} not found at {filepath}")
        return False

    print(f"ESPHome: Patching {description}...")
    with open(filepath, "r") as f:
        content = f.read()

    patched_content = apply_memory_patches(content)

    if patched_content != content:
        with open(filepath, "w") as f:
            f.write(patched_content)
        print(f"ESPHome: Successfully patched {description}")
        return True
    else:
        print(f"ESPHome: {description} already patched or no changes needed")
        return False


def patch_local_linker_script(source, target, env):
    """Patch the local.eagle.app.v6.common.ld in build directory.

    This patches the preprocessed linker script that PlatformIO creates in the build
    directory, enlarging IRAM, DRAM, and Flash segments for testing mode.

    Args:
        source: SCons source nodes
        target: SCons target nodes
        env: SCons environment
    """
    # Check if we're in testing mode
    build_flags = env.get("BUILD_FLAGS", [])
    testing_mode = any("-DESPHOME_TESTING_MODE" in flag for flag in build_flags)

    if not testing_mode:
        return

    # Patch the local linker script if it exists
    build_dir = env.subst("$BUILD_DIR")
    ld_dir = os.path.join(build_dir, "ld")
    if os.path.exists(ld_dir):
        local_ld = os.path.join(ld_dir, "local.eagle.app.v6.common.ld")
        if os.path.exists(local_ld):
            patch_linker_script_file(local_ld, "local.eagle.app.v6.common.ld")


# Check if we're in testing mode
build_flags = env.get("BUILD_FLAGS", [])
testing_mode = any("-DESPHOME_TESTING_MODE" in flag for flag in build_flags)

if testing_mode:
    # Create a custom linker script in the build directory with patched memory limits
    # This allows larger IRAM/DRAM/Flash for CI component grouping tests
    build_dir = env.subst("$BUILD_DIR")
    ldscript = env.GetProjectOption("board_build.ldscript", "")
    assert ldscript, "No linker script configured in board_build.ldscript"

    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif8266")
    assert framework_dir is not None, "Could not find framework-arduinoespressif8266 package"

    # Read the original SDK linker script (read-only, SDK is never modified)
    sdk_ld = os.path.join(framework_dir, "tools", "sdk", "ld", ldscript)
    # Create a custom version in the build directory (isolated, temporary)
    custom_ld = os.path.join(build_dir, f"testing_{ldscript}")

    if os.path.exists(sdk_ld) and not os.path.exists(custom_ld):
        # Read the SDK linker script
        with open(sdk_ld, "r") as f:
            content = f.read()

        # Apply memory patches (IRAM: 2MB, DRAM: 2MB, Flash: 32MB)
        patched_content = apply_memory_patches(content)

        # Write the patched linker script to the build directory
        with open(custom_ld, "w") as f:
            f.write(patched_content)

        print(f"ESPHome: Created custom linker script: {custom_ld}")

    # Tell the linker to use our custom script from the build directory
    assert os.path.exists(custom_ld), f"Custom linker script not found: {custom_ld}"
    env.Replace(LDSCRIPT_PATH=custom_ld)
    print(f"ESPHome: Using custom linker script with patched memory limits")

    # Also patch local.eagle.app.v6.common.ld after PlatformIO creates it
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_local_linker_script)
