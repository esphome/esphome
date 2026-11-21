from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # ac_dimmer.h includes the output component's FloatOutput
    manifest.dependencies = manifest.dependencies + ["output"]
