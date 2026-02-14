#!/usr/bin/env node
/**
 * SEO Audit: Compare live site vs local Starlight build
 *
 * Checks:
 * - Meta tags (title, description, og:*, twitter:*)
 * - Canonical URLs
 * - Heading structure (h1, h2, h3)
 * - robots.txt and sitemap
 * - Internal/external link counts
 */

import { readdir, readFile } from 'fs/promises';
import { join, relative } from 'path';
import { JSDOM } from 'jsdom';

const LIVE_SITE = 'https://esphome.io';
const DIST_DIR = 'dist';
const SAMPLE_SIZE = 50; // Number of pages to audit

/**
 * Extract SEO data from HTML
 */
function extractSEO(html, url) {
  const dom = new JSDOM(html);
  const doc = dom.window.document;

  // Title
  const title = doc.querySelector('title')?.textContent?.trim() || '';

  // Meta tags
  const getMeta = (name) => {
    const el = doc.querySelector(`meta[name="${name}"], meta[property="${name}"]`);
    return el?.getAttribute('content') || '';
  };

  // Canonical
  const canonical = doc.querySelector('link[rel="canonical"]')?.getAttribute('href') || '';

  // Headings
  const h1s = [...doc.querySelectorAll('h1')].map(h => h.textContent?.trim());
  const h2Count = doc.querySelectorAll('h2').length;
  const h3Count = doc.querySelectorAll('h3').length;

  // Links
  const allLinks = [...doc.querySelectorAll('a[href]')];
  const internalLinks = allLinks.filter(a => {
    const href = a.getAttribute('href');
    return href?.startsWith('/') || href?.includes('esphome.io');
  }).length;
  const externalLinks = allLinks.filter(a => {
    const href = a.getAttribute('href');
    return href?.startsWith('http') && !href?.includes('esphome.io');
  }).length;

  // Images
  const images = doc.querySelectorAll('img');
  const imagesWithAlt = [...images].filter(img => img.getAttribute('alt')).length;

  return {
    url,
    title,
    titleLength: title.length,
    description: getMeta('description'),
    descriptionLength: getMeta('description').length,
    ogTitle: getMeta('og:title'),
    ogDescription: getMeta('og:description'),
    ogImage: getMeta('og:image'),
    twitterCard: getMeta('twitter:card'),
    canonical,
    h1s,
    h1Count: h1s.length,
    h2Count,
    h3Count,
    internalLinks,
    externalLinks,
    totalImages: images.length,
    imagesWithAlt,
  };
}

/**
 * Get sample of HTML files from dist
 */
async function* getHtmlFiles(dir, limit) {
  let count = 0;
  const stack = [dir];

  while (stack.length > 0 && count < limit) {
    const current = stack.pop();
    const entries = await readdir(current, { withFileTypes: true });

    for (const entry of entries) {
      if (count >= limit) break;

      const fullPath = join(current, entry.name);
      if (entry.isDirectory()) {
        stack.push(fullPath);
      } else if (entry.name === 'index.html') {
        count++;
        yield fullPath;
      }
    }
  }
}

/**
 * Convert local path to live URL
 */
function localToLiveUrl(localPath) {
  let path = relative(DIST_DIR, localPath);
  path = path.replace(/\/index\.html$/, '/');
  if (path === 'index.html') path = '/';
  return new URL(path, LIVE_SITE).href;
}

/**
 * Analyze and report issues
 */
function analyzeIssues(local, live) {
  const issues = [];

  // Title issues
  if (!local.title) issues.push('Missing title');
  if (local.titleLength > 60) issues.push(`Title too long (${local.titleLength} chars)`);
  if (local.titleLength < 30) issues.push(`Title too short (${local.titleLength} chars)`);

  // Description issues
  if (!local.description) issues.push('Missing meta description');
  if (local.descriptionLength > 160) issues.push(`Description too long (${local.descriptionLength} chars)`);
  if (local.descriptionLength < 50 && local.description) issues.push(`Description too short (${local.descriptionLength} chars)`);

  // H1 issues
  if (local.h1Count === 0) issues.push('Missing H1');
  if (local.h1Count > 1) issues.push(`Multiple H1s (${local.h1Count})`);

  // Canonical issues
  if (!local.canonical) issues.push('Missing canonical URL');

  // OG issues
  if (!local.ogTitle) issues.push('Missing og:title');
  if (!local.ogDescription) issues.push('Missing og:description');
  if (!local.ogImage) issues.push('Missing og:image');

  // Comparison with live
  if (live) {
    if (local.title !== live.title && !local.title.includes(live.title.split(' - ')[0])) {
      issues.push('Title changed significantly');
    }
    if (local.description && live.description &&
        local.description !== live.description) {
      issues.push('Description changed');
    }
  }

  return issues;
}

