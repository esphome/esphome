import esphome.codegen as cg
from esphome.components.runtime_image import enable_format
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code is suppressed in cpptest builds; formats are normally enabled by
    # process_runtime_image_config(). Enable all formats so the format-switch
    # tests have two decoder types and every retained decoder is under test.
    async def to_code_testing(config: ConfigType) -> None:
        enable_format("BMP")
        enable_format("PNG")
        enable_format("JPEG")
        # JPEGDEC's host detection checks __MACH__/__LINUX__, but gcc only
        # predefines the lowercase __linux__, so without this it tries to
        # include Arduino.h on the Linux CI runners.
        cg.add_build_flag("-D__LINUX__")

    manifest.to_code = to_code_testing
