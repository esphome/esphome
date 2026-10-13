from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Loops that only exist on boards with PSRAM are gated on this define;
    # set it so the host benchmark measures the same code those boards run.
    add_defines_to_code(manifest, "USE_PSRAM")
