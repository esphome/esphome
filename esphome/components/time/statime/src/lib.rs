#![cfg_attr(not(any(test, feature = "std")), no_std)]

use core::cell::Cell;
use core::ffi::c_void;
use core::mem::{align_of, size_of};
use core::ptr::NonNull;

use statime_algo::{
    AlgoError, ClockConfig, ControllerConfig, KalmanController, KalmanLink, NoAllocKalmanStorage,
};
use statime_base::{
    Clock, ClockError, ClockId, Controller, Direction, Duration, LeapStatus, Link, Measurement,
    TAI, Timestamp,
};

const SOURCES: usize = 2;
const STORAGE: usize = (2 + SOURCES) * (2 + SOURCES);
const OK: u32 = 0;
const INVALID_ARGUMENT: u32 = 1;
const BUSY: u32 = 2;
const CLOCK_ERROR: u32 = 3;
const NONMONOTONIC: u32 = 4;
const ALGORITHM_ERROR: u32 = 5;
const FAULTED: u32 = 6;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ClockCallbacks {
    context: *mut c_void,
    now: Option<unsafe extern "C" fn(*mut c_void, *mut i64) -> i32>,
    set_frequency: Option<unsafe extern "C" fn(*mut c_void, f64, *mut i64) -> i32>,
    get_frequency: Option<unsafe extern "C" fn(*mut c_void, *mut f64) -> i32>,
    step: Option<unsafe extern "C" fn(*mut c_void, i64, *mut i64) -> i32>,
    max_frequency_ratio: f64,
}

// The FFI contract keeps context alive and calls serialized. This adapter never
// starts a thread or sends a clock anywhere; Send is required by Statime's Clock.
unsafe impl Send for ClockCallbacks {}

fn timestamp(ns: i64) -> Result<Timestamp<TAI>, ClockError> {
    let ns = u64::try_from(ns).map_err(|_| ClockError::InvalidValue)?;
    Ok(Timestamp::from_seconds_nanos_since_unix_epoch(
        ns / 1_000_000_000,
        (ns % 1_000_000_000) as u32,
    ))
}

fn clock_result(status: i32, ns: i64) -> Result<Timestamp<TAI>, ClockError> {
    if status != 0 {
        return Err(ClockError::Unknown);
    }
    timestamp(ns)
}

impl Clock<TAI> for ClockCallbacks {
    fn now(&self) -> Result<Timestamp<TAI>, ClockError> {
        let mut ns = -1;
        let callback = self.now.ok_or(ClockError::NotSupported)?;
        // SAFETY: checked callbacks and context are retained under the FFI contract.
        clock_result(unsafe { callback(self.context, &mut ns) }, ns)
    }
    fn set_frequency(&self, ratio: f64) -> Result<Timestamp<TAI>, ClockError> {
        if !ratio.is_finite() || ratio.abs() > self.max_frequency_ratio {
            return Err(ClockError::InvalidValue);
        }
        let mut ns = -1;
        let callback = self.set_frequency.ok_or(ClockError::NotSupported)?;
        clock_result(unsafe { callback(self.context, ratio, &mut ns) }, ns)
    }
    fn get_frequency(&self) -> Result<f64, ClockError> {
        let mut ratio = f64::NAN;
        let callback = self.get_frequency.ok_or(ClockError::NotSupported)?;
        if unsafe { callback(self.context, &mut ratio) } != 0 {
            return Err(ClockError::Unknown);
        }
        if !ratio.is_finite() || ratio.abs() > self.max_frequency_ratio {
            return Err(ClockError::InvalidValue);
        }
        Ok(ratio)
    }
    fn max_frequency(&self) -> Result<f64, ClockError> {
        Ok(self.max_frequency_ratio)
    }
    fn step_clock(&self, offset: Duration) -> Result<Timestamp<TAI>, ClockError> {
        let offset = i64::try_from(offset.as_nanos()).map_err(|_| ClockError::InvalidValue)?;
        let mut ns = -1;
        let callback = self.step.ok_or(ClockError::NotSupported)?;
        clock_result(unsafe { callback(self.context, offset, &mut ns) }, ns)
    }
    // This adapter exposes uncertainty through esphome_time_estimate. These
    // informational hooks do not mutate the underlying clock or claim OS support.
    fn error_estimate_update(&self, _: Duration, _: Duration) -> Result<(), ClockError> {
        Ok(())
    }
    fn synchronization_update(&self, _: bool) -> Result<(), ClockError> {
        Ok(())
    }
    fn leap_update(&self, status: LeapStatus) -> Result<(), ClockError> {
        match status {
            LeapStatus::None => Ok(()),
            _ => Err(ClockError::NotSupported),
        }
    }
}

type Storage = NoAllocKalmanStorage<ClockCallbacks, STORAGE, 1, SOURCES, SOURCES, 4>;
type Algorithm = KalmanController<Storage, ClockCallbacks>;
type Source = KalmanLink<ControllerRef, Storage, ClockCallbacks>;

