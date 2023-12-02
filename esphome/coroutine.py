"""
ESPHome's coroutine system.

The Problem: When running the code generationg, components can depend on variables being registered.
For example, an i2c-based sensor would need the i2c bus component to first be declared before the
codegen can emit code using that variable (or otherwise the C++ won't compile).

ESPHome's codegen system solves this by using coroutine-like methods. When a component depends on
a variable, it waits for it to be registered using `await cg.get_variable()`. If the variable
hasn't been registered yet, control will be yielded back to another component until the variable
is registered. This leads to a topological sort, solving the dependency problem.

Importantly, ESPHome only uses the coroutine *syntax*, no actual asyncio event loop is running in
the background. This is so that we can ensure the order of execution is constant for the same
YAML configuration, thus main.cpp only has to be recompiled if the configuration actually changes.

There are two syntaxes for ESPHome coroutines ("old style" vs "new style" coroutines).

"new style" - This is very much like coroutines you might be used to:

```py
async def my_coroutine(config):
    var = await cg.get_variable(config[CONF_ID])
    await some_other_coroutine(xyz)
    return var
```

new style coroutines are `async def` methods that use `await` to await the result of another coroutine,
and can return values using a `return` statement.

"old style" - This was a hack for when ESPHome still had to run on python 2, but is still compatible

```py
@coroutine
def my_coroutine(config):
    var = yield cg.get_variable(config[CONF_ID])
    yield some_other_coroutine(xyz)
    yield var
```

Here everything is combined in `yield` expressions. You await other coroutines using `yield` and
the last `yield` expression defines what is returned.
"""

import collections
import functools
import heapq
import inspect
import logging
import types
from typing import Any, Callable
from collections.abc import Awaitable, Generator, Iterator

_LOGGER = logging.getLogger(__name__)


def coroutine(func: Callable[..., Any]) -> Callable[..., Awaitable[Any]]:
    """Decorator to apply to methods to convert them to ESPHome coroutines."""
    if getattr(func, "_esphome_coroutine", False):
        # If func is already a coroutine, do not re-wrap it (performance)
        return func
    if inspect.isasyncgenfunction(func):
        # Trade-off: In ESPHome, there's not really a use-case for async generators.
        # and during the transition to new-style syntax it will happen that a `yield`
        # is not replaced properly, so don't accept async generators.
        raise ValueError(
            f"Async generator functions are not allowed. "
            f"Please check whether you've replaced all yields with awaits/returns. "
            f"See {func} in {func.__module__}"
        )
    if inspect.iscoroutinefunction(func):
        # A new-style async-def coroutine function, no conversion needed.
        return func

    if inspect.isgeneratorfunction(func):

        @functools.wraps(func)
        def coro(*args, **kwargs):
            gen = func(*args, **kwargs)
            ret = yield from _flatten_generator(gen)
            return ret

    else:
        # A "normal" function with no `yield` statements, convert to generator
        # that includes a yield just so it's also a generator function
        @functools.wraps(func)
        def coro(*args, **kwargs):
            res = func(*args, **kwargs)
            yield
            return res

    # Add coroutine internal python flag so that it can be awaited from new-style coroutines.
    coro = types.coroutine(coro)
    # pylint: disable=protected-access
    coro._esphome_coroutine = True
    return coro


def coroutine_with_priority(priority: float):
    """Decorator to apply to functions to convert them to ESPHome coroutines.

    :param priority: priority with which to schedule the coroutine, higher priorities run first.
    """

    def decorator(func):
        coro = coroutine(func)
        coro.priority = priority
        return coro

    return decorator


