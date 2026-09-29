from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # host must run its to_code during builds because it sets up
    # the host platform target execution environment.
    manifest.enable_codegen()
