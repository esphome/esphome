# Optional nRF52 Zigbee endpoints and generic attributes

Design checkpoint, 2026-11-15. No implementation in this checkpoint.
Base: `904f0df92034ee70233f6f225c65a5784fb2a6a8`.

## Acceptance and corrected API assumption

Existing simple sensors, binary sensors, switches, numbers and Zigbee time keep their
automatic endpoint assignments and generated descriptors exactly unchanged by default.
They never need explicit clusters, attributes, or user-written C++ classes. Optional
endpoint numbering is useful on its own and is the first implementation slice.

At this base, `zigbee/__init__.py`, `core/entity_helpers.py`, `zigbee_esp32.py`
and `zigbee_ep_esp32.py` expose ESP32 entity options `endpoint`, `cluster`, `report`
and `use_device_type`. The nested `clusters` and `attributes` dictionaries are
internal Python data, not an existing public YAML endpoint/attribute schema.
There is no full public schema here to port verbatim. Reuse the existing options,
validators, type names, defaults and normalized contracts where applicable; do not
claim the advanced example below already exists. Its surface is a proposed extension
requiring review before implementation. No third configuration language or YAML tags.

The broad goal is standard clusters and generic custom attributes on nRF52840 with
Zephyr/ZBOSS. It excludes embedded thermostat control, universal cluster-command
support and automatic Zigbee2MQTT converter/exposes generation.

## Two neutral user examples

### Simple: unchanged automatic endpoints

This illustrates existing syntax. The commented option is the proposed nRF port of
the existing ESP32 option; it needs no advanced configuration.

```yaml
esphome:
  name: sample-sensors
nrf52:
  board: xiao_ble
zigbee:
  id: radio
sensor:
  - platform: template
    id: sample_a
    name: Sample A
    lambda: return 12.0f;
    # endpoint: 17  # Optional after the numbering slice; otherwise automatic.
  - platform: template
    id: sample_b
    name: Sample B
    lambda: return 34.0f;
```

The default remains the legacy Analog Input representation. A device class does
not silently switch legacy nRF sensors to measurement clusters or merge endpoints.
Changing an explicit endpoint is an intentional device-description change; default
configurations require no migration introduced by this port.

### Advanced: proposed extension, not currently valid YAML

This review sketch shows the full generic tracer bullet. Field spelling and action
contracts beyond the existing entity options are proposals, not implemented API.
Presence of `endpoints` opts into advanced mode; there is no extra required mode flag.
These values are protocol values, not household settings.

```yaml
esphome:
  name: sample-attributes
nrf52:
  board: xiao_ble
zigbee:
  id: radio
  endpoints:
    - endpoint: 17
      device_type: CUSTOM_ATTR
      clusters:
        - id: 0xFC00
          role: SERVER
          attributes:
            - attribute_id: 0x0001
              type: INT16
              value: -125
              access: [READ, WRITE, REPORT]
            - attribute_id: 0x0002
              type: UINT16
              value: 250
              access: [READ, WRITE, REPORT]
```

Both values must support coordinator read, write and configured reporting through
the generic adapter. This example does not imply manufacturer-specific frame support:
manufacturer code handling must either be implemented end to end or rejected. A
custom cluster ID alone does not promise interoperability or automatic discovery.
Later bindings to ordinary entities/automations remain optional; no per-device C++
class is required. This example deliberately makes no unreviewed automation promise.

## Endpoint allocation and compatibility policy

Separate compact registration slots from wire endpoint IDs. Store an explicit
slot-to-ID mapping, never size arrays by the largest wire ID or index by ID minus one.

For the numbering slice:

1. With no explicit numbers, keep the old validation reservations and codegen
   consumption order. Snapshot output from this exact base before changing it.
2. If any entity has `endpoint`, reserve all explicit IDs before allocating implicit
   ones. Allocate implicit IDs as the lowest free ID in the existing codegen order.
   Explicit IDs survive insertion and reordering. Implicit IDs have no new stability
   promise after a configuration change. Inserting an explicit ID can displace an
   implicit one; document and test this opt-in consequence.
