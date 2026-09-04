# ALPHA3 BLE Control and Telemetry Design

Date: 2026-09-05

Status: Approved

## Summary

Extend ESPHome's existing `alpha3` component from a read-only, delay-based BLE
implementation into a non-blocking Grundfos GENI client for ALPHA3 Model B. The
component will retain its current flow, head, speed, power, voltage, and current
entities while adding:

- operating mode control: Stop, Min, Normal, and Max;
- control mode selection;
- separate constant-speed, constant-pressure, and proportional-pressure
  setpoint numbers;
- total operating hours and cumulative energy consumption;
- diagnostics useful for safe Home Assistant automation;
- a device-profile boundary that permits newer compatible family-52 pumps to be
  added without duplicating the BLE transport.

The first hardware milestone is deliberately read-only. Control is enabled only
after the corrected transport, identity detection, and readback paths have been
verified against the user's ALPHA3 Model B.

## Goals

1. Fix the current notification setup and transaction behavior so sensor values
   are received reliably after an on-demand BLE connection.
2. Ensure no component operation blocks the ESPHome loop for 50 ms or longer.
3. Implement complete GENI framing, fragmentation, reassembly, CRC checking,
   transaction timeout, and response validation.
4. Add the requested telemetry and non-optimistic pump controls.
5. Preserve existing `sensor: - platform: alpha3` configurations.
6. Make support for newer protocol-compatible pumps a profile addition rather
   than a transport rewrite.
7. Prevent control writes to an unidentified or unverified pump profile.

## Non-goals

- Supporting every Grundfos GENI device in the initial change.
- Simultaneous phone and ESP connections to a pump that permits only one BLE
  client.
- Automatically connecting whenever a Home Assistant control is changed.
- Queuing a control command while disconnected for execution on a later
  connection.
- Exposing destructive maintenance commands such as factory reset.
- Adding Auto Night Setback, air venting, or alarm reset controls in the first
  implementation. Their read-only status may be added later, and any command
  must be an explicit momentary action rather than a persistent switch.

## Reverse-engineering evidence

The design is based on the decompiled Grundfos GO 7.0.4 application, its bundled
GENI XML profiles, and its ALPHA3 Model B demo data.

### BLE transport

- Service UUID: `0xFE5D`.
- Characteristic UUID: `859cffd1-036e-432a-aa28-1a0085b87ba9`.
- The characteristic supports write-without-response and notifications.
- Notification delivery depends on registering for notifications and writing
  the Client Characteristic Configuration Descriptor (`0x2902`).
- The application permits one outstanding GENI transaction and uses a one
  second response timeout.
- Frames larger than the BLE payload are fragmented.
- CRC is CRC-16/CCITT with polynomial `0x1021`, initial value `0xFFFF`, computed
  from the GENI length byte through the body, complemented at the end, and
  encoded high byte followed by low byte.

### Current runtime failure

The existing component registers for notifications during service discovery,
before authentication has necessarily completed. It then marks itself
established on the registration event without checking the event status or
waiting for CCCD confirmation. On the observed pump, service discovery finished
before authentication. No GENI response notifications followed, so Home
Assistant sensors remained `Unknown`.

The existing `update()` also performs two `delay(25)` calls, which accounts for
the observed 51 ms ESPHome component warning.

### ALPHA3 Model B objects

The profile `geni_profile_52_1_0.xml` identifies the target as unit family 52,
type 1, version 0. Relevant objects are:

| Purpose | Data ID / sub-ID | Type and version | Size |
|---|---:|---:|---:|
| Flow/head process values | 93 / 289 | 304 v1 | 24 bytes |
| Motor/electrical details | 87 / 69 | 256 v1 or v2 | 37 or 41 bytes |
| Operation history | 93 / 1 | 248 v2 | 28 bytes |
| Overall energy | 87 / 1 | 232 v1 | variable, first value is an 8-byte double |
| Local operation request/status | 86 / 6 | 303 v1 | 7 bytes |
| Prioritized operation status | 86 / 7 | 303 v1 | 7 bytes |
| Realized operation status | 86 / 8 | 303 v1 | 7 bytes |
| Local control-mode request/status | 86 / 10 | 303 v1 | 7 bytes |
| Constant-speed user configuration | 86 / 14 | 302 v1 | 18 bytes |
| Constant-pressure user configuration | 86 / 16 | 302 v1 | 18 bytes |
| Proportional-pressure user configuration | 86 / 18 | 302 v1 | 18 bytes |
| Operation-mode request configuration | 86 / 100 | 340 v1 | 4 bytes |