struct ControllerRef(NonNull<Algorithm>);
impl AsRef<Algorithm> for ControllerRef {
    fn as_ref(&self) -> &Algorithm {
        // SAFETY: init pins State before creating links; links drop before the
        // controller. The caller cannot move State during its initialized lifetime.
        unsafe { self.0.as_ref() }
    }
}

struct State {
    // Field order matters: links must be dropped while controller is still alive.
    sources: [Option<Source>; SOURCES],
    controller: Algorithm,
    clock: ClockCallbacks,
    id: ClockId,
    busy: Cell<bool>,
    faulted: Cell<bool>,
}

#[repr(C)]
pub struct Estimate {
    offset_seconds: f64,
    offset_variance: f64,
    frequency_ratio: f64,
    frequency_variance: f64,
    dispersion_seconds: f64,
    active_sources: u32,
}

fn error_code(error: AlgoError) -> u32 {
    match error {
        AlgoError::ClockError(_) => CLOCK_ERROR,
        AlgoError::NonMonotonicTimeProgression { .. } => NONMONOTONIC,
        _ => ALGORITHM_ERROR,
    }
}

fn aligned<T>(pointer: *const T) -> bool {
    !pointer.is_null() && (pointer as usize).is_multiple_of(align_of::<T>())
}

enum AdapterError {
    InvalidArgument,
    Algorithm(AlgoError),
}

impl From<AlgoError> for AdapterError {
    fn from(error: AlgoError) -> Self {
        Self::Algorithm(error)
    }
}

impl From<ClockError> for AdapterError {
    fn from(error: ClockError) -> Self {
        Self::Algorithm(error.into())
    }
}

// SAFETY for all handle operations: caller supplies live State storage, and
// serializes calls. Null/alignment checks cannot prove a pointer's provenance.
unsafe fn with_state(
    storage: *mut c_void,
    operation: impl FnOnce(&State) -> Result<(), AdapterError>,
) -> u32 {
    let pointer = storage.cast::<State>();
    if !aligned(pointer) {
        return INVALID_ARGUMENT;
    }
    let state = unsafe { &*pointer };
    if state.busy.get() {
        return BUSY;
    }
    if state.faulted.get() {
        return FAULTED;
    }
    state.busy.set(true);
    let result = operation(state);
    state.busy.set(false);
    match result {
        Ok(()) => OK,
        Err(AdapterError::InvalidArgument) => INVALID_ARGUMENT,
        Err(AdapterError::Algorithm(error)) => {
            state.faulted.set(true);
            error_code(error)
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn esphome_time_size() -> usize {
    size_of::<State>()
}
#[unsafe(no_mangle)]
pub extern "C" fn esphome_time_alignment() -> usize {
    align_of::<State>()
}

/// # Safety
/// Follow the storage and callback lifetime contract in time_discipline.h.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_init(
    storage: *mut c_void,
    size: usize,
    clock: ClockCallbacks,
    sources: u32,
) -> u32 {
    let pointer = storage.cast::<State>();
    if !aligned(pointer)
        || size < size_of::<State>()
        || sources == 0
        || sources as usize > SOURCES
        || clock.now.is_none()
        || clock.set_frequency.is_none()
        || clock.get_frequency.is_none()
        || clock.step.is_none()
        || !clock.max_frequency_ratio.is_finite()
        || clock.max_frequency_ratio <= 0.0
        || clock.max_frequency_ratio >= 1.0
    {
        return INVALID_ARGUMENT;
    }
    let config = ControllerConfig {
        select_offset_uncertainty_window: 3.0,
        select_link_uncertainty_window: 3.0,
        select_delay_uncertainty_window: 1.0,
        select_max_window_size: 10.0,
        minimum_agreeing_sources: 1,
    };
    let clock_config = ClockConfig {
        // Legacy observations have coarse uncertainty and may be minutes apart.
        // Spread phase correction over ten minutes so millisecond timestamp noise
        // does not repeatedly exhaust the clock's 200 ppm steering range.
        slew_time_constant: Duration::from_seconds_nanos(600, 0),
        ..ClockConfig::default()
    };
    let (controller, id) = match Algorithm::new(clock, clock_config, config) {
        Ok(result) => result,
        Err(error) => return error_code(error),
    };
    // SAFETY: storage is writable and not initialized; no internal references yet.
    unsafe {
        pointer.write(State {
            sources: core::array::from_fn(|_| None),
            controller,
            clock,
            id,
            busy: Cell::new(false),
            faulted: Cell::new(false),
        });
    }
    let controller =
        unsafe { NonNull::new_unchecked(core::ptr::addr_of_mut!((*pointer).controller)) };
    for index in 0..sources as usize {
        let source = match Algorithm::create_untracked_link(
            ControllerRef(controller),
            id,
            None,
            Default::default(),
        ) {
            Ok(source) => source,
            Err(error) => {
                unsafe {
                    pointer.drop_in_place();
                }
                return error_code(error);
            }
        };
        // SAFETY: only the distinct sources field is modified, not the controller
        // referred to by the newly created and previously created links.
        unsafe {
            (*pointer).sources[index] = Some(source);
        }
    }
    OK
}

/// # Safety
/// The handle must be live and exclusively owned as specified in the C header.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_destroy(storage: *mut c_void) -> u32 {
    let pointer = storage.cast::<State>();
    if !aligned(pointer) {
        return INVALID_ARGUMENT;
    }
    if unsafe { (*pointer).busy.get() } {
        return BUSY;
    }
    unsafe {
        pointer.drop_in_place();
    }
    OK
}

/// # Safety
/// The handle must satisfy the C header's initialized storage contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_observe(
    storage: *mut c_void,
    source: u32,
    reference_ns: i64,
    local_ns: i64,
    uncertainty_ns: i64,
) -> u32 {
    if source as usize >= SOURCES || reference_ns < 0 || local_ns < 0 || uncertainty_ns <= 0 {
        return INVALID_ARGUMENT;
    }
    unsafe {
        with_state(storage, |state| {
            let link = state.sources[source as usize]
                .as_ref()
                .ok_or(AdapterError::InvalidArgument)?;
            link.measurement(
                Measurement {
                    send_timestamp: timestamp(reference_ns)?,
                    recv_timestamp: timestamp(local_ns)?,
                    uncertainty: Duration::from_seconds_nanos(
                        uncertainty_ns / 1_000_000_000,
                        (uncertainty_ns % 1_000_000_000) as u32,
                    ),
                },
                Direction::Reverse,
            )
            .map_err(Into::into)
        })
    }
}

