"""Apply the esp-matter source patches this component depends on.

Invoked from ``_patch_hook.cmake`` via ``-DCMAKE_PROJECT_INCLUDE``, which
runs AFTER idf-component-manager's integrity-restore pass but BEFORE ninja
starts compiling — the only configure-time hook that lands there. CMake
re-runs whenever inputs change; component-manager may revert our files on
those re-runs and this script re-patches in the same configure pass, so
ninja never sees an unpatched source. On skip-configure builds (day-to-day
rebuilds) the previous configure's patches remain in place.

All patches are idempotent — each writes a marker string on first
application and short-circuits on subsequent runs.

Patches:
  1. NetworkCommissioningDriver_Ethernet.cpp — replace body with empty stub;
     the internal ESP32 EMAC APIs it hardcodes don't exist on S3/S2/C3/C6/H2
     with external SPI PHYs, and even on classic ESP32 we don't want it
     driving the PHY because ESPHome's ``ethernet:`` already installed the
     driver. ``ESPEthernetDriver::Init`` is provided by
     ``matter_ethernet_stub.cpp``.
  2. esp_matter_core.cpp — comment out the ``InitWiFiStack()`` call inside
     ``esp_matter::start()``. On Wi-Fi builds this helper calls
     ``esp_wifi_init()``, which fails with ``ESP_ERR_INVALID_STATE`` because
     ESPHome's ``wifi:`` component already inited the driver — the failure
     aborts ``esp_matter::start()``. On Ethernet-only builds the surrounding
     ``#if CHIP_DEVICE_CONFIG_ENABLE_WIFI`` is off and the call was already
     going to be compiled out; the textual patch is still applied so the
     source is uniform across variants. The one thing ``InitWiFiStack``
     also did — ``esp_event_handler_register(WIFI_EVENT, …,
     PlatformManagerImpl::HandleESPSystemEvent, …)`` — is reproduced from
     ``matter_component.cpp::setup()`` when ``USE_WIFI`` is defined, so
     CHIP still sees ``WIFI_EVENT_STA_DISCONNECTED`` / ``WIFI_EVENT_SCAN_DONE``.
  3. Every ``*_integration.cpp`` (or ``clusters/*/integration.cpp``) that
     declares
     ``std::unordered_map<EndpointId, LazyRegisteredServerCluster<...>> gServers``
     is rewritten to ``std::map``. std::map is a tree, so insertion NEVER
     invalidates pointers to existing elements (C++ spec guarantee). Fixes
     a rehash bug where CHIP's cluster registry holds pointers into the
     map that go dangling when ``unordered_map`` grows past its load-factor
     threshold. On a bridge with 76 endpoints (root + aggregator + 74
     bridged entities) this reproducibly corrupted
     ``DefaultServerCluster::Startup`` mid-boot.
  4. esp_matter's top-level CMakeLists.txt — drop the arm that excludes
     ``ESP32DnssdImpl.cpp`` when both ``ENABLE_WIFI_STATION`` and
     ``ENABLE_WIFI_AP`` are off. ``DnssdImpl.cpp`` still calls the
     ``EspDnssd*`` symbols defined in that file, so the exclusion breaks
     linking on Ethernet-only builds. Those symbols only touch
     ``esp_netif`` and ``mdns`` — safe to link without Wi-Fi.
  5. connectedhomeip's chip/Kconfig — the ``SEC_CERT_DAC_PROVIDER`` symbol
     is declared standalone here with a ``bool "..."`` prompt and a
     ``default n`` while ALSO being a member of the
     ``ESP_MATTER_DAC_PROVIDER`` choice declared in esp_matter/Kconfig.
     Kconfig warns because (a) defaults on choice symbols have no effect
     and (b) prompts on choice symbols must live inside the choice. Both
     warnings are cosmetic but noisy — remove the two offending property
     lines and keep the accumulated help text.

Errors: PATCH1/2/4 are file-specific and load-bearing. If their target file
is missing, or the marker string the patch anchors on is not present in the
current esp-matter release, the script exits non-zero — the CMake hook
surfaces that as a warning and downstream compilation will still fail hard.
Silent-skip on those was masking upstream drift where our patch was quietly
no-op'd but the code was actually broken. PATCH3 iterates across many
integration files; each file may or may not carry the buggy pattern
(``LazyRegisteredServerCluster<…> gServers``), so per-file skips are
legitimate — but if the sweep patches ZERO files, that's fatal too
(esp-matter 1.5+ always has some).
"""

