#!/usr/bin/env node

import { readFile, stat, readdir } from 'fs/promises';
import { join, relative, basename, dirname, extname } from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// ANSI color codes
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  dim: '\x1b[2m',
  red: '\x1b[31m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  cyan: '\x1b[36m',
  boldRed: '\x1b[1;31m',
  boldGreen: '\x1b[1;32m',
};

// Folders to ignore
const ignoreFolders = [
  'pagefind/',
  'node_modules/',
  'dist/',
  '.astro/',
];

// File types
const fileTypes = [
  '.cfg', '.css', '.gif', '.h', '.html', '.ico', '.jpg', '.js', '.json',
  '.md', '.mdx', '.png', '.py', '.svg', '.toml', '.txt', '.webmanifest',
  '.xml', '.yaml', '.yml', '.mjs', '.ts', '.tsx', '.astro', '.sh', '.webp',
  '' // empty string for files without extension (like .gitignore)
];
const imageTypes = ['.webp', '.jpg', '.ico', '.png', '.svg', '.gif'];

// Store errors
const errors = new Map();

function addError(fname, lineno, col, msg) {
  if (!errors.has(fname)) {
    errors.set(fname, []);
  }
  errors.get(fname).push({ lineno, col, msg });
}

// Get files from git
function getGitFiles() {
  try {
    const output = execSync('git ls-files -s', { encoding: 'utf-8' });
    const files = new Map();

    for (const line of output.trim().split('\n')) {
      const parts = line.split(/\s+/);
      if (parts.length >= 4) {
        const mode = parseInt(parts[0]);
        const path = parts.slice(3).join(' ');
        files.set(path, mode);
      }
    }

    return files;
  } catch (error) {
    console.error('Error getting git files:', error.message);
    return new Map();
  }
}

// Find all occurrences of a substring
function* findAll(content, substring) {
  const lines = content.split('\n');
  for (let i = 0; i < lines.length; i++) {
    let col = 0;
    while (true) {
      col = lines[i].indexOf(substring, col);
      if (col === -1) break;
      yield { line: i + 1, col: col + 1 };
      col += substring.length;
    }
  }
}

// File checks
async function checkFileExtension(fname) {
  const ext = extname(fname);
  if (!fileTypes.includes(ext)) {
    addError(fname, 1, 1,
      'This file extension is not a registered file type. If this is an error, please update lint.mjs.');
  }
}

async function checkExecutableBit(fname, gitMode) {
  const exclude = ['script/', '.devcontainer/', 'lint.py', 'lint.mjs'];
  if (exclude.some(ex => fname.startsWith(ex) || fname === ex)) {
    return;
  }

  if (gitMode !== 100644) {
    addError(fname, 1, 1,
      `File has invalid executable bit ${gitMode}. If running from a windows machine please see disabling executable bit in git.`);
  }
}

async function checkImageSize(fname, fileStat) {
  const ext = extname(fname);
  if (!imageTypes.includes(ext)) return;

  if (fileStat.size > 1024 * 1024) {
    const sizeKb = Math.floor(fileStat.size / 1024);
    addError(fname, 1, 1,
      `Image is too large. Images should be 1MB max. Use https://compress-or-die.com/ to reduce size. Current size: ${sizeKb}kb`);
  }
}

// Content checks
function checkTabs(fname, content) {
  if (fname === 'Makefile') return;

  for (const { line, col } of findAll(content, '\t')) {
    addError(fname, line, col, 'File contains tab character. Please convert tabs to spaces.');
    return; // Only report first occurrence
  }
}

function checkNewlines(fname, content) {
  for (const { line, col } of findAll(content, '\r')) {
    addError(fname, line, col, 'File contains Windows newline. Please set your editor to Unix newline mode.');
    return; // Only report first occurrence
  }
}

function checkEndNewline(fname, content) {
  const exclude = ['.svg', 'runtime.txt'];
  if (exclude.some(ex => fname.endsWith(ex)) || fname.includes('_static/')) {
    return;
  }

  if (content && !content.endsWith('\n')) {
    addError(fname, 1, 1, 'File does not end with a newline, please add an empty line at the end of the file.');
  }
}

