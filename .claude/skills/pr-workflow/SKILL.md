---
name: pr-workflow
description: Create pull requests for esphome-docs. Use when creating PRs, submitting changes, or preparing contributions.
allowed-tools: Read, Bash, Glob, Grep
---

# ESPHome Docs PR Workflow

When creating a pull request for esphome-docs, follow these steps:

## 1. Create Branch from Upstream

Always base your branch on **upstream** (not origin/fork) to ensure you have the latest code:

```bash
git fetch upstream
git checkout -b <branch-name> upstream/current
```

Use `upstream/current` for documentation fixes, or `upstream/next` for new component docs.

## 2. Read the PR Template

Before creating a PR, read `.github/PULL_REQUEST_TEMPLATE.md` to understand required fields.

## 3. Create the PR

Use `gh pr create` with the **full template** filled in. Never skip or abbreviate sections.

Required fields:
- **Description**: What changes are being made
- **Related issue**: Use `fixes <link>` syntax if applicable
- **Pull request in esphome**: Link if this documents a new feature
- **Checklist**: Check the appropriate boxes:
  - `next` branch: New docs with matching esphome PR
  - `current` branch: Fixes/adjustments to existing docs
  - Component index: Check if link added to `/components/_index.md`

## 4. Example PR Body

```markdown
## Description:

<describe your changes here>

**Related issue (if applicable):** fixes https://github.com/esphome/esphome-docs/issues/XXX

**Pull request in [esphome](https://github.com/esphome/esphome) with YAML changes (if applicable):**

- N/A (or esphome/esphome#XXX)

## Checklist:

  - [ ] I am merging into `next` because this is new documentation that has a matching pull-request in [esphome](https://github.com/esphome/esphome) as linked above.
    or
  - [x] I am merging into `current` because this is a fix, change and/or adjustment in the current documentation and is not for a new component or feature.

  - [ ] Link added in `/components/_index.md` when creating new documents for new components or cookbook.
```

## 5. Push and Create PR

```bash
git push -u origin <branch-name>
gh pr create --repo esphome/esphome-docs --base current --title "[component] Brief description"
```

Use `--base next` if documenting a new feature with a matching esphome PR.