from pathlib import Path
import sys


class PatchError(Exception):
    """Raised when a required patch cannot be applied.

    Marker string not found in the target file, target file missing, or
    the PATCH3 sweep found no matching files — all mean the upstream we
    thought we were patching has drifted, and continuing would produce a
    silently-broken binary.
    """


PATCH1_STUB = """// ESPHOME-MATTER-PATCH
//
// Original file hardcoded internal ESP32 EMAC APIs (esp_eth_mac_new_esp32,
// eth_esp32_emac_config_t) that do not exist on ESP32-S3/S2/C3/C6/H2 with
// external SPI PHYs (W5500, DM9051). Even on the classic ESP32 where those
// APIs do exist, ESPHome's ethernet: component already installed the PHY
// driver, so letting this TU run would fight it. Neutralized at build time
// by components/matter/_apply_patches.py invoked from CMakeLists.txt.
// ESPEthernetDriver::Init is provided by components/matter/matter_ethernet_stub.cpp.
"""

PATCH2_OLD = (
    "VerifyOrReturnError(chip::DeviceLayer::Internal::ESP32Utils::InitWiFiStack() "
    '== CHIP_NO_ERROR, ESP_FAIL, ESP_LOGE(TAG, "Error initializing Wi-Fi stack"));'
)
PATCH2_NEW = (
    "// ESPHOME-MATTER-PATCH-WIFI: InitWiFiStack skipped — ESPHome owns "
    "esp_wifi_init; WIFI_EVENT handler re-registered from matter_component.cpp"
)

# PATCH 4: keep ESP32DnssdImpl.cpp in the esp-matter component build even
# when WIFI_STATION and WIFI_AP are both off. The upstream CMakeLists.txt
# excludes that .cpp on !WIFI_STATION && !WIFI_AP (line 441 in 1.6.0), but
# DnssdImpl.cpp keeps compiling and calls into `chip::Dnssd::Esp*` symbols
# defined only inside ESP32DnssdImpl.cpp — the exclusion breaks linking on
# our Ethernet-only builds. Those Esp* functions only touch esp_netif +
# mdns (no direct esp_wifi calls in the exported symbols), so keeping the
# file compiled is safe under Ethernet. The exclusion still applies when
# USE_MINIMAL_MDNS is on — that path swaps the whole mDNS backend and
# doesn't need ESP32DnssdImpl at all.
PATCH4_OLD = (
    "    if((CONFIG_USE_MINIMAL_MDNS) OR "
    "((NOT CONFIG_ENABLE_WIFI_STATION) AND (NOT CONFIG_ENABLE_WIFI_AP)))\n"
    "        list(APPEND EXCLUDE_SRCS_LIST "
    '"${MATTER_SDK_PATH}/src/platform/ESP32/ESP32DnssdImpl.cpp")\n'
    "    endif()"
)
PATCH4_NEW = (
    "    # ESPHOME-MATTER-PATCH-DNSSD: dropped the "
    "!WIFI_STATION && !WIFI_AP arm.\n"
    "    # DnssdImpl.cpp references EspDnssd* symbols defined in "
    "ESP32DnssdImpl.cpp;\n"
    "    # excluding that .cpp on Ethernet-only builds breaks linking. The\n"
    "    # exported EspDnssd* functions only use esp_netif + mdns and are\n"
    "    # safe to link without Wi-Fi.\n"
    "    if(CONFIG_USE_MINIMAL_MDNS)\n"
    "        list(APPEND EXCLUDE_SRCS_LIST "
    '"${MATTER_SDK_PATH}/src/platform/ESP32/ESP32DnssdImpl.cpp")\n'
    "    endif()"
)

PATCH5_MARKER_V2 = "# ESPHOME-MATTER-PATCH-KCONFIG-V2"
PATCH5_MARKER_V1 = "ESPHOME-MATTER-PATCH-KCONFIG"
PATCH5_REPLACEMENT = (
    "        # ESPHOME-MATTER-PATCH-KCONFIG-V2: standalone declaration of\n"
    "        # SEC_CERT_DAC_PROVIDER removed. The symbol is a member of the\n"
    "        # ESP_MATTER_DAC_PROVIDER choice defined at esp_matter/Kconfig:48;\n"
    "        # every reference below (`depends on SEC_CERT_DAC_PROVIDER`) and\n"
    "        # every `#ifdef CONFIG_SEC_CERT_DAC_PROVIDER` guard in the .cpp\n"
    "        # sources binds to that choice definition just fine. Keeping a\n"
    "        # second standalone `config` here tripped three separate lints:\n"
    "        # prompt-outside-choice, default-on-choice-symbol, and (with\n"
    '        # esp-idf-kconfig 1.x) "Multiple Symbol/Choice Definitions".'
)