3. Permit sparse application IDs 1..239, following the existing public ESP32 range.
   Reject zero, negative values, reserved 240..255, values above 255, nonintegers and
   duplicate endpoint ownership. Eight endpoints with sparse IDs still count as eight.
4. Do not enable ESP32 merging for legacy nRF entities. Two simple entities claiming
   the same ID fail clearly in the first slice. Advanced declarations own one endpoint
   containing multiple clusters/attributes instead.
5. Include Zigbee time in reservations and numbering. Preserve its existing implicit
   allocation; exposing a time-specific explicit option can be a separate slice.
6. Count actual registered endpoints, including time, against eight. Do not increase
   `MAX_EP_NUMBER_ZEPHYR`, override the SDK ABI cap, or promise eleven endpoints.

Advanced mode initially rejects any combination with automatically exposed simple
entities or implicit Zigbee time. This early error is intentional and limited to
advanced opt-in configurations. Internal source entities may coexist without taking
endpoints. An explicit endpoint declaration cannot be duplicated; within it, clusters
are unique by ID and role, attributes by ID and supported manufacturer context.
Basic/Identify injection must reject user collisions rather than silently overwrite.

## Architecture and implementation seams

### Python and code generation

The first slice changes only the platform gate for `endpoint`, the nRF allocation
state and uses of wire IDs. Keep ESP32 schema acceptance, merging, device selection
and output unchanged. Do not broaden `cluster`, `report` or `use_device_type` gates
as a side effect. Keep new state under the Zigbee namespace in `CORE.data`, with a
typed per-run structure; preserve existing keys until their consumers are migrated.

Likely touched files: `core/entity_helpers.py`, `components/zigbee/__init__.py`,
`zigbee_zephyr.py`, `zigbee_zephyr.h/.cpp`, and the time codegen call site.
Time currently hardcodes `set_endpoint(slot_index + 1)`: update that setter to the
resolved wire ID even if the registration function signature remains unchanged.
Test that an explicit sensor reserving ID 1 cannot make time's descriptor and
runtime callback IDs diverge. Do not combine unrelated core cleanups with the port.

For the generic stage, extract genuinely platform-neutral endpoint/attribute
normalization and type metadata from the ESP32 backend behind compatibility exports.
ESP32 EZB enum expressions must not leak into ZBOSS generation. Share semantic
metadata; use separate backend emitters. Existing integer validators lack width
bounds in some paths: new nRF capability validation must enforce exact bounds without
silently tightening legacy ESP32 behavior. Generate and compare language schema too.

Emit static value storage, attribute tables, cluster tables, simple descriptors,
reporting contexts and device context. Every pointer registered with ZBOSS must
remain valid for firmware lifetime. Initialize values before context registration
and before stack startup; never point at temporary strings, stack objects or movable
containers. Calculate every count from the final normalized model and assert limits.
Cluster role must be explicit, not inferred from a nonempty attribute list. Simple
descriptor cluster IDs must list input/server clusters before output/client clusters.
Derive reporting context capacity from reportable attributes, including injected ones.
For arbitrary numeric cluster IDs, verify descriptor initialization without relying
on SDK macro token concatenation that only recognizes built-in symbolic cluster IDs.

### Callback routing and thread ownership

Current nRF `add_callback(endpoint, cb)` stores one callback at `endpoint - 1` and
overwrites earlier callbacks. The current lookup also admits zero before subtraction.
The numbering slice needs bounded sparse lookup while retaining legacy handler
behavior. The generic stage uses a static dispatch table keyed by endpoint, cluster
and attribute; include role/manufacturer context where the callback API supplies it.
Reject ambiguous registrations where the callback cannot disambiguate them.

Check callback kind before reading the union. Validate key, type, width, access and
value before accepting a write. Never queue a `zb_bufid_t`, SDK pointer, or a shallow
copy containing `p_data`. Copy the scalar or bounded bytes while the callback owns
them, then dispatch main-loop effects using ESPHome's thread-safe handoff mechanism.
Keep small deferred captures or use a statically bounded queue with index captures.
Specify queue-full behavior and synchronous status mapping before tests go green;
never acknowledge a queued application write that will silently disappear.

