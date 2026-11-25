"""Exercise the real HTTP source and decoder workers with an injected transport."""

from contextlib import AbstractContextManager, nullcontext
from dataclasses import dataclass
from pathlib import Path
import sys

from esphome import git
import esphome.codegen as cg
from esphome.components.audio import MICRO_DECODER_VERSION, MICRO_FLAC_VERSION
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.loader import FileResource
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

# Library sources the decoder needs, from the releases the audio component builds with
_DEPENDENCIES = (
    (
        "micro-decoder",
        MICRO_DECODER_VERSION,
        (
            "audio_decoder",
            "audio_reader",
            "decoder_source",
            "md_transfer_buffer",
            "ring_buffer",
            "types",
        ),
    ),
    (
        "micro-flac",
        MICRO_FLAC_VERSION,
        ("crc", "decorrelation", "flac_decoder", "frame_header", "lpc", "pcm_packing"),
    ),
)


@dataclass(frozen=True, order=True)
class DependencySource(FileResource):
    """Compile an unchanged dependency translation unit through the standard writer."""

    source_path: Path

    def path(self) -> AbstractContextManager[Path]:
        return nullcontext(self.source_path)


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["audio", "media_source"]
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        for name, version, sources in _DEPENDENCIES:
            root, _ = git.clone_or_update(
                url=f"https://github.com/esphome-libs/{name}.git",
                ref=f"v{version}",
                refresh=git.NEVER_REFRESH,
                domain="cpp_unit_test",
            )
            for directory in ("include", "src"):
                cg.add_build_flag(f"-I{root / directory}")
            manifest.add_resources(
                *(
                    DependencySource(
                        "esphome.components.audio_http",
                        f"native_{name}_{source}.cpp",
                        root / "src" / f"{source}.cpp",
                    )
                    for source in sources
                )
            )
        cg.add_build_flag("-DMICRO_DECODER_CODEC_FLAC=1")
        cg.add_build_flag("-DMICRO_FLAC_DISABLE_OGG")
        # Apple's linker has no --wrap, so the worker thread counts are only checked elsewhere
        if sys.platform != "darwin":
            cg.add_define("AUDIO_HTTP_TEST_COUNT_THREADS")
            cg.add_build_flag("-Wl,--wrap=pthread_create")
            cg.add_build_flag("-Wl,--wrap=pthread_join")
        sdk = Path(__file__).parent.parent / "speaker_source" / "__sdk"
        manifest.add_resources(
            *(
                DependencySource(
                    "speaker_source", f"__sdk/freertos/{name}", sdk / "freertos" / name
                )
                for name in ("FreeRTOS.h", "queue.h")
            )
        )
        cg.add_build_flag(f"-I{CORE.relative_src_path('speaker_source', '__sdk')}")
        cg.add_build_flag(f"-I{CORE.relative_src_path('audio_http', '__sdk')}")
        cg.add_define(
            "AUDIO_HTTP_TEST_FIXTURES",
            str(CORE.relative_src_path("audio_http", "__data")),
        )

    manifest.to_code = to_code_testing
