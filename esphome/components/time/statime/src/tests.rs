use super::*;
use std::alloc::{Layout, alloc, dealloc};

struct TestClock {
    ns: i64,
    frequency: f64,
    fail: bool,
    fail_after_write: bool,
    writes: usize,
    reenter: *mut c_void,
    reentrant_status: u32,
}

unsafe extern "C" fn now(context: *mut c_void, output: *mut i64) -> i32 {
    let clock = unsafe { &mut *context.cast::<TestClock>() };
    if clock.fail {
        return -1;
    }
    unsafe {
        output.write(clock.ns);
    }
    0
}
unsafe extern "C" fn get_frequency(context: *mut c_void, output: *mut f64) -> i32 {
    let clock = unsafe { &mut *context.cast::<TestClock>() };
    unsafe {
        output.write(clock.frequency);
    }
    0
}
unsafe extern "C" fn set_frequency(context: *mut c_void, frequency: f64, output: *mut i64) -> i32 {
    let clock = unsafe { &mut *context.cast::<TestClock>() };
    if clock.fail {
        return -1;
    }
    if !clock.reenter.is_null() {
        clock.reentrant_status = unsafe { esphome_time_update(clock.reenter) };
    }
    clock.frequency = frequency;
    clock.writes += 1;
    if clock.fail_after_write {
        return -1;
    }
    unsafe {
        output.write(clock.ns);
    }
    0
}
unsafe extern "C" fn step(context: *mut c_void, delta: i64, output: *mut i64) -> i32 {
    let clock = unsafe { &mut *context.cast::<TestClock>() };
    let Some(ns) = clock.ns.checked_add(delta).filter(|ns| *ns >= 0) else {
        return -1;
    };
    clock.ns = ns;
    clock.writes += 1;
    unsafe {
        output.write(ns);
    }
    0
}

struct Harness {
    clock: Box<TestClock>,
    storage: *mut c_void,
    layout: Layout,
}

#[test]
fn learned_frequency_improves_holdover_in_both_directions() {
    for drift in [-50e-6, 50e-6] {
        let mut harness = Harness::new(1);
        let mut reference = harness.clock.ns;
        assert_eq!(
            unsafe { esphome_time_set_usable(harness.storage, 0, 1) },
            OK
        );
        // Two days of legacy observations, then six hours without a reference.
        for second in 1..=172800 {
            reference += 1_000_000_000;
            harness.clock.ns += (1e9 * (1.0 + drift + harness.clock.frequency)).round() as i64;
            if second % 900 == 0 {
                assert_eq!(
                    unsafe {
                        esphome_time_observe(
                            harness.storage,
                            0,
                            reference,
                            harness.clock.ns,
                            1_000_000_000,
                        )
                    },
                    OK
                );
            } else {
                assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
            }
        }
        let start_error = harness.clock.ns - reference;
        let learned = harness.clock.frequency;
        assert_eq!(
            unsafe { esphome_time_set_usable(harness.storage, 0, 0) },
            OK
        );
        for _ in 0..21600 {
            reference += 1_000_000_000;
            harness.clock.ns += (1e9 * (1.0 + drift + harness.clock.frequency)).round() as i64;
            assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
        }
        let holdover_error = (harness.clock.ns - reference - start_error).abs() as f64 / 1e9;
        let uncompensated = drift.abs() * 6.0 * 3600.0;
        println!(
            "drift={drift} learned={learned} holdover={holdover_error}s free={uncompensated}s"
        );
        assert!(holdover_error < uncompensated / 2.0);
        assert_eq!(
            unsafe { esphome_time_set_usable(harness.storage, 0, 1) },
            OK
        );
        assert_eq!(
            unsafe {
                esphome_time_observe(
                    harness.storage,
                    0,
                    reference,
                    harness.clock.ns,
                    1_000_000_000,
                )
            },
            OK
        );
    }
}
impl Harness {
    fn new(sources: u32) -> Self {
        let layout =
            Layout::from_size_align(esphome_time_size(), esphome_time_alignment()).unwrap();
        let storage = unsafe { alloc(layout).cast::<c_void>() };
        assert!(!storage.is_null());
        let mut harness = Self {
            clock: Box::new(TestClock {
                ns: 1_700_000_000_000_000_000,
                frequency: 0.0,
                fail: false,
                fail_after_write: false,
                writes: 0,
                reenter: core::ptr::null_mut(),
                reentrant_status: OK,
            }),
            storage,
            layout,
        };
        let callbacks = harness.callbacks();
        assert_eq!(
            unsafe { esphome_time_init(storage, layout.size(), callbacks, sources) },
            OK
        );
        harness
    }
    fn callbacks(&mut self) -> ClockCallbacks {
        ClockCallbacks {
            context: (&mut *self.clock as *mut TestClock).cast(),
            now: Some(now),
            set_frequency: Some(set_frequency),
            get_frequency: Some(get_frequency),
            step: Some(step),
            max_frequency_ratio: 0.001,
        }
    }
}
impl Drop for Harness {
    fn drop(&mut self) {
        assert_eq!(unsafe { esphome_time_destroy(self.storage) }, OK);
        unsafe {
            dealloc(self.storage.cast(), self.layout);
        }
    }
}

