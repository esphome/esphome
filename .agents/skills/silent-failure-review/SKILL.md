---
name: silent-failure-review
description: Find code paths in a branch's diff that fail without telling the user - early returns, swallowed errors, ignored return values, half-done cleanup, and checks that are only safe under an unstated assumption. Use when asked for a silent failure review or before pushing a PR for review.
---

# Silent failure review

Report only. Do not edit files.

## Scope

The diff of the checked-out branch against its base. Fetch the base first (`git fetch upstream dev`, or
`git fetch origin dev` when there is no `upstream` remote); a stale base pulls in commits the branch only merged.
Then run `git merge-base upstream/dev HEAD` (or `origin/dev`) on its own. If it fails or prints nothing (often a
shallow clone; `git fetch --unshallow` fixes that), say the review could not run and stop; never report "no
findings" then. Diff against that commit with `git diff <base>`. The diff includes uncommitted changes but not
untracked files, so also read in full every file `git status --porcelain` lists as `??`.
Use another branch only if the user names one. If neither remote exists, say so and stop. Read each changed
function in full, and its callers, not just the hunks. When a failure depends on how a library call behaves,
read that library's code too.

## What to look for

1. **Early returns and skips:** a `return` or `continue` taken before a check or a fetch. Name the assumption
   it relies on and the case that breaks it (a later release, a lost file, an older install).
2. **Swallowed errors:** `except` that logs, passes or returns a default; `OSError` tolerated where the next
   step needs the file. In C++: an error code mapped to a default, or a status flag that is never set.
3. **Ignored results:** a `False`, `None`, exit code, `bool` or `ErrorCode` that the caller does not check.
4. **Half-done state:** a failure between two writes that leaves a marker, stamp, lock or filter saying
   something is done when it is not; cleanup missing on an error path.
5. **Stale stored state:** markers, stamps or saved preferences (NVS, flash) written by an older version that
   the new code trusts.
6. **Unclear late failure:** an error that surfaces later (CMake, compile, boot) instead of where it starts,
   with a message that does not point back to the cause.
7. **Defaults standing in for errors:** `x or default`, `.get(key, default)` or an empty object takes the
   place of a value that is missing or could not be read, and the code carries on as if it were real.
8. **Unwatched work:** a thread, task or subprocess is started and nothing ever looks at how it ended.
9. **Wrong value on an error branch:** the error is detected, but the branch returns success, `None` or the
   wrong code instead of passing it on.
10. **State reset or skipped:** for each counter, flag, retry or timeout the diff adds or changes, find every
    place that writes it, and check that no path resets or skips it before it can do its job.
11. **Boundaries:** length and buffer comparisons (`>=` against `>`, `len` against `len + 1`), the exact-fit
    case and the empty case.
12. **Unjudged results:** a test or script that prints a result but never fails on it.

Moved code counts too: if the original had the same flaw, say so, but still report it. Report only flaws in
changed or moved lines; unchanged callers are context. If the branch has an open PR (`gh pr view --json number`
finds it), read its reviews (`gh pr view <n> --comments` and
`gh api --paginate repos/esphome/esphome/pulls/<n>/comments`) and skip what they already raised.

Before reporting a candidate, follow it to the line that proves it and quote that line. If you cannot
confirm it, drop it or mark it as likely.

## Skip

- Code that ignores a failure and has a comment explaining why.
- Tests that provoke an error and check for it.
- Optional tidy-up steps (removing a temp file, say) that may fail with only a log line.

## Report

One entry per finding, most severe first:

- **HIGH / MEDIUM / LOW** - one-line title, `file:line`. HIGH: wrong result or lost data. MEDIUM: a
  misleading or delayed failure. LOW: affects only logs or diagnostics.
- **Confidence:** confirmed, or likely and why it could not be confirmed.
- **Risk:** the concrete input or state, and what the user sees.
- **Fix:** the smallest change.
- **Test:** the test that fails before the fix, or for on-device code, the steps to reproduce it on a board.

No findings: say so in one line. Keep the report short; no summary of what the PR does.