ZBOSS storage updates/report requests must obey the stack's thread/locking contract.
Main-loop publication and ZBOSS mutation are separate operations. Avoid recursive
entity writeback, duplicate reports and cross-attribute updates. Audit whether the
SDK callback occurs before or after storage mutation; use pre-write validation hooks
where rejection must leave storage unchanged. A host fake must model that ordering,
and cannot substitute for a real SDK compilation or radio test.

### Typed values, access and reporting

The generic tracer supports only INT16 and UINT16 initially, with explicit read/write/
report access. Implement a platform-neutral bounded codec and a thin ZBOSS adapter.
Verify exact lengths, signed conversion, lower/upper limits and little-endian wire
values; no unaligned or aliasing `reinterpret_cast` loads. Distinguish SDK host-value
storage from serialized wire bytes so conversion does not swap twice. Decode failures
leave values unchanged and return a defined error. Reading/reporting always selects
the same descriptor and backing storage as writing that key.

Test report configuration, intervals, reportable change, local and remote changes,
read-only failures, unknown keys, malformed lengths, and dispatch isolation. Attribute
read/write/report are ZCL foundation operations. Cluster-specific commands need
separate handlers; advertising a cluster does not implement its commands.

Reject STRING/CHAR_STR (0x42), long strings, arrays and all unimplemented types in
the first explicit generic slice. Existing legacy description strings remain intact.
Future CHAR_STR support must own up to 254 payload bytes plus the one-byte length,
treat 0xFF as invalid rather than length 255, validate encoded byte length, and copy
using both `data_variable.size` and `p_data` bounds before deferral. Public SDK 2.9.2
headers expose that pair; that alone does not prove maximum-length callback delivery
or radio transport. Test empty/max/overflow/invalid-prefix/truncated/null cases and
storage lifetime before enabling strings.

### Standard cluster validation

Add standard clusters one at a time after the generic tracer. For Temperature
Measurement, validate the signed hundredths-of-degree scale, invalid sentinel,
representable range and mandatory measured/min/max attributes and relationships.
Use SDK/ZCL definitions as the oracle, not only ESP32 preset dictionaries. Include
NaN, negative temperatures, conversion rounding and out-of-range tests.

Thermostat support is a separate reviewed capability: validate its mandatory
attributes for the chosen feature set, hundredths scaling, sentinels, enum values,
access flags and setpoint relationships. Do not invent default heating behavior,
embed a control algorithm or claim commands from attribute registration. Until real
validation exists, reject unsupported standard-cluster configurations clearly.

## Sequential RED–GREEN roadmap

No implementation is started at this checkpoint. Execute one failing behavior test,
minimal implementation, passing targeted checks and review before the next bullet.
An unavailable test runner is a prerequisite failure, not a RED behavioral result.

1. **Baseline:** run existing relevant unit/config/codegen tests unchanged. Capture
   nRF and ESP32 normalized YAML/codegen numbering goldens, including time, internal
   entities and mixed domains. Normalize only known nondeterminism such as build time.
2. **Optional numbering tracer:** RED for one simple sensor at endpoint 17, with no
   cluster declarations. GREEN through schema, descriptor generation, entity setter,
   sparse callback dispatch and host tests. Then RED–GREEN mixed reservation,
   insertion/reorder stability, duplicate/reserved IDs and the eight/nine boundary.
   All-implicit goldens must remain byte-identical in endpoint-relevant output.
3. **Reviewed advanced schema:** add focused validation/codegen tests for the proposed
   surface; resolve field/access/binding contracts before making it public. Preserve
   ESP32 tests while extracting shared metadata. Test advanced/simple early rejection.
4. **Generic tracer:** one endpoint 17, cluster 0xFC00, two numeric attributes as in
   the sketch. Sequential RED–GREEN registration/count/lifetime, read, write of each
   attribute, deferred payload ownership, and configured reporting. Test the actual
   production codec/router in C++, not a parallel implementation or source-text grep.
5. **Expansion:** a second cluster on the same endpoint, repeated attribute IDs in
   different clusters, same cluster/attribute on another sparse endpoint; then standard
   measurement semantics, optional bindings and more types individually. Strings and
   thermostat remain gated until their validation and SDK evidence exist.
