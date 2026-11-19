---
name: pr-workflow
description: Create pull requests for esphome. Use when creating PRs, submitting changes, or preparing contributions.
allowed-tools: Read, Bash, Glob, Grep
---

# ESPHome PR Workflow

When creating a pull request for esphome, follow these steps:

## 1. Create Branch from Upstream

Always base your branch on **upstream** (not origin/fork) to ensure you have the latest code:

```bash
git fetch upstream
git checkout -b <branch-name> upstream/dev
```

## 2. Read the PR Template

Before creating a PR, read `.github/PULL_REQUEST_TEMPLATE.md` to understand required fields.

## 3. Create the PR

Use `gh pr create` with the **full template** filled in. Never skip or abbreviate sections.

Required fields:
- **What does this implement/fix?**: Brief description of changes
- **Types of changes**: Check ONE appropriate box (Bugfix, New feature, Breaking change, etc.)
- **Related issue**: Use `fixes <link>` syntax if applicable
- **Pull request in esphome.io**: Link if docs are needed
- **Test Environment**: Check platforms you tested on
- **Example config.yaml**: Include working example YAML
- **Checklist**: Verify code is tested and tests added

## 4. Example PR Body

```markdown
# What does this implement/fix?

<describe your changes here>

## Types of changes

- [ ] Bugfix (non-breaking change which fixes an issue)
- [x] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Developer breaking change (an API change that could break external components)
- [ ] Code quality improvements to existing code or addition of tests
- [ ] Other

**Related issue or feature (if applicable):**

- fixes https://github.com/esphome/esphome/issues/XXX

**Pull request in [esphome.io](https://github.com/esphome/esphome.io) with documentation (if applicable):**

- esphome/esphome.io#XXX

## Test Environment

- [x] ESP32
- [x] ESP32 IDF
- [ ] ESP8266
- [ ] RP2040
- [ ] BK72xx
- [ ] RTL87xx
- [ ] LN882x
- [ ] nRF52840

## Example entry for `config.yaml`:

```yaml
# Example config.yaml
component_name:
  id: my_component
  option: value
```

## Checklist:
  - [x] The code change is tested and works locally.
  - [x] Tests have been added to verify that the new code works (under `tests/` folder).

If user exposed functionality or configuration variables are added/changed:
  - [ ] Documentation added/updated in [esphome.io](https://github.com/esphome/esphome.io).
```

## 5. Push and Create PR

```bash
git push -u origin <branch-name>
gh pr create --repo esphome/esphome --base dev --title "[component] Brief description"
```

Title should be prefixed with the component name in brackets, e.g. `[safe_mode] Add feature`.

## 6. Fixes for a Release Branch

A fix that should ship in a beta or patch release still targets `dev`. Give it the milestone of that release
(`gh pr create --milestone <version>`). The release manager cherry-picks each PR's squash-merge commit onto `beta` (beta
releases) or `release` (patch releases), so test that same change in a throwaway worktree:

```bash
git worktree add --detach <scratch-dir> upstream/release   # or upstream/beta
# Before the PR merges: the PR's whole change, as it will be squashed
git diff $(git merge-base upstream/dev HEAD) HEAD | git -C <scratch-dir> apply --3way
# After it merges: the squash commit itself
git -C <scratch-dir> cherry-pick --no-commit <merge commit>
```

If it conflicts, resolve it there, commit it as one commit, push it to a branch of your fork, and declare it in the PR
body as step 7 of the contribution workflow in `AGENTS.md` describes (a `release-cherry-pick` block with `branch` and
`commit`). The release manager compares that commit with the PR's squash-merge commit before using it, so make a new one
and update the block if the PR changes afterwards. Keep the branch until the release is out.
