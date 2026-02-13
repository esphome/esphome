#!/usr/bin/env node
/**
 * SEO Report: Generate CSV comparing live site vs local Starlight build
 */

import { readdir, readFile, writeFile } from 'fs/promises';
import { join, relative } from 'path';
import { JSDOM } from 'jsdom';

const LIVE_SITE = 'https://esphome.io';
const DIST_DIR = 'dist';
const OUTPUT_FILE = 'seo_report.csv';

/**
 * Extract all SEO data from HTML
 */
function extractSEO(html, url) {
  const dom = new JSDOM(html);
  const doc = dom.window.document;

  const title = doc.querySelector('title')?.textContent?.trim() || '';

  const getMeta = (name) => {
    const el = doc.querySelector(`meta[name="${name}"], meta[property="${name}"]`);
    return el?.getAttribute('content') || '';
  };

  const canonical = doc.querySelector('link[rel="canonical"]')?.getAttribute('href') || '';
  const h1s = [...doc.querySelectorAll('h1')].map(h => h.textContent?.trim()).join(' | ');
  const h1Count = doc.querySelectorAll('h1').length;

  return {
    url,
    title,
    titleLength: title.length,
    description: getMeta('description'),
    descriptionLength: getMeta('description').length,
    canonical,
    ogTitle: getMeta('og:title'),
    ogDescription: getMeta('og:description'),
    ogImage: getMeta('og:image'),
    ogType: getMeta('og:type'),
    ogUrl: getMeta('og:url'),
    twitterCard: getMeta('twitter:card'),
    twitterTitle: getMeta('twitter:title'),
    twitterDescription: getMeta('twitter:description'),
    twitterImage: getMeta('twitter:image'),
    h1s,
    h1Count,
    robots: getMeta('robots'),
  };
}

async function* getHtmlFiles(dir) {
  const stack = [dir];
  while (stack.length > 0) {
    const current = stack.pop();
    const entries = await readdir(current, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = join(current, entry.name);
      if (entry.isDirectory()) {
        stack.push(fullPath);
      } else if (entry.name === 'index.html') {
        yield fullPath;
      }
    }
  }
}

function localToUrlPath(localPath) {
  let path = relative(DIST_DIR, localPath);
  path = path.replace(/index\.html$/, '');
  if (path === '') path = '/';
  else path = '/' + path;
  return path;
}

function escapeCSV(value) {
  if (value === null || value === undefined) return '';
  const str = String(value);
  if (str.includes(',') || str.includes('"') || str.includes('\n')) {
    return '"' + str.replace(/"/g, '""') + '"';
  }
  return str;
}

async function fetchWithTimeout(url, timeout = 10000) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeout);
  try {
    const response = await fetch(url, { signal: controller.signal });
    clearTimeout(timeoutId);
    return response;
  } catch (e) {
    clearTimeout(timeoutId);
    throw e;
  }
}

async function main() {
  console.log('Generating SEO Report (local vs live)...\n');

  const localPages = [];
  for await (const file of getHtmlFiles(DIST_DIR)) {
    localPages.push(file);
  }

  console.log(`Found ${localPages.length} pages to analyze\n`);

  const headers = [
    'URL',
    'Local Title',
    'Local Title Length',
    'Live Title',
    'Live Title Length',
    'Title Match',
    'Local Description',
    'Local Desc Length',
    'Live Description',
    'Live Desc Length',
    'Desc Match',
    'Local Canonical',
    'Live Canonical',
    'Local og:title',
    'Live og:title',
    'Local og:description',
    'Live og:description',
    'Local og:image',
    'Live og:image',
    'Local og:type',
    'Live og:type',
    'Local og:url',
    'Live og:url',
    'Local twitter:card',
    'Live twitter:card',
    'Local twitter:image',
    'Live twitter:image',
    'Local H1 Count',
    'Live H1 Count',
    'Local H1s',
    'Live H1s',
    'Local robots',
    'Live robots',
    'Issues'
  ];

  const rows = [headers.map(escapeCSV).join(',')];

  let processed = 0;
  const batchSize = 10;

  for (let i = 0; i < localPages.length; i += batchSize) {
    const batch = localPages.slice(i, i + batchSize);

    const results = await Promise.all(batch.map(async (localFile) => {
      const urlPath = localToUrlPath(localFile);
      const liveUrl = new URL(urlPath, LIVE_SITE).href;

      const localHtml = await readFile(localFile, 'utf-8');
      const local = extractSEO(localHtml, urlPath);

      let live = null;
      try {
        const resp = await fetchWithTimeout(liveUrl);
        if (resp.ok) {
          const liveHtml = await resp.text();
          live = extractSEO(liveHtml, liveUrl);
        }
      } catch (e) {}

      const issues = [];
      if (!local.title) issues.push('Missing title');
      if (local.titleLength > 60) issues.push('Title too long');
      if (local.titleLength < 30 && local.title) issues.push('Title short');
      if (!local.description) issues.push('Missing description');
      if (local.descriptionLength > 160) issues.push('Desc too long');
      if (local.descriptionLength < 50 && local.description) issues.push('Desc short');
      if (local.h1Count === 0) issues.push('Missing H1');
      if (local.h1Count > 1) issues.push('Multiple H1s');
      if (!local.canonical) issues.push('Missing canonical');
      if (!local.ogImage) issues.push('Missing og:image');
      if (!local.ogTitle) issues.push('Missing og:title');
      if (!local.ogDescription) issues.push('Missing og:description');

      const titleMatch = live ? (local.title === live.title ? 'Yes' : 'No') : 'N/A';
      const descMatch = live ? (local.description === live.description ? 'Yes' : 'No') : 'N/A';

      return [
        urlPath,
        local.title,
        local.titleLength,
        live?.title || '',
        live?.titleLength || '',
        titleMatch,
        local.description,
        local.descriptionLength,
        live?.description || '',
        live?.descriptionLength || '',
        descMatch,
        local.canonical,
        live?.canonical || '',
        local.ogTitle,
        live?.ogTitle || '',
        local.ogDescription,
        live?.ogDescription || '',
        local.ogImage,
        live?.ogImage || '',
        local.ogType,
        live?.ogType || '',
        local.ogUrl,
        live?.ogUrl || '',
        local.twitterCard,
        live?.twitterCard || '',
        local.twitterImage,
        live?.twitterImage || '',
        local.h1Count,
        live?.h1Count || '',
        local.h1s,
        live?.h1s || '',
        local.robots,
        live?.robots || '',
        issues.join('; ')
      ].map(escapeCSV).join(',');
    }));

    rows.push(...results);
    processed += batch.length;
    process.stdout.write(`\rProcessed ${processed}/${localPages.length} pages...`);
  }

  console.log('\n\nWriting CSV...');
  await writeFile(OUTPUT_FILE, rows.join('\n'), 'utf-8');
  console.log(`\nReport saved to: ${OUTPUT_FILE}`);
  console.log(`Total pages: ${localPages.length}`);
}

main().catch(console.error);