6. **Compile/review:** isolated nRF SDK 2.9.2 and ESP32 compile only using an existing
   safe toolchain. No automatic installation or dashboard cache reuse. Independent
   native read-only review/test run for each completed slice. No push/PR/flash/deploy.

## Backward compatibility and test matrix

| Case | Required checks |
| --- | --- |
| nRF simple sensor/binary sensor/switch/number, no endpoint | Existing YAML passes; identical assignment, generic cluster, strings and reporting descriptors |
| nRF mixed domains plus time; internal entities | Golden reservations/codegen order; time keeps its endpoint; internal entities consume none |
| nRF optional numbering | Sparse 1, 17, 239; explicit IDs stable under reorder/insertion; lowest-free implicit policy |
| nRF invalid numbering | Duplicate ownership, 0, negative, 240..255, >255, noninteger; early useful errors |
| nRF capacity | Eight registered endpoints pass, nine fail outside test bypass; multiple attrs do not consume endpoints |
| ESP32 default and explicit | Existing merge/default/device-class/use_device_type/report behavior and language schema unchanged |
| ESP32 time | TIME client/server still join first endpoint per existing backend |
| Advanced nRF | Opt-in only; no forced migration; duplicate cluster/attribute and Basic/Identify collisions rejected |
| Numeric read/write/report | Bounds/endian/access, type mismatch, callback buffer reuse, wrong callback kind, unknown key, sparse zero guard |
| Static codegen | Descriptor counts, server/client ordering, initialization lifetime and reporting capacity |
| Deferred dispatch | Two writes before loop, full queue, no pointer capture, no recursive publication/cross-talk |
| Standard clusters and strings | Explicitly rejected until their documented capability tests pass |

Python tests belong in `tests/unit_tests/components/zigbee/` and focused core tests
where the shared schema changes. Use actual config/codegen fixtures, not grep-only
assertions. C++ tests use `tests/components/zigbee/`, gtest and the existing host
runner with ASan/UBSan where supported. Add a narrow fake SDK boundary for host
tests; do not claim it verifies the binary ABI. Build YAML uses the existing
`tests/components/zigbee/test.*.yaml` and `validate.*.yaml` conventions and named
packages. Verify schema extraction with `script/build_language_schema.py` in scratch
output. Read runner/install behavior before invoking any build helper.

## Public overlap and review dependencies

- [PR 17402](https://github.com/esphome/esphome/pull/17402), merged 2026-07-08,
  adds ESP32 endpoint merging and explicitly documents changed existing descriptions.
  Reuse its contracts but do not apply that renumbering/merging to default nRF YAML.
- [PR 17388](https://github.com/esphome/esphome/pull/17388), merged 2026-09-30,
  adds opt-in device-class measurement mapping. Its separation from generic defaults
  is useful; it does not provide public arbitrary attribute YAML.
- [PR 22482](https://github.com/esphome/esphome/pull/22482), open at inspection,
  head `67ce5588d632d809bfcdc1c0f16fb150b9f83dfe`, moves generated Zigbee IDs from
  entity schemas into Zigbee validation and changes constants/imports/time. Its actual
  file diff overlaps this port. Do not cherry-pick it implicitly; re-check status
  before a future rebase and rerun numbering and ID goldens if it lands.
- [Issue 18796](https://github.com/esphome/esphome/issues/18796) reports ESP32
  cross-endpoint reports. It motivates routing/report isolation tests; it is a report,
  not independently reproduced evidence of the cause.

Public searches for nRF52 Zigbee clusters, custom attributes and endpoints found
related work, not an exact generic nRF port. This is not proof none exists. Follow
the repository AGENTS, CONTRIBUTING and component/config/codegen/memory test skills;
the [contribution guide](https://developers.esphome.io/contributing/code/) also
requires preserving public contracts. Public SDK evidence used for callback shape:
[2.9.2 zb_zcl_common.h](https://github.com/nrfconnect/sdk-nrfxlib/blob/v2.9.2/zboss/production/include/zcl/zb_zcl_common.h).

The first-turn native independent review confirmed the schema discrepancy, time
allocation, zero/sparse callback hazard, role/order/count requirements and missing
test tools. Those findings are incorporated above. There is no remaining user API
decision blocking the optional numbering slice; broader advanced API remains proposed.