Operation-mode protocol values used by the requested UI are Normal/Auto `0`,
Stop `1`, Min `2`, and Max `3`.

The supported ALPHA3 Model B control modes exposed by the app are:

| UI value | Protocol value |
|---|---:|
| Constant pressure | 0 |
| Proportional pressure | 1 |
| Constant speed | 2 |
| AUTOADAPT radiator | 13 |
| AUTOADAPT underfloor | 14 |
| AUTOADAPT radiator and underfloor | 15 |

Type 302 contains a two-byte feedback ID, a four-byte local setpoint, and three
four-byte PID fields. A setpoint update must preserve every field other than the
local setpoint.

Operation history stores total starts at offset 0 and operating time in seconds
at offset 8. Overall energy stores `system_energy` in watt-seconds at offset 0;
the Home Assistant value is `system_energy / 3,600,000` kWh. Bundled demo values
confirm these conversions.

## Architecture

### Protocol layer

Introduce a stateless `Alpha3Protocol` layer responsible for:

- CRC calculation and validation;
- request construction;
- complete-frame validation;
- endian-safe integer, float, and double decoding;
- object header decoding;
- serialization of known writable objects;
- bounds checking before all reads and writes.

It must not know about ESPHome entities, the BLE connection state, or polling.
Protocol tests can therefore use captured byte arrays without an ESP32 BLE
stack.

### Component coordinator

`Alpha3` remains the public component and owns:

- BLE lifecycle and readiness state;
- a fixed-capacity transaction queue;
- fragment transmission and notification reassembly;
- the one active transaction;
- timeout and retry decisions;
- device identity and selected profile;
- polling orchestration;
- entity publication and command verification.

Runtime buffers and queues use fixed-capacity storage. The implementation must
not repeatedly allocate or resize heap-backed containers after `setup()`.

### Entity adapters

Small select and number classes hold a required `Alpha3 *` supplied through
their constructors. They forward requested values to the coordinator and are
updated only by confirmed device reads. The coordinator remains the only code
that constructs or submits protocol transactions.

## BLE readiness state machine

Service discovery and authentication can finish in either order. The component
tracks them as independent prerequisites:

1. **Disconnected**: no handles or transactions are valid.
2. **Connected**: encryption is requested.
3. **Waiting for prerequisites**: service/characteristic discovery and
   authentication may complete in either order.
4. **Subscribing**: only after both prerequisites succeed, register through the
   BLE client parent's notification helper.
5. **Enabling notifications**: validate notification-registration status and
   wait for a successful CCCD descriptor write.
6. **Ready**: publish protocol readiness and enqueue identity plus initial
   reads.

Failure at authentication, service lookup, notification registration, or CCCD
write leaves the component non-ready and logs the exact stage and status.

Disconnect performs all of the following:

- marks protocol readiness false;
- invalidates handles and assembly state;
- cancels the transaction timeout;
- fails the active transaction;
- clears queued work, including commands;
- retains the last valid sensor readings rather than publishing `NaN`.

At boot, entities remain `Unknown` until their first successful read. Retaining
values across an intentional on-demand disconnect makes their behavior useful
in Home Assistant, while the readiness binary sensor identifies whether they
are live.

## Transaction engine

Only one GENI request may be active. A transaction contains its operation,
object descriptor, expected response, priority, retry count, encoded frame, and
completion behavior.

The queue has fixed capacity and two effective priorities:

- user commands and their prerequisite/verification reads;
- identity, initial reads, and periodic polling.

Periodic requests already active or queued are coalesced. If capacity is
exhausted, redundant/lower-priority polls are discarded first. A user command
that still cannot be accepted fails immediately and logs a warning; it is never
silently deferred.

Frame fragments are emitted asynchronously through write-without-response.
There are no `delay()` calls. Once the last fragment is accepted by the BLE API,
the one-second protocol response timeout begins. Completion is based on a valid
GENI response notification, not on a write-characteristic callback that may not
exist for a no-response write.

Notification fragments are appended to a fixed assembly buffer. The declared
frame length determines completion. A completed response must pass:

- start/header validation;
- declared versus received length validation;
- CRC validation;
- expected operation and object identity validation;
- expected type/version and minimum payload-length validation.

Unexpected, malformed, or stale frames are logged and discarded without being
published as another transaction's result.

A timed-out or invalid read can be retried once. A write is never blindly
retried because the first write may have reached the pump. Instead, its
verification read determines the resulting state.

