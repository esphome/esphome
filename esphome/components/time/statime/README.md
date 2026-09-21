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

Rust must already be installed in the firmware build environment. On ESP32-C6:

```sh
rustup toolchain install 1.98.1 --profile minimal --target riscv32imac-unknown-none-elf
```

ESP32/S3 require the esp-rs Xtensa toolchain registered as `esp-1.97.0.0`, with
its matching Rust sources for `-Zbuild-std=core`. CMake resolves both Cargo and
rustc through rustup and builds the archive from source with `--locked`.
ESP32-C3 is rejected because the dependency needs atomic operations unavailable
on `riscv32imc-unknown-none-elf`. Normal ESPHome builders do not yet install these
Rust toolchains automatically; `external_components` alone does not install them.
Toolchain provisioning is a remaining upstream integration requirement.

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
to 200 ppm. UTC/TAI conversion currently assumes the 37-second offset in effect
since 2017; leap announcements and future leap changes are not implemented.

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
allocated once during setup and never moved. Rust does not allocate. Calls are
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
public system time, holdover and outliers. Its OS clock is simulated: it does not
change the host clock or prove real-device accuracy.

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
standard CI/toolchain integration, and public user documentation are also pending.
Board-specific YAML and display fixtures are intentionally not part of this PR.