function checkEsphomeLinks(fname, content) {
  if (!fname.endsWith('.md') && !fname.endsWith('.mdx')) return;

  const linkPattern = /\[([^\]]+)\]\((https:\/\/esphome\.io\/[^)]+)\)/g;
  let match;

  while ((match = linkPattern.exec(content)) !== null) {
    const lineno = content.substring(0, match.index).split('\n').length;
    const col = match.index - content.lastIndexOf('\n', match.index);
    const linkText = match[1];
    const fullUrl = match[2];

    addError(fname, lineno, col,
      `Markdown link to esphome.io should use relative path. Change [${linkText}](${fullUrl}) to use a relative URL`);
  }
}

// Build anchor cache for link validation
async function buildAnchorCache() {
  const cache = new Map();
  const contentDir = join(__dirname, 'src/content/docs');

  try {
    await stat(contentDir);
  } catch {
    return cache;
  }

  function generateSlug(text) {
    return text
      .toLowerCase()
      .replace(/[^\w\s-]/g, '')
      .replace(/[-\s]+/g, '-')
      .replace(/^-+|-+$/g, '');
  }

  async function processDir(dir, basePath = '') {
    const entries = await readdir(dir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      const relPath = basePath ? join(basePath, entry.name) : entry.name;

      if (entry.isDirectory()) {
        await processDir(fullPath, relPath);
      } else if (entry.name.endsWith('.md') || entry.name.endsWith('.mdx')) {
        let pagePath;
        if (entry.name === 'index.md' || entry.name === 'index.mdx') {
          pagePath = basePath || '';
        } else {
          pagePath = relPath.replace(/\.mdx?$/, '');
        }

        pagePath = pagePath.replace(/\\/g, '/');

        const content = await readFile(fullPath, 'utf-8');
        const anchors = new Set();

        for (const line of content.split('\n')) {
          // Match headings
          const headingMatch = line.match(/^(#{1,6})\s+(.*)/);
          if (headingMatch) {
            const headingText = headingMatch[2].trim();
            const anchorId = generateSlug(headingText);
            anchors.add(anchorId);
          }

          // Match anchor shortcodes (old Hugo style)
          const shortcodeMatch = line.match(/\{\{<\s*anchor\s+"([^\s>]+)"\s*>}}/);
          if (shortcodeMatch) {
            anchors.add(shortcodeMatch[1]);
          }

          // Match HTML anchor spans
          const spanMatch = line.match(/<span id="([^"]+)"><\/span>/);
          if (spanMatch) {
            anchors.add(spanMatch[1]);
          }
        }

        cache.set(pagePath, anchors);
      }
    }
  }

  await processDir(contentDir);
  return cache;
}

async function checkInternalLinks(fname, content, anchorCache) {
  if (!fname.endsWith('.md') && !fname.endsWith('.mdx')) return;

  // Skip template and documentation files with example links
  const ignoreFiles = [
    '.claude/instructions.md',
    'script/release_notes_template.md',
  ];
  if (ignoreFiles.includes(fname)) return;

  const linkPattern = /\[([^\]]+)\]\(([^)]+)\)/g;
  let match;

  while ((match = linkPattern.exec(content)) !== null) {
    const linkText = match[1];
    const linkUrl = match[2];
    const lineno = content.substring(0, match.index).split('\n').length;
    const col = match.index - content.lastIndexOf('\n', match.index);

    // Skip external links
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(linkUrl)) {
      continue;
    }

    // Check anchor-only links
    if (linkUrl.startsWith('#')) {
      const anchorId = linkUrl.substring(1);

      // Get current page path
      let currentPage;
      try {
        if (fname.includes('src/content/docs/')) {
          const relPath = fname.split('src/content/docs/')[1];
          if (basename(relPath) === 'index.md' || basename(relPath) === 'index.mdx') {
            currentPage = dirname(relPath) === '.' ? '' : dirname(relPath);
          } else {
            currentPage = relPath.replace(/\.mdx?$/, '');
          }
          currentPage = currentPage.replace(/\\/g, '/');

          if (anchorCache.has(currentPage)) {
            if (!anchorCache.get(currentPage).has(anchorId)) {
              addError(fname, lineno, col,
                `Anchor-only link '#${anchorId}' not found on current page. If this should link to another page, use the full path like [${linkText}](/path/to/page#${anchorId})`);
            }
          }
        }
      } catch (e) {
        // Skip if not in content directory
      }
      continue;
    }

    // Skip relative links to static assets
    if (!linkUrl.startsWith('/') && /\.(png|jpg|jpeg|gif|svg|webp|pdf|zip)$/i.test(linkUrl)) {
      continue;
    }

    // Skip absolute links to static assets
    if (linkUrl.startsWith('/images/') && /\.(png|jpg|jpeg|gif|svg|webp|pdf|zip)$/i.test(linkUrl)) {
      continue;
    }

    // Skip links with spaces or parentheses (likely code)
    if (linkUrl.includes(' ') || linkUrl.includes('(') || linkUrl.includes(')')) {
      continue;
    }

    // Skip relative links without leading slash
    if (!linkUrl.startsWith('/')) {
      continue;
    }

    // Parse internal link
    let pathPart, anchorPart;
    if (linkUrl.includes('#')) {
      [pathPart, anchorPart] = linkUrl.split('#', 2);
    } else {
      pathPart = linkUrl;
      anchorPart = null;
    }

    // Skip paths with query strings or .html
    if (pathPart.includes('?') || pathPart.includes('.html')) {
      continue;
    }

    // Remove leading slash and trailing slash
    const cleanPath = pathPart.replace(/^\/+/, '').replace(/\/+$/, '');

    // Check if page exists
    // Try both with and without '/index' suffix since both are valid
    const pathsToCheck = [cleanPath];
    if (cleanPath.endsWith('/index')) {
      pathsToCheck.push(cleanPath.replace(/\/index$/, ''));
    } else if (cleanPath !== '') {
      pathsToCheck.push(cleanPath + '/index');
    }

    const pageExists = pathsToCheck.some(p => anchorCache.has(p) || p === '');
    if (!pageExists && cleanPath !== '') {
      addError(fname, lineno, col,
        `Internal link references non-existent page: '${pathPart}' in link [${linkText}](${linkUrl})`);
      continue;
    }

    // Validate anchor if present
    if (anchorPart && cleanPath) {
      // Find the actual page path in cache (might be with or without /index)
      let targetPage = cleanPath;
      if (!anchorCache.has(targetPage)) {
        if (cleanPath.endsWith('/index')) {
          targetPage = cleanPath.replace(/\/index$/, '');
        } else {
          const withIndex = cleanPath + '/index';
          if (anchorCache.has(withIndex)) {
            targetPage = withIndex;
          }
        }
      }

      if (anchorCache.has(targetPage)) {
        if (!anchorCache.get(targetPage).has(anchorPart)) {
          addError(fname, lineno, col,
            `Internal link references non-existent anchor: '#${anchorPart}' on page '${pathPart}' in link [${linkText}](${linkUrl})`);
        }
      }
    }
  }
}