#[test]
fn observations_and_periodic_updates_use_the_same_controller() {
    let mut harness = Harness::new(2);
    for source in 0..2 {
        assert_eq!(
            unsafe { esphome_time_set_usable(harness.storage, source, 1) },
            OK
        );
    }
    for _ in 0..20 {
        harness.clock.ns += 1_000_000_000;
        for source in 0..2 {
            assert_eq!(
                unsafe {
                    esphome_time_observe(
                        harness.storage,
                        source,
                        harness.clock.ns,
                        harness.clock.ns,
                        1_000_000,
                    )
                },
                OK
            );
        }
    }
    harness.clock.ns += 1_000_000_000;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
    let mut output = core::mem::MaybeUninit::<Estimate>::uninit();
    assert_eq!(
        unsafe { esphome_time_estimate(harness.storage, output.as_mut_ptr()) },
        OK
    );
    let output = unsafe { output.assume_init() };
    assert_eq!(output.active_sources, 3);
    assert!(output.offset_seconds.abs() < 1e-9);
    assert!(output.frequency_ratio.abs() < 1e-9);
    assert!(output.dispersion_seconds.is_finite());
    assert_eq!(harness.clock.writes, 41);
    assert_eq!(
        unsafe { esphome_time_set_usable(harness.storage, 0, 0) },
        OK
    );
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
    println!(
        "State storage: {} bytes, alignment {}",
        harness.layout.size(),
        harness.layout.align()
    );
}

#[test]
fn bad_arguments_do_not_poison_a_live_instance() {
    let harness = Harness::new(1);
    for (source, reference, local, uncertainty) in [
        (2, 0, 0, 1),
        (1, 0, 0, 1),
        (0, -1, 0, 1),
        (0, 0, -1, 1),
        (0, 0, 0, 0),
        (0, 0, 0, -1),
    ] {
        assert_eq!(
            unsafe { esphome_time_observe(harness.storage, source, reference, local, uncertainty) },
            INVALID_ARGUMENT
        );
    }
    assert_eq!(
        unsafe { esphome_time_set_usable(harness.storage, 0, 2) },
        INVALID_ARGUMENT
    );
    assert_eq!(
        unsafe { esphome_time_set_usable(harness.storage, 1, 1) },
        INVALID_ARGUMENT
    );
    assert_eq!(
        unsafe { esphome_time_estimate(harness.storage, core::ptr::null_mut()) },
        INVALID_ARGUMENT
    );
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
}

#[test]
fn null_and_misaligned_storage_are_rejected() {
    assert_eq!(
        unsafe { esphome_time_update(core::ptr::null_mut()) },
        INVALID_ARGUMENT
    );
    assert_eq!(
        unsafe { esphome_time_destroy(core::ptr::null_mut()) },
        INVALID_ARGUMENT
    );
    let mut bytes = [0u8; 32];
    let address = bytes.as_mut_ptr() as usize;
    let offset = if address.is_multiple_of(esphome_time_alignment()) {
        1
    } else {
        0
    };
    assert_eq!(
        unsafe { esphome_time_update(bytes.as_mut_ptr().add(offset).cast()) },
        INVALID_ARGUMENT
    );
}