PATCH5_ORIGINAL_BLOCK = (
    "        config SEC_CERT_DAC_PROVIDER\n"
    '            bool "Use Secure Cert DAC Provider"\n'
    "            default n\n"
    "            help\n"
    "                Use ESP32 Secure Cert DAC Provider which is  "
    "ESP32 DeviceAttestationCredentialsProvider implementation which "
    "reads attestation\n"
    "                information from the esp_secure_cert partition"
)


PATCH6_OLD = (
    "if(CONFIG_DISABLE_IPV4)\n"
    "    if(CONFIG_LWIP_IPV4)\n"
    '        message(FATAL_ERROR "Please also disable config option CONFIG_LWIP_IPV4")\n'
    "    else()\n"
    '        target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_DEVICE_CONFIG_ENABLE_IPV4=false")\n'
    "    endif()"
)
PATCH6_NEW = (
    "# ESPHOME-MATTER-PATCH-IPV4-GUARD: keep LwIP IPv4 available for the\n"
    "# outer ESPHome application (api / ota / captive_portal / non-Matter mdns)\n"
    "# while still forcing CHIP itself onto IPv6 only. INET_CONFIG_ENABLE_IPV4=0\n"
    "# (set by CONFIG_DISABLE_IPV4=y via Kconfig) strips every IPv4 code path\n"
    "# inside CHIP at compile time; the FATAL_ERROR here was defensive against\n"
    "# a nonexistent runtime interaction.\n"
    "if(CONFIG_DISABLE_IPV4)\n"
    "    if(CONFIG_LWIP_IPV4)\n"
    '        message(STATUS "esp_matter: CHIP IPv4 disabled; LwIP IPv4 kept enabled for the outer application")\n'
    "    endif()\n"
    '    target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_DEVICE_CONFIG_ENABLE_IPV4=false")'
)


PATCH3_MARKER = "// ESPHOME-MATTER-PATCH-MAP"

# Two substitutions per integration file: swap the include and the type
# alias. std::unordered_map guarantees O(1) average lookup at the price of
# rehashing on insert — which invalidates pointers CHIP holds into the map.
# std::map is a tree (O(log N) lookup) whose iterators/references stay valid
# through all inserts of distinct keys (§26.6.4.4 [container.reqmts]).
PATCH3_INCLUDE_OLD = "#include <unordered_map>"
PATCH3_INCLUDE_NEW = f"#include <map>  {PATCH3_MARKER}"
PATCH3_TYPE_OLD = "std::unordered_map"
PATCH3_TYPE_NEW = "std::map"


def _apply_stub_overwrite(path: Path, marker: str, stub: str) -> str:
    if not path.exists():
        raise PatchError(
            f"required patch target missing: {path} — esp-matter version "
            f"drift? update _apply_patches.py"
        )
    head = path.read_bytes()[:128].decode(errors="replace")
    # Accept the pre-rename marker (PATCHED-BY-UNISEC-MATTER) as "already
    # patched" so a file stubbed by an older version of this script isn't
    # rewritten every configure. The stub content itself may differ slightly
    # (comment wording) — that difference alone is not worth re-writing.
    if marker in head or "PATCHED-BY-UNISEC-MATTER" in head:
        return f"noop (already patched): {path.name}"
    path.write_text(stub, encoding="utf-8")
    return f"stub: {path.name}"


