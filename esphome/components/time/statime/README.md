# Experimental clock discipline

This opt-in draft connects Statime to ESPHome's common `time` component. Existing
sources still acquire time and consumers still read the ESP system clock. Without
`discipline`, the Rust component and clock interception are not linked.

## Configuration and build

Supported targets are ESP32, ESP32-S3 and ESP32-C6 with native ESP-IDF. Configure
`framework.advanced.loop_task_stack_size: 32768`, then add `discipline: {id:
clock_discipline}` to one existing time source. At most two sources are supported.
The source's existing update interval controls acquisition; the discipline uses
ESPHome's polling scheduler once per second for steering and holdover.

Rust is installed automatically on the first firmware build that enables
`discipline`. C6 uses Rust 1.98.1 (minimal profile and only its RISC-V target);
ESP32/S3 use esp-rs 1.97.0.0 with matching sources for `-Zbuild-std=core`.
The adapter is always compiled from source with `--locked`; no prebuilt firmware
library is downloaded. `esphome config` and code generation do not install Rust.

`build.py` uses ESPHome's existing verified/resumable download and archive helpers.
Rustup 1.29.1 bootstrap and esp-rs archives have committed SHA-256 checksums;
Rustup verifies its versioned compiler/target downloads. Installation is protected
by an inter-process lock. Completed toolchains are reused without network checks,
and incomplete installs do not get a completion marker.

Everything is kept under the existing ESP-IDF tools cache's `rust/` directory,
including an isolated Cargo dependency cache and Rustup home. The existing
`ESPHOME_ESP_IDF_PREFIX` override controls its location, so Docker builders can
reuse their persistent IDF cache. No shell profiles, global PATH or personal Rust
installations are changed. An empty builder needs network access on its first
build. On Linux x86-64, the C6 Rust cache is approximately 672 MiB after a build;
Xtensa adds its compiler and sources only when an ESP32/S3 build needs them.

Host packages are selected for Linux glibc x86-64/AArch64, macOS Intel/Apple
Silicon, and Windows x86-64. The esp-rs release has no Intel macOS package, so
ESP32/S3 builds on that host are rejected explicitly. Musl and 32-bit Linux hosts
are not supported by this draft. A native host linker is still required for Cargo
build scripts (on Windows, MSVC Build Tools). End-to-end installation/build checks
have been run on Linux x86-64; the other hosts still need physical CI runners.
ESP32-C3 remains unsupported because the dependency requires unavailable atomics.

The helper is bundled with the external `time` component so the existing ESPHome
2026.9 builder can load this draft without a core upgrade. CMake invokes it using
the ESPHome interpreter recorded during code generation, rather than IDF's Python
venv. It can be moved to shared build infrastructure in a subsequent PR split.

## Legacy observations and steering

The common `synchronize_epoch_` path passes the original source timestamp to the
controller. Sources such as SNTP write libc time directly before notifying their
callbacks. An opt-in linker `--wrap=settimeofday` hook captures those writes in a
mutex-protected mailbox and, after initialization, prevents raw writes from
bypassing discipline. Only an unambiguous captured write is consumed by a sync
notification. This process-wide hook needs upstream architecture review; source
attribution cannot be guaranteed for unrelated external clock writers.

Legacy timestamps are untracked observations with a default one-second standard
deviation, not a measured precision guarantee. Sources expire after three of
their update intervals, with a minimum timeout of one minute. The controller can
learn local frequency error and continue steering during holdover. A private
affine clock model supplies Statime's clock callbacks; steps and repeated
`adjtime()` corrections publish its time through the existing ESP system clock.
Consumers do not read a separate public clock. Frequency correction is limited
to 200 ppm. Offset slewing uses a ten-minute time constant: the upstream default
of eight seconds can exhaust that range with only 1.6 ms of estimated offset.
The slower phase correction reduces steering driven by noisy legacy timestamps;
it is not a guarantee that frequency learning completes in ten minutes.
UTC/TAI conversion currently assumes the 37-second offset in effect
since 2017; leap announcements and future leap changes are not implemented.

For diagnostics, `get_estimated_drift_ppm()` removes the applied steering from
Statime's frequency estimate in its additive model. Positive drift means the
uncorrected local clock runs fast. `get_drift_uncertainty_ppm()` returns the
model's one-standard-deviation uncertainty. These are estimates, not independent
hardware measurements. `get_frequency_ppm()` still reports the applied correction,
including phase slewing. Read these diagnostics only when `has_estimate()` is true;
frequency learning requires observations separated in time.

## Dependency and ABI

`statime-algo` and `statime-base` use the exact integration revision
`cc188798d01ec7546b5dc6f17bf22791bf96df20` of `Nebensound/ntpd-rs`, based on
upstream `1bc46908dea2730f592afc991f5a9f05c211b22c`. It combines proposed fixes
for [no_std dispersion](https://github.com/pendulum-project/ntpd-rs/pull/2500),
[frequency access](https://github.com/pendulum-project/ntpd-rs/pull/2501),
[fallible updates](https://github.com/pendulum-project/ntpd-rs/pull/2505), and
independently sized fixed-capacity storage buffers to reduce stack usage.
The storage change still needs a separate upstream proposal. This temporary fork
is an explicit draft dependency, not an accepted upstream release. Replace it
with a tested upstream revision before considering this integration merge-ready.

`include/time_discipline.h` defines the internal ABI. Opaque, aligned storage is
allocated once during setup with the alignment reported by Rust and never moved.
ESP-IDF's ordinary heap only guarantees four-byte alignment, so it is insufficient
for the eight-byte Rust state alignment on C6. Estimates cross the FFI boundary
through an explicitly aligned stack buffer before being copied into the component.
Rust does not allocate. Calls are
serialized on the ESPHome thread; only the capture mailbox crosses threads.
Callbacks and context must outlive the controller. Timestamps are signed 64-bit
TAI nanoseconds, frequency is seconds per second, and uncertainty is a positive
standard deviation. Invalid input is rejected; algorithm or clock errors latch
a failure and restore legacy clock writes. Rust panics log and abort without
unwinding across C. No historical Statime API compatibility shims are provided.

## Validation and remaining gates

The Rust tests include simulated +/-50 ppm drift, learning, six-hour holdover,
and reacquisition. The host runtime test links the actual C++ bridge to the Rust
archive and checks both legacy input paths, callbacks, duplicate/ambiguous syncs,
public system time, holdover and outliers. Minute-spaced observations with bounded
20 ms timestamp noise check steering saturation and accuracy in both drift
directions. Its OS clock is simulated: it does not
change the host clock or prove real-device accuracy.
The runtime test also models a four-byte-aligned ordinary heap allocation to
check initialization with Rust's stricter alignment requirement.

Run at the repository root, with Rust 1.98.1 active:

```sh
cargo test --locked --manifest-path esphome/components/time/statime/Cargo.toml
cargo build --release --locked --features std --manifest-path esphome/components/time/statime/Cargo.toml
python tests/time_discipline/run_runtime_test.py esphome/components/time/statime/target/release/libesphome_time_discipline.a
```

A C6 firmware build includes the live Rust controller. Measured C6 controller
storage is 1688 bytes; individual release stack frames include approximately
5216 bytes for initialization, 10240 for observation and 5136 for steering.
These are not complete call-chain bounds. Full stack bounds and physical-device
high-water measurements remain required; the 32768-byte configuration is not a
claim of proven worst-case stack safety. ESP32/S3 runtime resource validation,
host-platform CI coverage and public user documentation are also pending.
Board-specific YAML and display fixtures are intentionally not part of this PR.