function checkAutomationHeadings(fname, content) {
  if (!fname.startsWith('src/content/docs/components/')) return;
  if (!fname.endsWith('.md') && !fname.endsWith('.mdx')) return;

  // Skip the main actions page which documents core actions (delay, if, lambda, etc.)
  if (fname.includes('automations/')) return;

  const lines = content.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineno = i + 1;

    // Only look at heading lines
    if (!line.match(/^#{2,4}\s/)) continue;

    // Check 1: Backticked `domain.name` without Action/Condition/Trigger suffix
    // e.g. ### `wireguard.enabled`
    if (line.match(/^#{2,4}\s+`[a-z_][a-z0-9_.]*\.[a-z_][a-z0-9_]*`\s*$/)) {
      addError(fname, lineno, 1,
        'Heading has backticked automation name but is missing Action/Condition/Trigger suffix. ' +
        'Add " Action", " Condition", or " Trigger" after the closing backtick.');
    }

    // Check 2: Action/Condition with backticked name missing domain prefix (no dot)
    // e.g. ### `arm_away` Action
    const noDotMatch = line.match(/^#{2,4}\s+`([a-z_][a-z0-9_]*)`\s+(?:Action|Condition)s?\s*$/i);
    if (noDotMatch) {
      const name = noDotMatch[1];
      // Exclude core actions/conditions documented on actions.mdx
      const coreNames = [
        'delay', 'if', 'lambda', 'repeat', 'wait_until', 'while',
        'and', 'all', 'or', 'any', 'xor', 'not', 'for',
      ];
      if (!coreNames.includes(name) && !name.startsWith('on_')) {
        addError(fname, lineno, 1,
          `Heading "\`${name}\`" is missing the domain prefix. ` +
          'Use the format: `domain.name` Action/Condition (e.g. `switch.toggle` Action).');
      }
    }

    // Check 3: Bold **Action** or **Condition** suffix
    // e.g. ### `remote_transmitter.transmit_nec` **Action**
    if (line.match(/^#{2,4}\s+`[^`]+`.*\*\*(?:Action|Condition)s?\*\*/)) {
      addError(fname, lineno, 1,
        'Action/Condition suffix should not be bold. Use plain text: " Action" or " Condition".');
    }

    // Check 4: Lowercase action/condition suffix (not matching standard capitalization)
    // e.g. ### `sprinkler.start_full_cycle` action
    if (line.match(/^#{2,4}\s+`[a-z_][a-z0-9_.]*\.[a-z_][a-z0-9_]*`\s+(?:action|condition)s?\s*$/)) {
      addError(fname, lineno, 1,
        'Action/Condition suffix should be capitalized. Use "Action" or "Condition" (not lowercase).');
    }
  }
}

// Main execution
async function main() {
  console.log(`${colors.cyan}Running ESPHome documentation linter...${colors.reset}\n`);

  const gitFiles = getGitFiles();
  const anchorCache = await buildAnchorCache();

  // Process files
  for (const [fname, gitMode] of gitFiles) {
    // Skip ignored folders
    if (ignoreFolders.some(folder => fname.startsWith(folder) || fname.includes(`/${folder}`))) {
      continue;
    }

    try {
      const fileStat = await stat(fname);

      if (!fileStat.isFile()) continue;

      // Run file checks
      await checkFileExtension(fname);
      await checkExecutableBit(fname, gitMode);
      await checkImageSize(fname, fileStat);

      // Skip binary files
      if (imageTypes.includes(extname(fname))) {
        continue;
      }

      // Read and check content
      let content;
      try {
        content = await readFile(fname, 'utf-8');
      } catch (error) {
        if (error.message.includes('invalid')) {
          addError(fname, 1, 1, 'File is not readable as UTF-8. Please set your editor to UTF-8 mode.');
          continue;
        }
        throw error;
      }

      // Run content checks
      checkTabs(fname, content);
      checkNewlines(fname, content);
      checkEndNewline(fname, content);
      checkEsphomeLinks(fname, content);
      checkAutomationHeadings(fname, content);
      await checkInternalLinks(fname, content, anchorCache);

    } catch (error) {
      if (error.code !== 'ENOENT') {
        console.error(`Error processing ${fname}:`, error.message);
      }
    }
  }

  // Print errors
  if (errors.size > 0) {
    for (const [fname, errs] of [...errors.entries()].sort()) {
      console.log(`${colors.boldGreen}### File ${colors.bright}${fname}${colors.reset}\n`);

      for (const { lineno, col, msg } of errs) {
        console.log(
          `${colors.bright}${fname}:${lineno}:${col}:${colors.reset} ` +
          `${colors.bright}${colors.boldRed}lint:${colors.reset} ${msg}`
        );
      }
      console.log();
    }

    console.log(`${colors.boldRed}Found ${errors.size} file(s) with errors${colors.reset}`);
    process.exit(errors.size);
  } else {
    console.log(`${colors.boldGreen}✓ All checks passed!${colors.reset}`);
    process.exit(0);
  }
}

main().catch(error => {
  console.error(`${colors.boldRed}Fatal error:${colors.reset}`, error);
  process.exit(1);
});