def _apply_std_map_swap(path: Path) -> str:
    """Swap `std::unordered_map` → `std::map` in a cluster integration file.

    Only touches files that actually declare a
      std::unordered_map<EndpointId, LazyRegisteredServerCluster<...>> gServers;
    global — the exact pattern that hits the rehash-invalidates-pointers bug.
    Files with unordered_map used for unrelated things (or none at all) are
    skipped. Idempotent via the PATCH3_MARKER comment inserted at the top.

    Some integration files (descriptor_integration.cpp, etc.) get
    `std::unordered_map` from a transitive header include chain rather than
    a direct `#include <unordered_map>`. Handle both cases: if the direct
    include is present, swap it; otherwise insert a fresh `#include <map>`
    below the file's last existing include.

    Per-file misses ARE legitimate here (many integration files don't carry
    the buggy pattern), so this stays tolerant. The aggregate check "at
    least one file was patched" runs in ``apply_patches`` below.
    """
    if not path.exists():
        return f"skip (missing): {path.name}"
    content = path.read_text(encoding="utf-8")
    # Accept the pre-rename marker so a file patched by an older version of
    # this script (before PATCHED-BY-UNISEC-MATTER → ESPHOME-MATTER-PATCH)
    # is not re-processed and left with a duplicate `#include <map>`.
    _LEGACY_PATCH3_MARKER = "// PATCHED-BY-UNISEC-MATTER-MAP"
    if PATCH3_MARKER in content or _LEGACY_PATCH3_MARKER in content:
        return f"noop (already patched): {path.name}"
    if "LazyRegisteredServerCluster" not in content or "gServers" not in content:
        return f"skip (no buggy pattern): {path.name}"

    # Remove the earlier UserLabel-specific reserve() patch if it lingers from
    # a prior build. std::map has no bucket_count/reserve — leaving those in
    # yields a compile error.
    old_marker = "// ESPHOME-MATTER-PATCH-USERLABEL"
    if old_marker in content:
        cleaned_lines = []
        skip_lines = 0
        for line in content.splitlines(keepends=True):
            if skip_lines > 0:
                skip_lines -= 1
                continue
            if old_marker in line:
                # Drop this comment line and the two follow-up lines that
                # make up the original 4-line block (2 comment lines + 1 if).
                skip_lines = 3
                continue
            cleaned_lines.append(line)
        content = "".join(cleaned_lines)

    # Prefer swapping the direct include if it's there; otherwise inject a
    # fresh `#include <map>` right after the last existing include line.
    if PATCH3_INCLUDE_OLD in content:
        patched = content.replace(PATCH3_INCLUDE_OLD, PATCH3_INCLUDE_NEW, 1)
    else:
        lines = content.splitlines(keepends=True)
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.startswith("#include"):
                last_include_idx = i
        if last_include_idx < 0:
            return f"skip (no include lines): {path.name}"
        lines.insert(last_include_idx + 1, f"{PATCH3_INCLUDE_NEW}\n")
        patched = "".join(lines)

    patched = patched.replace(PATCH3_TYPE_OLD, PATCH3_TYPE_NEW)
    path.write_text(patched, encoding="utf-8")
    return f"map-swap: {path.name}"


def _apply_patch5_kconfig(path: Path) -> str:
    """Delete the standalone SEC_CERT_DAC_PROVIDER declaration in chip/Kconfig.

    Handles three input states:

    1. Fresh upstream: exact ORIGINAL block present → replace with V2 marker.
    2. V1 leftover: a prior version of this patch removed the ``bool`` and
       ``default`` but left ``config SEC_CERT_DAC_PROVIDER + help`` (and a
       V1 marker comment). That leftover still trips the
       "Multiple Symbol/Choice Definitions" lint from esp-idf-kconfig 1.x.
       Detect via the V1 marker + the vestigial ``config`` block, strip the
       whole thing (V1 marker comment block + config + help), leave only
       the V2 marker.
    3. Already V2: noop.

    Any other state (someone hand-edited, or upstream drifted mid-migration)
    is fatal — we don't guess.
    """
    if not path.exists():
        raise PatchError(
            f"required patch target missing: {path} — esp-matter version "
            f"drift? update _apply_patches.py"
        )
    content = path.read_text(encoding="utf-8")

    # Recognise the pre-rename marker names too: a build that ran an earlier
    # version of this script (before the PATCHED-BY-UNISEC-MATTER → ESPHOME-
    # MATTER-PATCH sweep) will have stamped `PATCHED-BY-UNISEC-MATTER-KCONFIG*`
    # into the file. Treat legacy V2 as already patched; legacy V1 flows into
    # the V1→V2 migration branch below.
    _LEGACY_PATCH5_MARKER_V2 = "PATCHED-BY-UNISEC-MATTER-KCONFIG-V2"
    _LEGACY_PATCH5_MARKER_V1 = "PATCHED-BY-UNISEC-MATTER-KCONFIG"

    if PATCH5_MARKER_V2 in content or _LEGACY_PATCH5_MARKER_V2 in content:
        return f"noop (already patched V2): {path.name}"

    if PATCH5_MARKER_V1 in content or _LEGACY_PATCH5_MARKER_V1 in content:
        # V1-patched shape: the V1 marker sits as a comment block, followed
        # by `config SEC_CERT_DAC_PROVIDER` with only `help` (no bool/default).
        # Find the V1 marker comment line and delete everything from there
        # through the closing help paragraph (blank line terminator).
        lines = content.splitlines(keepends=True)
        v1_start = None
        for i, line in enumerate(lines):
            if PATCH5_MARKER_V1 in line or _LEGACY_PATCH5_MARKER_V1 in line:
                v1_start = i
                break
        if v1_start is None:
            raise PatchError(f"PATCH5: V1 marker in {path.name} but line lookup failed")
        # Walk back to the leading whitespace / previous blank so we drop the
        # comment block cleanly; walk forward to the blank line that ends
        # the `help` paragraph.
        v1_end = v1_start
        while v1_end < len(lines) and lines[v1_end].strip() != "":
            v1_end += 1
        # v1_end now points at the blank line after `help`; keep that blank.
        del lines[v1_start:v1_end]
        # Insert V2 replacement + a blank line to match surrounding style.
        lines.insert(v1_start, PATCH5_REPLACEMENT + "\n")
        path.write_text("".join(lines), encoding="utf-8")
        return f"migrate V1→V2: {path.name}"

    if PATCH5_ORIGINAL_BLOCK in content:
        path.write_text(
            content.replace(PATCH5_ORIGINAL_BLOCK, PATCH5_REPLACEMENT),
            encoding="utf-8",
        )
        return f"replace: {path.name}"

    raise PatchError(
        f"PATCH5: neither V2 marker nor V1 marker nor original block found "
        f"in {path.name} — esp-matter release changed. Update PATCH5 in "
        f"_apply_patches.py."
    )


