# ESPHome Documentation AI Collaboration Guide

This document provides context for AI models interacting with this project.
Adhering to these guidelines will promote consistency and content quality.

## Project Overview & Purpose

- **Primary Goal:** ESPHome is a system to configure microcontrollers (like ESP32, ESP8266, RP2040, and LibreTiny-based chips)
  using simple yet powerful YAML configuration files. It generates C++ firmware that can be compiled and flashed to
  these devices, allowing users to control them remotely through home automation systems.

  This repo is the source for the primary documentation for users of ESPHome, published on [esphome.io](https://esphome.io).
- **Business Domain:** Internet of Things (IoT), Home Automation.

## Core Technologies & Stack

- **Languages:** TypeScript, MDX (Markdown with JSX), CSS, JavaScript
- **Frameworks & Runtimes:** Astro, Starlight, Node.js
- **Key Libraries/Dependencies:**
  - **Astro:** Static site generator with component-based architecture
  - **Starlight:** Documentation framework built on Astro
  - **Pagefind:** Client-side text searching
  - **KaTeX:** Mathematical equation rendering
  - **remark-github-blockquote-alert:** GitHub-flavored alert boxes

## Branches

- **Current**
  The `current` branch represents the published documentation in sync with the latest ESPHome release.
  PRs may be raised against this where they contain documentation revisions only.
- **Next**
  The `next` branch is where changes are made via PR corresponding to new features in the ESPHome code repo
  (esphome/esphome). When a release is made this branch is merged into current.

## Documentation Style, Workflow & Conventions

[CONTRIBUTING.md](CONTRIBUTING.md) is the authoritative source for documentation style, MDX syntax,
image handling, component usage, git workflow, and the pre-submission checklist. Follow it for any
content change.

For repository structure and local development setup, see the [README](README.md).

@CONTRIBUTING.md

## Guidelines for AI Generated Reviews and PR Summaries

Avoid the use of flowery language and weasel-words that add no useful content. Keep comments concise and technically
accurate - you are not writing a press release.

For example instead of "Created comprehensive documentation with configuration examples and setup instructions"
it is sufficient to say "Created documentation with examples and instructions".
