#!/usr/bin/env node
/**
 * Check that all URLs from the live site exist in the local build
 *
 * Usage:
 *   npm run build
 *   node script/check_urls.mjs
 */

import { existsSync } from 'fs';
import { join } from 'path';

const LIVE_SITEMAP = 'https://esphome.io/sitemap.xml';
const DIST_DIR = 'dist';

/**
 * Convert live URL to local dist path
 * https://esphome.io/components/sensor/dht/ -> dist/components/sensor/dht/index.html
 */
function liveUrlToLocalPath(url) {
  const u = new URL(url);
  let path = u.pathname;

  // Remove leading slash
  if (path.startsWith('/')) {
    path = path.slice(1);
  }

  // Handle trailing slash - add index.html
  if (path.endsWith('/')) {
    path = path + 'index.html';
  } else if (!path.endsWith('.html')) {
    path = path + '/index.html';
  }

  // Handle root
  if (path === 'index.html' || path === '') {
    path = 'index.html';
  }

  return join(DIST_DIR, path);
}

/**
 * Parse sitemap XML and extract URLs
 */
function extractUrls(xml) {
  const urls = [];
  const regex = /<loc>([^<]+)<\/loc>/g;
  let match;
  while ((match = regex.exec(xml)) !== null) {
    urls.push(match[1]);
  }
  return urls;
}

async function main() {
  console.log('Fetching live sitemap...');

  const resp = await fetch(LIVE_SITEMAP);
  if (!resp.ok) {
    console.error(`Failed to fetch sitemap: ${resp.status}`);
    process.exit(1);
  }

  const xml = await resp.text();
  const liveUrls = extractUrls(xml);

  console.log(`Found ${liveUrls.length} URLs in live sitemap\n`);

  const missing = [];
  const found = [];

  for (const url of liveUrls) {
    const localPath = liveUrlToLocalPath(url);

    if (existsSync(localPath)) {
      found.push({ url, localPath });
    } else {
      missing.push({ url, localPath });
    }
  }

  console.log('='.repeat(50));
  console.log('RESULTS');
  console.log('='.repeat(50));
  console.log(`  Found locally:     ${found.length.toString().padStart(4)}`);
  console.log(`  Missing:           ${missing.length.toString().padStart(4)}`);
  console.log('='.repeat(50));

  if (missing.length > 0) {
    console.log(`\n--- Missing URLs (${missing.length}) ---`);
    for (const { url, localPath } of missing.slice(0, 50)) {
      console.log(`  ${url}`);
      console.log(`    -> ${localPath}`);
    }
    if (missing.length > 50) {
      console.log(`  ... and ${missing.length - 50} more`);
    }

    console.log('\n✗ Some URLs are missing');
    process.exit(1);
  } else {
    console.log('\n✓ All live URLs exist locally');
    process.exit(0);
  }
}

main().catch(e => {
  console.error(e);
  process.exit(1);
});