def _apply_string_replace(
    path: Path, marker: str, old: str, new: str, legacy_markers: tuple = ()
) -> str:
    if not path.exists():
        raise PatchError(
            f"required patch target missing: {path} — esp-matter version "
            f"drift? update _apply_patches.py"
        )
    content = path.read_text(encoding="utf-8")
    if marker in content:
        return f"noop (already patched): {path.name}"
    # Recognise markers from an earlier version of this script that patched
    # the same anchor. The anchor string was already replaced by that build,
    # so a naive re-run would find neither the new marker nor the anchor and
    # fail with "esp-matter release changed" even though the file is fine.
    # We treat legacy markers as "already patched" and move on.
    for legacy in legacy_markers:
        if legacy in content:
            return f"noop (legacy patch marker present): {path.name}"
    if old not in content:
        # Truncate the expected substring for logging — the full text can be
        # multi-line and swamp CMake's output.
        preview = " ".join(old.split())[:160]
        raise PatchError(
            f"expected upstream marker string not found in {path.name} — "
            f"esp-matter release changed. Update the patch definition in "
            f"_apply_patches.py.\n  expected (first 160 chars): {preview}"
        )
    path.write_text(content.replace(old, new), encoding="utf-8")
    return f"replace: {path.name}"


def apply_patches(matter_dir: Path) -> list:
    """Apply all esp-matter source patches under ``matter_dir``.

    Returns a list of one-line result strings suitable for logging. Raises
    :class:`PatchError` if a required patch cannot be applied — the CLI
    entrypoint (``main``) turns that into a non-zero exit so the CMake hook
    surfaces the failure. Callers that want to keep going on failure must
    catch it themselves.
    """
    results = []

    results.append(
        _apply_stub_overwrite(
            matter_dir
            / "connectedhomeip"
            / "connectedhomeip"
            / "src"
            / "platform"
            / "ESP32"
            / "NetworkCommissioningDriver_Ethernet.cpp",
            "ESPHOME-MATTER-PATCH",
            PATCH1_STUB,
        )
    )

    results.append(
        _apply_string_replace(
            matter_dir / "components" / "esp_matter" / "esp_matter_core.cpp",
            "ESPHOME-MATTER-PATCH-WIFI",
            PATCH2_OLD,
            PATCH2_NEW,
            legacy_markers=("PATCHED-BY-UNISEC-MATTER-WIFI",),
        )
    )

    results.append(
        _apply_string_replace(
            matter_dir / "CMakeLists.txt",
            "ESPHOME-MATTER-PATCH-DNSSD",
            PATCH4_OLD,
            PATCH4_NEW,
            legacy_markers=("PATCHED-BY-UNISEC-MATTER-DNSSD",),
        )
    )

    results.append(
        _apply_patch5_kconfig(
            matter_dir
            / "connectedhomeip"
            / "connectedhomeip"
            / "config"
            / "esp32"
            / "components"
            / "chip"
            / "Kconfig"
        )
    )

    # PATCH 6: esp-matter's own CMakeLists guards CONFIG_DISABLE_IPV4 with a
    # FATAL_ERROR unless CONFIG_LWIP_IPV4=n. That is defensive over-reach —
    # CHIP with INET_CONFIG_ENABLE_IPV4=0 does not construct IPv4
    # PeerAddresses, does not bind IPv4 sockets, and does not advertise A
    # records; incoming IPv4 traffic simply has no listener. LwIP's IPv4
    # stack stays live for ESPHome's own components (api / ota /
    # captive_portal / native mdns), which is exactly what we want. Downgrade
    # the guard to a status message so both stacks coexist.
    results.append(
        _apply_string_replace(
            matter_dir / "CMakeLists.txt",
            "ESPHOME-MATTER-PATCH-IPV4-GUARD",
            PATCH6_OLD,
            PATCH6_NEW,
            legacy_markers=("PATCHED-BY-UNISEC-MATTER-IPV4-GUARD",),
        )
    )

    # Patch 3: every integration.cpp under data_model_provider/clusters/
    # that has the unordered_map<EndpointId, LazyRegisteredServerCluster<>> gServers
    # pattern gets its map type swapped to std::map.
    #
    # esp-matter 1.6.0 reorganised the cluster integrations from
    # `clusters/<name>_integration.cpp` into `clusters/<name>/integration.cpp`.
    # Use a recursive glob so we hit both layouts — _apply_std_map_swap is a
    # no-op on files that don't match the pattern, so the older sibling files
    # (if any survive in some future intermediate layout) stay safe.
    clusters_dir = (
        matter_dir / "components" / "esp_matter" / "data_model_provider" / "clusters"
    )
    if not clusters_dir.is_dir():
        raise PatchError(
            f"PATCH3: cluster integrations dir missing: {clusters_dir} — "
            f"esp-matter reorganised its data_model_provider layout, "
            f"update the glob in _apply_patches.py"
        )
    candidates = set(clusters_dir.glob("*_integration.cpp"))
    candidates.update(clusters_dir.glob("**/integration.cpp"))
    if not candidates:
        raise PatchError(f"PATCH3: no integration.cpp candidates under {clusters_dir}")
    swap_count = 0
    noop_count = 0
    for path in sorted(candidates):
        result = _apply_std_map_swap(path)
        results.append(result)
        if result.startswith("map-swap"):
            swap_count += 1
        elif result.startswith("noop"):
            noop_count += 1
    # After a full sweep at least one file should have carried the buggy
    # pattern — every esp-matter 1.5+ release has ~40 of them. If we
    # patched zero AND noop'd zero, we're patching thin air.
    if swap_count == 0 and noop_count == 0:
        raise PatchError(
            f"PATCH3: swept {len(candidates)} candidate files but found "
            f"no LazyRegisteredServerCluster<…> gServers pattern to swap. "
            f"esp-matter refactored the cluster integration layout — "
            f"update the pattern check in _apply_std_map_swap."
        )
    results.append(
        f"map-swap summary: patched={swap_count} already-patched={noop_count} "
        f"scanned={len(candidates)}"
    )

    return results


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: _apply_patches.py <esp_matter_dir>", file=sys.stderr)
        return 2
    raw = sys.argv[1].strip()
    if not raw:
        print("_apply_patches.py: empty esp_matter_dir arg", file=sys.stderr)
        return 3
    matter_dir = Path(raw)
    if not matter_dir.is_dir():
        # esp-matter not yet downloaded — component-manager normally populates
        # it before this hook runs, but on a cold configure the two can race.
        # Return a distinct code so the CMake hook treats it as tolerable
        # (skip with STATUS) rather than confusing it with a real
        # patch-application failure. Any other non-zero code is fatal.
        print(
            f"_apply_patches.py: esp_matter dir not present yet: "
            f"{matter_dir} — skipping (will retry next configure)",
            file=sys.stderr,
        )
        return 20

    try:
        results = apply_patches(matter_dir)
    except PatchError as e:
        print(f"matter-patches: FATAL: {e}", file=sys.stderr)
        return 4
    for r in results:
        print(f"matter-patches: {r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