`PollingComponent::update()` only coalesces/enqueues the configured reads and
returns. It performs no BLE wait, sleep, parsing loop, or multi-request burst.

## Identity and device profiles

The standard read-only parameters identify a pump:

- class 2, ID 148: unit family;
- class 2, ID 149: unit type;
- class 2, ID 150: unit version.

A compile-time profile registry maps a verified identity to:

- product/profile name;
- readable object descriptors and supported versions;
- telemetry decoders and unit conversions;
- operation and control mode choices;
- setpoint objects, units, and allowed ranges;
- independent read and write capability flags.

The initial writable profile is ALPHA3 Model B `52/1/0` only.

Newer app profiles for family 52 use the same operation, control, setpoint,
history, energy, and electrical object mappings. Their hydraulic telemetry is
different: types 6, 7, 20, 21, and 27 use data ID/sub-ID `93/290`, type `565 v2`,
with a 36-byte extended process-value payload instead of ALPHA3 Model B's
`93/289`, type `304 v1`, 24-byte payload. This demonstrates why compatibility
must be per capability rather than inferred from the common service UUID.

Compatibility policy:

- An exact known profile enables only its declared capabilities.
- A newer unknown version of a known type may use validated compatible
  read-only object schemas, one capability at a time.
- An unknown type exposes identity and diagnostics but no pump control.
- Write capability requires an explicit profile entry and hardware verification.
- A longer known-version-compatible payload may be prefix-decoded only when the
  profile explicitly declares that safe.
- Unknown enum values are preserved for logging and do not become a known select
  option.

Adding a compatible device consists of adding its profile/object descriptors,
captured protocol fixtures, and hardware verification. The transport and queue
are reused unchanged. The public integration remains named `alpha3` for this
work; the protocol/transport boundary can later be extracted into a
`grundfos_ble` hub if support expands beyond ALPHA-family circulators.

## Entity model and compatibility

The existing sensor platform remains the hub and its `id` is referenced by new
platforms. Existing configurations require no change.

Conceptual configuration:

```yaml
sensor:
  - platform: alpha3
    id: floor_pump
    ble_client_id: alpha3_ble
    flow:
      name: Floor Pump Flow
    head:
      name: Floor Pump Head
    speed:
      name: Floor Pump Speed
    power:
      name: Floor Pump Power
    voltage:
      name: Floor Pump Voltage
    current:
      name: Floor Pump Current
    operating_hours:
      name: Floor Pump Operating Hours
    energy:
      name: Floor Pump Energy
    starts:
      name: Floor Pump Starts
    alarm_code:
      name: Floor Pump Alarm Code
    warning_code:
      name: Floor Pump Warning Code

select:
  - platform: alpha3
    alpha3_id: floor_pump
    operating_mode:
      name: Floor Pump Operating Mode
    control_mode:
      name: Floor Pump Control Mode

number:
  - platform: alpha3
    alpha3_id: floor_pump
    constant_speed_setpoint:
      name: Floor Pump Constant Speed Setpoint
    constant_pressure_setpoint:
      name: Floor Pump Constant Pressure Setpoint
    proportional_pressure_setpoint:
      name: Floor Pump Proportional Pressure Setpoint

binary_sensor:
  - platform: alpha3
    alpha3_id: floor_pump
    ready:
      name: Floor Pump Protocol Ready
```

The three setpoint numbers are intentionally separate because speed and pressure
use different units, ranges, and underlying configuration objects. Constant
speed is presented in RPM. Pressure-mode setpoints are presented as pump head in
meters and converted by the selected profile.

The energy sensor uses kWh, device class energy, and state class
`total_increasing`. Operating hours uses hours and a duration device class.
Starts is a monotonically increasing total.

Operating and control mode selects are non-optimistic. Their displayed state is
the confirmed local request read from the pump, not the last Home Assistant
command. A separate read-only realized operating state/control source may be
exposed as diagnostic text so automations can distinguish local configuration
from an active internal override.

## Control flows

Commands are accepted only while the component is protocol-ready and the
selected device profile explicitly permits the capability. A disconnected
command is rejected, not saved for the next connection.

### Operating mode

1. Read the complete operation-mode request configuration at `86/100`.
2. Preserve its priority and copy flags.
3. Replace only the local operation mode with Stop, Min, Normal/Auto, or Max.
4. Write the complete four-byte object.
5. Read local and realized operation status.
6. Publish the select state only from the confirmed local status.

### Control mode

1. Read the complete local control-mode request/status object at `86/10`.
2. Preserve source, operation mode, and setpoint fields not targeted by the
   operation.
