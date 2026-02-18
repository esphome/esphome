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

## Architectural Patterns

See the README.md file for detailed information about:

- Project structure (Astro/Starlight conventions)
- Image handling (local imports vs. absolute paths)
- MDX format and component usage
- Custom Astro components

## Content Format

- **File Format:** MDX (`.mdx` files in `src/content/docs/`)
- **Frontmatter:** Required YAML frontmatter with `title` and `description`
- **Images:**
  - Single-use images: Import locally from `./images/` directory
  - Multi-use images: Use absolute paths from `/public/images/`
  - ImgTable images: Must be in `/public/images/`
- **Components:** Import Astro components using `@components/` alias

## Branches

- **Current**
  The `current` branch represents the published documentation in sync with the latest ESPHome release.
  PRs may be raised against this where they contain documentation revisions only.
- **Next**
  The `next` branch is where changes are made via PR corresponding to new features in the ESPHome code repo
  (esphome/esphome). When a release is made this branch is merged into current.

## Development & Testing Workflow

See the README.md file

Quick start:

1. Install dependencies: `npm install`
1. Run dev server: `npm run dev`
1. View at: `http://localhost:4321/`

## Contribution Workflow (Pull Request Process)

1. **Fork:** Fork the repository.
1. **Branch:** Create a new branch in your fork from the appropriate base branch (`current` or `next`.)
1. **Make Changes:** Adhere to all coding conventions and patterns.
1. **Test:** Use `npm run dev` to run the development server for instant preview.
1. **Commit:** Commit your changes. There is no strict format for commit messages.
1. **Pull Request:** Submit a PR against the base branch. The Pull Request title should have a prefix of the component being worked on (e.g., `[display] Add new examples`, `[abc123] Add new component`). Pull requests should always be made with the PULL_REQUEST_TEMPLATE.md template filled out correctly.

## MDX Writing Guidelines

### Images

**Single-use images (used in one file only):**

```mdx
import { Image } from 'astro:assets';
import myImageImg from './images/my-image.jpg';

<Image src={myImageImg} alt="Description" layout="constrained" />
```

**Multi-use images (used in multiple files):**

```mdx
<Image src="/images/shared-image.jpg" alt="Description" layout="constrained" />
```

**Important:** All images used in ImgTable components MUST be in `/public/images/` with absolute paths.

### Alert Boxes

Use GitHub-flavored alert syntax:

```markdown
> [!NOTE]
> Important information

> [!WARNING]
> Warning message

> [!TIP]
> Helpful tip
```

### Components

Import custom components:

```mdx
import APIRef from '@components/APIRef.astro';
import Figure from '@components/Figure.astro';
import myImageImg from './images/my-image.jpg';

<APIRef text="component.h" path="component/component.h" />

<Figure src={myImageImg} alt="Description" caption="Optional caption" />
```

### Mathematical Expressions

Use LaTeX syntax with KaTeX:

```markdown
Inline: $E = mc^2$

Block:
$$
\text{formula} = \frac{a}{b}
$$
```

## Guidelines for AI Generated Reviews and PR Summaries

Avoid the use of flowery language and weasel-words that add no useful content. Keep comments concise and technically
accurate - you are not writing a press release.

For example instead of "Created comprehensive documentation with configuration examples and setup instructions"
it is sufficient to say "Created documentation with examples and instructions".
