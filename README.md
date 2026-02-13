# ESPHome-Docs

[![Netlify Status][netlify-badge]][netlify-link]
[![Discord Chat][discord-badge]][discord-link]
[![GitHub release][github-badge]][github-link]

[netlify-badge]: https://api.netlify.com/api/v1/badges/97a2e9ce-cee7-4cc8-8dc7-537c92a23fa7/deploy-status
[netlify-link]: https://app.netlify.com/sites/esphome/deploys
[discord-badge]: https://img.shields.io/discord/429907082951524364.svg
[discord-link]: https://discord.gg/KhAMKrd
[github-badge]: https://img.shields.io/github/release/esphome/esphome.svg
[github-link]: https://github.com/esphome/esphome/releases

<a href="https://esphome.io/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://media.esphome.io/logo/logo-text-on-dark.svg">
    <img src="https://media.esphome.io/logo/logo-text-on-light.svg" alt="ESPHome Logo">
  </picture>
</a>

This repository contains the source for the documentation site for ESPHome, built with [Astro](https://astro.build/) and [Starlight](https://starlight.astro.build/).

## Project Structure

The project follows the Astro/Starlight directory structure:

```text
esphome-docs/
├── src/
│   ├── assets/           # Static assets (logos, etc.)
│   ├── components/       # Astro components
│   ├── content/
│   │   └── docs/         # MDX documentation files
│   ├── lib/              # Utility functions
│   └── styles/           # CSS files
├── public/
│   └── images/           # Shared images (multi-use, ImgTable)
├── astro.config.mjs      # Astro configuration
├── tsconfig.json         # TypeScript configuration
└── package.json          # Node.js dependencies
```

## Image File Resolution

Images in the Astro/Starlight site are handled in two ways:

### Imported Local Images (Single-use images)

For images used in only one document:

```jsx
import { Image } from 'astro:assets';
import myImageImg from './images/my-image.jpg';

<Image src={myImageImg} alt="Description" layout="constrained" />
```

- Images are stored in a local `images/` directory next to the MDX file
- They are imported at the top of the file
- Astro automatically optimizes these images during build
- Variable names follow camelCase convention with `Img` suffix

### Absolute Paths (Multi-use images and ImgTable)

For images used in multiple documents or in ImgTable components:

```jsx
<Image src="/images/shared-image.jpg" alt="Description" layout="constrained" />
```

- Images are stored in `/public/images/`
- Referenced using absolute paths starting with `/images/`
- **Important**: All images used in ImgTable components MUST remain in `/public/images/`

### ImgTable Component

The ImgTable component creates grids of component cards (commonly used on index pages):

```jsx
<ImgTable items={[
  ["Title", "/path/to/page/", "image.png"],
  ["Title 2", "/path/to/page2/", "image2.png", "caption"],
  ["Title 3", "/path/to/page3/", "image3.png", "caption", "dark-invert"],
]} />
```

All images referenced in ImgTable **must** be in `/public/images/` as the component resolves them to `/images/filename.png`.

## Starlight Framework

The site uses [Starlight](https://starlight.astro.build/), a documentation framework built on Astro. Key features include:

- Built-in responsive design
- Automatic dark mode support
- Search functionality via Pagefind
- Sidebar navigation
- Right-hand table of contents
- SEO optimization
- Accessibility features

## MDX Format

Content is written in [MDX](https://mdxjs.com/), which allows you to use JSX components within Markdown:

```mdx
---
title: "My Page Title"
description: "Page description for SEO"
---

import { Image } from 'astro:assets';
import myImageImg from './images/my-image.jpg';

# Heading

Regular Markdown content here.

<Image src={myImageImg} alt="Description" layout="constrained" />
```

## Custom Components

### Astro Components

Custom components are located in `src/components/` and can be imported into MDX files:

- **APIRef**: Links to C++ API documentation
- **ImgTable**: Grid of component cards with images
- **Figure**: Images with optional captions
- **Footer**: Custom footer component

### Using Components

Import and use components in MDX files:

```jsx
import APIRef from '@components/APIRef.astro';
import Figure from '@components/Figure.astro';
import myImageImg from './images/my-image.jpg';

<APIRef text="component.h" path="component/component.h" />

<Figure src={myImageImg} alt="Description" caption="Optional caption" />
```

## Alert Boxes

Use GitHub-flavored alert syntax for callouts:

```markdown
> [!NOTE]
> This is important information that readers should pay attention to.

> [!IMPORTANT]
> This is helpful information that readers should be aware of.

> [!TIP]
> Helpful advice or best practices.

> [!WARNING]
> Important warnings or potential issues.

> [!CAUTION]
> Critical cautions that could cause damage.
```

## Mathematical Expressions

LaTeX equations are supported using KaTeX:

Inline math: `$E = mc^2$`

Block equations:

```markdown
$$
\text{formula} = \frac{a}{b}
$$
```

## Development

To run the site locally:

1. Install Node.js (v18 or later recommended)
1. Clone this repository
1. Install dependencies: `npm install`
1. Run development server: `npm run dev`
1. Open your browser to `http://localhost:4321/`

### Available Commands

```bash
npm run dev          # Start development server
npm run build        # Build for production
npm run preview      # Preview production build locally
npm run astro        # Run Astro CLI commands
```

## Building for Production

```bash
npm run build
```

The built site will be in the `dist/` directory.

See the GitHub workflows in `.github/workflows` for CI/CD configuration.

## Contributing

Contributions to improve the documentation are welcome! Please follow these steps:

1. Fork the repository
1. Create a new branch for your changes
1. Make your changes following the conventions described above
1. Test your changes locally with `npm run dev`
1. Submit a pull request

### Image Guidelines

- Use local imported images for single-use images (one file only)
- Keep multi-use images in `/public/images/` with absolute paths
- All ImgTable images must remain in `/public/images/`
- Use descriptive alt text for accessibility
- Include `layout="constrained"` for responsive images

## License

The ESPHome documentation is licensed under the
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][by-ns-sa].

**Documentation:** <https://esphome.io/>

For issues, please go to [the issue tracker](https://github.com/esphome/esphome/issues).

For feature requests, please see [feature requests](https://github.com/orgs/esphome/discussions).

[by-ns-sa]: https://creativecommons.org/licenses/by-nc-sa/4.0/
