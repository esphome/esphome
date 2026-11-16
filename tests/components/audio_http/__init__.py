"""Exercise the real HTTP source and decoder workers with an injected transport."""

from contextlib import AbstractContextManager, nullcontext
from dataclasses import dataclass
from pathlib import Path

from esphome import git
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.loader import FileResource
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


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
        dependencies = (
            (
                "micro-decoder",
                "4d06df93a1ee69788dfc7d8676cbd66371b3eae9",
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
                "f9c27d4d0166021db977d7fc37d205b21d81e9f2",
                (
                    "crc",
                    "decorrelation",
                    "flac_decoder",
                    "frame_header",
                    "lpc",
                    "lpc_analysis",
                    "pcm_packing",
                    "rice_partitions",
                    "subframe_coding",
                ),
            ),
        )
        for name, revision, sources in dependencies:
            root, _ = git.clone_or_update(
                url=f"https://github.com/esphome-libs/{name}.git",
                ref=revision,
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