/// # Safety
/// The handle must satisfy the C header's initialized storage contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_set_usable(
    storage: *mut c_void,
    source: u32,
    usable: u32,
) -> u32 {
    if source as usize >= SOURCES || usable > 1 {
        return INVALID_ARGUMENT;
    }
    unsafe {
        with_state(storage, |state| {
            state.sources[source as usize]
                .as_ref()
                .ok_or(AdapterError::InvalidArgument)?
                .external_data_update(Duration::ZERO, None, usable == 1)
                .map_err(Into::into)
        })
    }
}

/// # Safety
/// The handle must satisfy the C header's initialized storage contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_update(storage: *mut c_void) -> u32 {
    unsafe {
        with_state(storage, |state| {
            state.controller.update().map_err(Into::into)
        })
    }
}

/// # Safety
/// The handle must be live; output must point to separate writable Estimate storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn esphome_time_estimate(storage: *mut c_void, output: *mut Estimate) -> u32 {
    if !aligned(output) {
        return INVALID_ARGUMENT;
    }
    unsafe {
        with_state(storage, |state| {
            let offset = state.controller.clock_offset(state.id)?;
            let frequency = state.controller.clock_frequency(state.id)?;
            let snapshot = state.controller.clock_snapshot(state.id)?;
            let dispersion = snapshot.root_dispersion(state.clock.now()?).as_seconds();
            if ![
                offset.value,
                offset.variance,
                frequency.value,
                frequency.variance,
                dispersion,
            ]
            .iter()
            .all(|v| v.is_finite())
                || offset.variance < 0.0
                || frequency.variance < 0.0
                || dispersion < 0.0
            {
                return Err(AlgoError::InternalError.into());
            }
            let mut active_sources = 0;
            for (index, source) in state.sources.iter().enumerate() {
                if let Some(source) = source {
                    if source.active()? {
                        active_sources |= 1 << index;
                    }
                }
            }
            output.write(Estimate {
                offset_seconds: offset.value,
                offset_variance: offset.variance,
                frequency_ratio: frequency.value,
                frequency_variance: frequency.variance,
                dispersion_seconds: dispersion,
                active_sources,
            });
            Ok(())
        })
    }
}

#[cfg(not(any(test, feature = "std")))]
mod fatal {
    use core::fmt::{self, Write};
    unsafe extern "C" {
        fn esphome_time_panic(message: *const u8, length: usize);
        fn abort() -> !;
    }
    struct Message {
        bytes: [u8; 256],
        length: usize,
    }
    impl Write for Message {
        fn write_str(&mut self, value: &str) -> fmt::Result {
            let length = value.len().min(self.bytes.len() - self.length);
            self.bytes[self.length..self.length + length]
                .copy_from_slice(&value.as_bytes()[..length]);
            self.length += length;
            Ok(())
        }
    }
    #[panic_handler]
    fn panic(info: &core::panic::PanicInfo<'_>) -> ! {
        let mut message = Message {
            bytes: [0; 256],
            length: 0,
        };
        let _ = write!(message, "{info}");
        // SAFETY: diagnostic hook receives valid bytes for this call only; abort
        // terminates the process, so no panic can unwind across the C ABI.
        unsafe {
            esphome_time_panic(message.bytes.as_ptr(), message.length);
            abort()
        }
    }
}

#[cfg(test)]
mod tests;
