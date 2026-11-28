from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The data code is compiled in only when an event declares variables; the unit tests have none.
    add_defines_to_code(manifest, "USE_EVENT_DATA")
