import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride

ESP_AUDIO_LIBS_COMMIT = "3079cd0576d563c955e412937a91782ab4eaf516"


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        cg.add_platformio_option("lib_compat_mode", "off")
        cg.add_library(
            "esp-audio-libs",
            None,
            f"https://github.com/esphome-libs/esp-audio-libs.git#{ESP_AUDIO_LIBS_COMMIT}",
        )

    manifest.to_code = to_code_testing
