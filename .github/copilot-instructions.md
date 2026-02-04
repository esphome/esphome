# ESPHome Documentation AI Collaboration Guide

This document provides context for AI models interacting with this project.
Adhering to these guidelines will promote consistency and content quality.

## Project Overview & Purpose

*   **Primary Goal:** ESPHome is a system to configure microcontrollers (like ESP32, ESP8266, RP2040, and LibreTiny-based chips)
    using simple yet powerful YAML configuration files. It generates C++ firmware that can be compiled and flashed to
    these devices, allowing users to control them remotely through home automation systems.

    This repo is the source for the primary documentation for users of ESPHome, published on [esphome.io](https://esphome.io).
*   **Business Domain:** Internet of Things (IoT), Home Automation.

## Core Technologies & Stack

*  **Languages:** HTML, CSS, Markdown, Go templates
*  **Frameworks & Runtimes:** Hugo and Pagefind
*  **Key Libraries/Dependencies:**
    * **Hugo:** For building the static site.
    * **Pagefind:** For client-side text searching

## Architectural Patterns

See the README.md file.

## Branches

* **Current**
  The `current` branch represents the published documentation in sync with the latest ESPHome release.
  PRs may be raised against this where they contain documentation revisions only.
* **Next**
  The `next` branch is where changes are made via PR corresponding to new features in the ESPHome code repo
  (esphome/esphome). When a release is made this branch is merged into current.

## Development & Testing Workflow

See the README.md file

## Contribution Workflow (Pull Request Process)
1.  **Fork:** Fork the repository.
1.  **Branch:** Create a new branch in your fork from the appropriate base branch (`current` or `next`.)
1.  **Make Changes:** Adhere to all coding conventions and patterns.
1.  **Test:** Use the `make live-html` command to invoke hugo in server mode for instant preview.
1.  **Commit:** Commit your changes. There is no strict format for commit messages.
1.  **Pull Request:** Submit a PR against the base branch. The Pull Request title should have a prefix of the component being worked on (e.g., `[display] Add new examples`, `[abc123] Add new component`). Pull requests should always be made with the PULL_REQUEST_TEMPLATE.md template filled out correctly.

## Guidelines for AI generated reviews and PR summaries

Avoid the use of flowery language and weasel-words that add no useful content; Keep comments concise and technically
accurate - you are not writing a press release.
For example instead of "Created comprehensive documentation with configuration examples and setup instructions"
it is sufficient to say "Created documentation with examples and instructions".

## Legacy Hugo Shortcodes

The following Hugo shortcodes are legacy and should be replaced when encountered:
* `{{< docref >}}`: use standard Markdown links instead
* `{{< img >}}`: use standard Markdown image syntax instead
