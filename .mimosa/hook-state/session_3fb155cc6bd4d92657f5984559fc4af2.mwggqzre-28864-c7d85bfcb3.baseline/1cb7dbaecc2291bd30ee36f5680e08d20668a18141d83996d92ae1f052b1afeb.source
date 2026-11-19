import esphome.codegen as cg
from esphome.components.light import gamma_table_initializer
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Light benchmarks need USE_LIGHT_GAMMA_LUT defined and a gamma table
    # with external linkage that the benchmark .cpp can reference.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        cg.add_define("USE_LIGHT_GAMMA_LUT")
        # Use the light component's own gamma_table_initializer() so the
        # benchmark stays in sync with any formula changes.
        # Extern-visible (non-static) so the benchmark .cpp can reference it.
        cg.add_global(
            cg.RawStatement(
                "extern const esphome::light::GammaTable bench_gamma_2_8 PROGMEM = "
                f"{gamma_table_initializer(2.8)};"
            )
        )

    to_code.priority = original_to_code.priority
    manifest.to_code = to_code