#[test]
fn callback_errors_latch_until_explicit_reinitialization() {
    let mut harness = Harness::new(1);
    harness.clock.fail = true;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, CLOCK_ERROR);
    harness.clock.fail = false;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, FAULTED);
    assert_eq!(harness.clock.writes, 0);
    assert_eq!(unsafe { esphome_time_destroy(harness.storage) }, OK);
    let callbacks = harness.callbacks();
    assert_eq!(
        unsafe { esphome_time_init(harness.storage, harness.layout.size(), callbacks, 1) },
        OK
    );
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
}

#[test]
fn backwards_clock_is_reported_without_further_steering() {
    let mut harness = Harness::new(1);
    harness.clock.ns -= 1_000_000_000;
    assert_eq!(
        unsafe { esphome_time_update(harness.storage) },
        NONMONOTONIC
    );
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, FAULTED);
    assert_eq!(harness.clock.writes, 0);
}

#[test]
fn reentrant_callbacks_cannot_borrow_the_controller_twice() {
    let mut harness = Harness::new(1);
    harness.clock.reenter = harness.storage;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, OK);
    assert_eq!(harness.clock.reentrant_status, BUSY);
    assert_eq!(harness.clock.writes, 1);
}

#[test]
fn nonfinite_clock_frequency_is_reported() {
    let mut harness = Harness::new(1);
    harness.clock.frequency = f64::NAN;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, CLOCK_ERROR);
    assert_eq!(harness.clock.writes, 0);
}

#[test]
fn a_partially_applied_clock_operation_cannot_be_retried_silently() {
    let mut harness = Harness::new(1);
    harness.clock.fail_after_write = true;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, CLOCK_ERROR);
    assert_eq!(harness.clock.writes, 1);
    harness.clock.fail_after_write = false;
    assert_eq!(unsafe { esphome_time_update(harness.storage) }, FAULTED);
    assert_eq!(harness.clock.writes, 1);
}

#[test]
fn timestamp_boundaries_preserve_integer_precision() {
    assert_eq!(timestamp(-1), Err(ClockError::InvalidValue));
    for ns in [0, 1, 999_999_999, 1_000_000_000, i64::MAX] {
        let roundtrip = (timestamp(ns).unwrap() - Timestamp::UNIX_EPOCH).as_nanos();
        // Statime uses binary fractions internally, which may truncate <1 ns.
        assert!((i128::from(ns) - roundtrip).abs() <= 1);
    }
}

#[test]
fn initialization_rejects_bad_storage_callbacks_and_capacity() {
    let mut harness = Harness::new(1);
    let callbacks = harness.callbacks();
    let layout = harness.layout;
    let storage = unsafe { alloc(layout).cast::<c_void>() };
    assert!(!storage.is_null());
    assert_eq!(
        unsafe { esphome_time_init(core::ptr::null_mut(), layout.size(), callbacks, 1) },
        INVALID_ARGUMENT
    );
    assert_eq!(
        unsafe { esphome_time_init(storage, layout.size() - 1, callbacks, 1) },
        INVALID_ARGUMENT
    );
    for sources in [0, 3, u32::MAX] {
        assert_eq!(
            unsafe { esphome_time_init(storage, layout.size(), callbacks, sources) },
            INVALID_ARGUMENT
        );
    }
    for maximum in [0.0, -1.0, 1.0, f64::NAN, f64::INFINITY] {
        let mut invalid = callbacks;
        invalid.max_frequency_ratio = maximum;
        assert_eq!(
            unsafe { esphome_time_init(storage, layout.size(), invalid, 1) },
            INVALID_ARGUMENT
        );
    }
    let mut invalid = callbacks;
    invalid.now = None;
    assert_eq!(
        unsafe { esphome_time_init(storage, layout.size(), invalid, 1) },
        INVALID_ARGUMENT
    );
    harness.clock.fail = true;
    assert_eq!(
        unsafe { esphome_time_init(storage, layout.size(), callbacks, 1) },
        CLOCK_ERROR
    );
    // None of the failed calls initialized storage, so it must not be destroyed.
    unsafe {
        dealloc(storage.cast(), layout);
    }
}