async function main() {
  console.log('SEO Audit: Live Site vs Starlight Migration\n');
  console.log('='.repeat(60));

  // Collect sample pages
  const localPages = [];
  for await (const file of getHtmlFiles(DIST_DIR, SAMPLE_SIZE)) {
    localPages.push(file);
  }

  console.log(`\nAuditing ${localPages.length} pages...\n`);

  const results = {
    local: [],
    live: [],
    issues: [],
  };

  // Audit each page
  for (const localFile of localPages) {
    const liveUrl = localToLiveUrl(localFile);
    const relPath = relative(DIST_DIR, localFile).replace('/index.html', '/');

    process.stdout.write('.');

    // Local
    const localHtml = await readFile(localFile, 'utf-8');
    const localSEO = extractSEO(localHtml, relPath);
    results.local.push(localSEO);

    // Live
    let liveSEO = null;
    try {
      const resp = await fetch(liveUrl, { timeout: 10000 });
      if (resp.ok) {
        const liveHtml = await resp.text();
        liveSEO = extractSEO(liveHtml, liveUrl);
        results.live.push(liveSEO);
      }
    } catch (e) {
      // Skip if can't fetch live
    }

    // Analyze
    const issues = analyzeIssues(localSEO, liveSEO);
    if (issues.length > 0) {
      results.issues.push({ url: relPath, issues });
    }
  }

  console.log('\n\n');

  // Summary stats
  console.log('='.repeat(60));
  console.log('SUMMARY');
  console.log('='.repeat(60));

  const localStats = {
    withTitle: results.local.filter(p => p.title).length,
    withDescription: results.local.filter(p => p.description).length,
    withCanonical: results.local.filter(p => p.canonical).length,
    withOgTitle: results.local.filter(p => p.ogTitle).length,
    withOgDescription: results.local.filter(p => p.ogDescription).length,
    withOgImage: results.local.filter(p => p.ogImage).length,
    withSingleH1: results.local.filter(p => p.h1Count === 1).length,
  };

  const liveStats = results.live.length > 0 ? {
    withTitle: results.live.filter(p => p.title).length,
    withDescription: results.live.filter(p => p.description).length,
    withCanonical: results.live.filter(p => p.canonical).length,
    withOgTitle: results.live.filter(p => p.ogTitle).length,
    withOgDescription: results.live.filter(p => p.ogDescription).length,
    withOgImage: results.live.filter(p => p.ogImage).length,
    withSingleH1: results.live.filter(p => p.h1Count === 1).length,
  } : null;

  const total = results.local.length;

  console.log(`\n${'Metric'.padEnd(25)} ${'Local'.padStart(8)} ${'Live'.padStart(8)} ${'Impact'.padStart(10)}`);
  console.log('-'.repeat(55));

  const metrics = [
    ['Has title', localStats.withTitle, liveStats?.withTitle],
    ['Has description', localStats.withDescription, liveStats?.withDescription],
    ['Has canonical', localStats.withCanonical, liveStats?.withCanonical],
    ['Has og:title', localStats.withOgTitle, liveStats?.withOgTitle],
    ['Has og:description', localStats.withOgDescription, liveStats?.withOgDescription],
    ['Has og:image', localStats.withOgImage, liveStats?.withOgImage],
    ['Single H1', localStats.withSingleH1, liveStats?.withSingleH1],
  ];

  for (const [name, local, live] of metrics) {
    const localPct = ((local / total) * 100).toFixed(0) + '%';
    const livePct = live !== undefined ? ((live / results.live.length) * 100).toFixed(0) + '%' : 'N/A';
    const impact = live !== undefined ? (local >= live ? '✓' : '⚠️  WORSE') : '';
    console.log(`${name.padEnd(25)} ${localPct.padStart(8)} ${livePct.padStart(8)} ${impact.padStart(10)}`);
  }

  // Pages with issues
  if (results.issues.length > 0) {
    console.log(`\n${'='.repeat(60)}`);
    console.log(`PAGES WITH ISSUES (${results.issues.length})`);
    console.log('='.repeat(60));

    // Group by issue type
    const issueTypes = {};
    for (const { url, issues } of results.issues) {
      for (const issue of issues) {
        if (!issueTypes[issue]) issueTypes[issue] = [];
        issueTypes[issue].push(url);
      }
    }

    console.log('\nIssue breakdown:');
    for (const [issue, urls] of Object.entries(issueTypes).sort((a, b) => b[1].length - a[1].length)) {
      console.log(`  ${urls.length.toString().padStart(3)}x ${issue}`);
    }

    console.log('\nSample pages with issues:');
    for (const { url, issues } of results.issues.slice(0, 15)) {
      console.log(`  ${url}`);
      for (const issue of issues) {
        console.log(`      - ${issue}`);
      }
    }
  }

  // Check robots.txt and sitemap
  console.log(`\n${'='.repeat(60)}`);
  console.log('ROBOTS & SITEMAP');
  console.log('='.repeat(60));

  try {
    const robotsLocal = await readFile(join(DIST_DIR, 'robots.txt'), 'utf-8').catch(() => null);
    const robotsLive = await fetch(`${LIVE_SITE}/robots.txt`).then(r => r.text()).catch(() => null);

    console.log(`\nrobots.txt: ${robotsLocal ? '✓ Present' : '✗ Missing'} (local) | ${robotsLive ? '✓ Present' : '✗ Missing'} (live)`);
  } catch (e) {}

  try {
    const sitemapLocal = await readFile(join(DIST_DIR, 'sitemap-index.xml'), 'utf-8').catch(() =>
      readFile(join(DIST_DIR, 'sitemap.xml'), 'utf-8').catch(() => null)
    );
    const sitemapLive = await fetch(`${LIVE_SITE}/sitemap.xml`).then(r => r.ok ? r.text() : null).catch(() => null);

    console.log(`sitemap.xml: ${sitemapLocal ? '✓ Present' : '✗ Missing'} (local) | ${sitemapLive ? '✓ Present' : '✗ Missing'} (live)`);

    if (sitemapLocal) {
      // Check for sitemap index - if found, count entries in sitemap-0.xml
      if (sitemapLocal.includes('sitemapindex')) {
        const sitemap0 = await readFile(join(DIST_DIR, 'sitemap-0.xml'), 'utf-8').catch(() => null);
        const localUrls = sitemap0 ? (sitemap0.match(/<loc>/g) || []).length : 0;
        console.log(`  Local sitemap entries: ${localUrls}`);
      } else {
        const localUrls = (sitemapLocal.match(/<loc>/g) || []).length;
        console.log(`  Local sitemap entries: ${localUrls}`);
      }
    }
  } catch (e) {}

  console.log('\n');
}

main().catch(console.error);