def _flatten_generator(gen: Generator[Any, Any, Any]):
    to_send = None
    while True:
        try:
            # Run until next yield expression
            val = gen.send(to_send)
        except StopIteration as e:
            # return statement or end of function

            # From py3.3, return with a value is allowed in generators,
            # and return value is transported in the value field of the exception.
            # If we find a value in the exception, use that as the return value,
            # otherwise use the value from the last yield statement ("old style")
            ret = to_send if e.value is None else e.value
            return ret

        if isinstance(val, collections.abc.Awaitable):
            # yielded object that is awaitable (like `yield some_new_style_method()`)
            # yield from __await__() like actual coroutines would.
            to_send = yield from val.__await__()
        elif inspect.isgenerator(val):
            # Old style, like `yield cg.get_variable()`
            to_send = yield from _flatten_generator(val)
        else:
            # Could be the last expression from this generator, record this as the return value
            to_send = val
            # perform a yield so that expressions like `while some_condition(): yield None`
            # do not run without yielding control back to the top
            yield


class FakeAwaitable:
    """Convert a generator to an awaitable object.

    Needed for internals of `cg.get_variable`. There we can't use @coroutine because
    native coroutines await from types.coroutine() directly without yielding back control to the top
    (likely as a performance enhancement).

    If we instead wrap the generator in this FakeAwaitable, control is yielded back to the top
    (reason unknown).
    """

    def __init__(self, gen: Generator[Any, Any, Any]) -> None:
        self._gen = gen

    def __await__(self):
        ret = yield from self._gen
        return ret


@functools.total_ordering
class _Task:
    def __init__(
        self,
        priority: float,
        id_number: int,
        iterator: Iterator[None],
        original_function: Any,
    ):
        self.priority = priority
        self.id_number = id_number
        self.iterator = iterator
        self.original_function = original_function

    def with_priority(self, priority: float) -> "_Task":
        return _Task(priority, self.id_number, self.iterator, self.original_function)

    @property
    def _cmp_tuple(self) -> tuple[float, int]:
        return (-self.priority, self.id_number)

    def __eq__(self, other):
        return self._cmp_tuple == other._cmp_tuple

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        return self._cmp_tuple < other._cmp_tuple


class FakeEventLoop:
    """Emulate an asyncio EventLoop to run some registered coroutine jobs in sequence."""

    def __init__(self):
        self._pending_tasks: list[_Task] = []
        self._task_counter = 0

    def add_job(self, func, *args, **kwargs):
        """Add a job to the task queue,

        Optionally retrieves priority from the function object, and schedules according to that.
        """
        if inspect.iscoroutine(func):
            raise ValueError("Can only add coroutine functions, not coroutine objects")
        if inspect.iscoroutinefunction(func):
            coro = func
            gen = coro(*args, **kwargs).__await__()
        else:
            coro = coroutine(func)
            gen = coro(*args, **kwargs)
        prio = getattr(coro, "priority", 0.0)
        task = _Task(prio, self._task_counter, gen, func)
        self._task_counter += 1
        heapq.heappush(self._pending_tasks, task)

    def flush_tasks(self):
        """Run until all tasks have been completed.

        :raises RuntimeError: if a deadlock is detected.
        """
        i = 0
        while self._pending_tasks:
            i += 1
            if i > 1000000:
                # Detect deadlock/circular dependency by measuring how many times tasks have been
                # executed. On the big tests/test1.yaml we only get to a fraction of this, so
                # this shouldn't be a problem.
                raise RuntimeError(
                    "Circular dependency detected! "
                    "Please run with -v option to see what functions failed to "
                    "complete."
                )

            task: _Task = heapq.heappop(self._pending_tasks)
            _LOGGER.debug(
                "Running %s in %s (num %s)",
                task.original_function.__qualname__,
                task.original_function.__module__,
                task.id_number,
            )

            try:
                next(task.iterator)
                # Decrease priority over time, so that if this task is blocked
                # due to a dependency others will clear the dependency
                # This could be improved with a less naive approach
                new_task = task.with_priority(task.priority - 1)
                heapq.heappush(self._pending_tasks, new_task)
            except StopIteration:
                _LOGGER.debug(" -> finished")