3. Replace the control mode with a profile-supported value.
4. Write the complete object.
5. Read local and realized status.
6. Publish the confirmed control mode or retain the prior state on failure.

### Setpoint

1. Choose `86/14`, `86/16`, or `86/18` from the number entity.
2. Read its complete 18-byte type-302 configuration.
3. Validate the requested value against the profile/device range.
4. Preserve feedback ID and every PID field.
5. Replace only `local_set_point`.
6. Write the complete object.
7. Read the configuration back and publish only the confirmed converted value.

Repeated identical pending commands are coalesced. Distinct user commands are
processed in submission order.

## Daily automation diagnostics

The initial implementation includes or leaves a direct path for these read-only
artifacts:

- protocol-ready state, distinct from BLE connected;
- unit family, type, version, and chosen profile in diagnostic logs;
- total starts from operation history;
- raw alarm code from class 2, ID 158;
- raw warning code from class 2, ID 156;
- realized operation state and active control source.

Raw alarm/warning codes remain useful for `> 0` alert automations even before a
complete human-readable event-code catalog is implemented.

Useful follow-up entities are Auto Night Setback enabled/status, air-venting
state, starts during the last hour and last 24 hours, and operating time during
the last 24 hours. Alarm reset and air venting are candidates for explicit
actions after their protocol and safety behavior is verified.

A safe on-demand Home Assistant/ESPHome automation sequence is:

1. connect the BLE client;
2. wait for protocol-ready rather than only BLE connected;
3. issue a select or number change;
4. wait for the entity's verified state or an operation-result signal;
5. disconnect the BLE client.

## Error handling

- Authentication, discovery, subscribe, and CCCD errors keep the component
  non-ready.
- A malformed length or CRC never publishes values.
- A mismatched response never completes the active transaction.
- Read failure after one retry leaves the last valid entity state unchanged.
- Write or readback failure leaves the select/number at its last confirmed state.
- Queue pressure removes redundant polls before rejecting commands.
- Disconnect clears all pending commands.
- Unsupported identity/schema/range disables only the affected capability and
  emits a clear warning.
- Normal logs summarize state changes and failures; byte dumps are limited to
  verbose/very-verbose logging.

## Testing

### Protocol unit tests

Add host-side C++ tests for:

- known request and response CRC vectors;
- frame construction;
- fragmented and single-notification reassembly;
- invalid start byte, declared length, CRC, operation, object, type, and version;
- big-endian integer, float, and double decoding;
- type 256 v1/v2 compatible electrical fields;
- type 304 ALPHA3 and type 565 extended telemetry decoding;
- demo operating-seconds to hours conversion;
- demo watt-seconds to kWh conversion;
- operation/control serialization;
- type-302 setpoint replacement while every other byte remains unchanged;
- unknown enum and unsupported-profile behavior.

### Configuration and build tests

Add ESP32-IDF component tests covering:

- the existing six-sensor configuration unchanged;
- all new optional sensors;
- selects and their hub reference;
- all three number entities;
- protocol-ready diagnostics;
- invalid or missing hub references and schema conflicts.

### Hardware rollout

1. Deploy transport repair with existing read-only telemetry only. Verify
   authentication/CCCD ordering, protocol-ready, notifications, CRC, polling,
   intentional disconnect/reconnect, and absence of loop-blocking warnings.
2. Enable identity, profile selection, hours, energy, starts, alarm, and warning
   reads.
3. Perform a no-change write/readback of a configuration object.
4. Make one small reversible setpoint adjustment and verify it both by protocol
   readback and at the pump/app.
5. Test control-mode changes.
6. Test Min, Normal, and Max.
7. Test Stop last because it can interrupt heating operation.

Keep OTA recovery and the BLE disconnect control available throughout testing.
The phone app must be disconnected while the ESP holds the pump's single BLE
connection.

## Completion criteria

- Existing sensor YAML still validates and compiles.
- The observed ALPHA3 Model B reaches protocol-ready after on-demand connection
  and all configured reads acquire values.
- No alpha3 operation produces ESPHome's 50 ms blocking warning.
- Frames and CRC are validated before parsing.
- Hours and kWh match the app/demo conversions.
- Selects and numbers update only after successful pump readback.
- Disconnect cannot cause a stale command to execute later.
- Unknown/newer devices never receive writes unless an explicit verified
  writable profile permits them.
- Tests cover protocol errors, supported versions, preservation writes, and
  backward-compatible configuration.
