---
name: silent-failure-review
description: Find code paths in a branch's diff that fail without telling the user - early returns, swallowed errors, ignored return values, half-done cleanup, and checks that are only safe under an unstated assumption. Use when asked for a silent failure review or before pushing a PR for review.
---

# Silent failure review

Report only. Do not edit files.

## Scope

The diff of the checked-out branch against its base (`git diff <base>...HEAD`, base `dev` unless the user
names another). Read each changed function in full, and its callers, not just the hunks.

## What to look for

1. **Early returns and skips:** a `return` or `continue` taken before a check or a fetch. Name the assumption
   it relies on and the case that breaks it (a later release, a lost file, an older install).
2. **Swallowed errors:** `except` that logs, passes or returns a default; `OSError` tolerated where the next
   step needs the file.
3. **Ignored results:** a `False`, `None` or exit code that the caller does not check.
4. **Half-done state:** a failure between two writes that leaves a marker, stamp, lock or filter saying
   something is done when it is not; cleanup missing on an error path.
5. **Stale on-disk state:** markers or stamps written by an older version that the new code trusts.
6. **Unclear late failure:** an error that surfaces later (CMake, compile, boot) instead of where it starts,
   with a message that does not point back to the cause.
7. **Defaults standing in for errors:** `x or default`, `.get(key, default)` or an empty object takes the
   place of a value that is missing or could not be read, and the code carries on as if it were real.
8. **Unwatched work:** a thread, task or subprocess is started and nothing ever looks at how it ended.
9. **Wrong value on an error branch:** the error is detected, but the branch returns success, `None` or the
   wrong code instead of passing it on.

Moved code counts too: if the original had the same flaw, say so, but still report it.

## Skip

- Code that ignores a failure and has a comment explaining why.
- Tests that provoke an error and check for it.
- Optional tidy-up steps (removing a temp file, say) that may fail with only a log line.

## Report

One entry per finding, most severe first:

- **HIGH / MEDIUM / LOW** - one-line title, `file:line`
- **Risk:** the concrete input or state, and what the user sees.
- **Fix:** the smallest change.
- **Test:** the test that fails before the fix.

No findings: say so in one line. Keep the report short; no summary of what the PR does.
