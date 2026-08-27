const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const {
  detectMergeBranch,
  detectNewPlatforms,
  detectNewComponents,
  detectPRSize,
  detectPRTemplateCheckboxes,
  detectRequirements,
} = require('../detectors');
const { MANAGED_LABELS } = require('../constants');

// Minimal GitHub API mock — only repos.getContent is called by detectNewPlatforms/detectNewComponents
// to check for CONFIG_SCHEMA in newly added files.
function makeGithub(content = '') {
  return {
    rest: {
      repos: {
        getContent: async () => ({
          data: { content: Buffer.from(content).toString('base64') }
        })
      }
    }
  };
}

const CONTEXT = {
  repo: { owner: 'esphome', repo: 'esphome' },
  payload: { pull_request: { head: { sha: 'abc123' }, base: { ref: 'dev' } } }
};

const API_DATA = {
  targetPlatforms: ['esp32', 'esp8266', 'rp2040'],
  platformComponents: ['cover', 'sensor', 'binary_sensor', 'switch', 'light', 'fan', 'climate', 'valve']
};

const WITH_SCHEMA = 'CONFIG_SCHEMA = cv.Schema({})';
const WITHOUT_SCHEMA = 'CODEOWNERS = ["@esphome/core"]';

// ---------------------------------------------------------------------------
// detectMergeBranch
// ---------------------------------------------------------------------------

// Builds a fresh context for detectMergeBranch tests instead of mutating the
// shared CONTEXT fixture above (which other describe blocks rely on).
function makeMergeContext(baseRef, { stack, defaultBranch = 'dev' } = {}) {
  const pull_request = { number: 1, base: { ref: baseRef } };
  if (stack !== undefined) {
    pull_request.stack = stack;
  }
  return {
    repo: { owner: 'esphome', repo: 'esphome' },
    payload: { pull_request, repository: { default_branch: defaultBranch } }
  };
}

// A GitHub API mock exposing only rest.pulls.get, with a call counter so
// tests can assert whether the API fallback was actually invoked.
function makeStackGithub({ stack = null, error = null } = {}) {
  const state = { calls: 0 };
  const github = {
    rest: {
      pulls: {
        get: async () => {
          state.calls++;
          if (error) throw error;
          return { data: { stack } };
        }
      }
    }
  };
  return { github, state };
}

const STACK_INFO = { base: { ref: 'dev' }, id: 71540, number: 17978, position: 3, size: 3 };

describe('detectMergeBranch', () => {
  it('base ref release adds merging-to-release only and never checks the stack', async () => {
    const { github, state } = makeStackGithub({ stack: STACK_INFO });
    const context = makeMergeContext('release', { stack: STACK_INFO });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['merging-to-release']);
    assert.equal(state.calls, 0);
  });

  it('base ref beta adds merging-to-beta only and never checks the stack', async () => {
    const { github, state } = makeStackGithub({ stack: STACK_INFO });
    const context = makeMergeContext('beta', { stack: STACK_INFO });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['merging-to-beta']);
    assert.equal(state.calls, 0);
  });

  it('stack present on the webhook payload adds stacked-pr without calling the API', async () => {
    const { github, state } = makeStackGithub();
    const context = makeMergeContext('feature-branch', { stack: STACK_INFO });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['stacked-pr']);
    assert.equal(state.calls, 0);
  });

  it('stack absent from payload falls back to the API and adds stacked-pr', async () => {
    const { github, state } = makeStackGithub({ stack: STACK_INFO });
    const context = makeMergeContext('feature-branch');
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['stacked-pr']);
    assert.equal(state.calls, 1);
  });

  it('bottom of a stack (base ref dev, stack present) still adds stacked-pr', async () => {
    const { github, state } = makeStackGithub();
    const context = makeMergeContext('dev', { stack: STACK_INFO });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['stacked-pr']);
    assert.equal(state.calls, 0);
  });

  it('not stacked, base ref not dev adds chained-pr', async () => {
    const { github } = makeStackGithub({ stack: null });
    const context = makeMergeContext('feature-branch');
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['chained-pr']);
  });

  it('not stacked, base ref dev adds no labels', async () => {
    const { github } = makeStackGithub({ stack: null });
    const context = makeMergeContext('dev');
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), []);
  });

  it('a failed stack lookup falls back to not-stacked, so a feature-branch base adds chained-pr', async () => {
    const { github, state } = makeStackGithub({ error: new Error('API unavailable') });
    const context = makeMergeContext('feature-branch');
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['chained-pr']);
    assert.equal(state.calls, 1);
  });

  it('base ref matches default branch adds no labels', async () => {
    const { github } = makeStackGithub({ stack: null });
    const context = makeMergeContext('other', { defaultBranch: 'other' });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), []);
  });

  it('base ref dev when the default branch is main adds chained-pr', async () => {
    const { github } = makeStackGithub({ stack: null });
    const context = makeMergeContext('dev', { defaultBranch: 'main' });
    const labels = await detectMergeBranch(github, context);
    assert.deepEqual(Array.from(labels).sort(), ['chained-pr']);
  });

});

// ---------------------------------------------------------------------------
// detectNewPlatforms
// ---------------------------------------------------------------------------

describe('detectNewPlatforms', () => {
  describe('restructure detection (no false positives)', () => {
    it('flat .py -> subdir __init__.py is not a new platform', async () => {
      const prFiles = [
        { filename: 'esphome/components/endstop/cover.py', status: 'removed' },
        { filename: 'esphome/components/endstop/cover/__init__.py', status: 'added' },
      ];
      const result = await detectNewPlatforms(makeGithub(WITH_SCHEMA), CONTEXT, prFiles, API_DATA);
      assert.equal(result.labels.size, 0);
      assert.equal(result.hasYamlLoadable, false);
    });

    it('subdir __init__.py -> flat .py is not a new platform', async () => {
      const prFiles = [
        { filename: 'esphome/components/endstop/cover/__init__.py', status: 'removed' },
        { filename: 'esphome/components/endstop/cover.py', status: 'added' },
      ];
      const result = await detectNewPlatforms(makeGithub(WITH_SCHEMA), CONTEXT, prFiles, API_DATA);
      assert.equal(result.labels.size, 0);
      assert.equal(result.hasYamlLoadable, false);
    });
  });

  describe('genuine new platforms', () => {
    it('new subdir platform with CONFIG_SCHEMA sets new-platform and hasYamlLoadable', async () => {
      const prFiles = [
        { filename: 'esphome/components/my_sensor/cover/__init__.py', status: 'added' },
      ];
      const result = await detectNewPlatforms(makeGithub(WITH_SCHEMA), CONTEXT, prFiles, API_DATA);
      assert.ok(result.labels.has('new-platform'));
      assert.equal(result.hasYamlLoadable, true);
    });

    it('new flat platform with CONFIG_SCHEMA sets new-platform and hasYamlLoadable', async () => {
      const prFiles = [
        { filename: 'esphome/components/my_sensor/cover.py', status: 'added' },
      ];
      const result = await detectNewPlatforms(makeGithub(WITH_SCHEMA), CONTEXT, prFiles, API_DATA);
      assert.ok(result.labels.has('new-platform'));
      assert.equal(result.hasYamlLoadable, true);
    });

    it('new platform without CONFIG_SCHEMA sets new-platform but not hasYamlLoadable', async () => {
      const prFiles = [
        { filename: 'esphome/components/my_sensor/cover.py', status: 'added' },
      ];
      const result = await detectNewPlatforms(makeGithub(WITHOUT_SCHEMA), CONTEXT, prFiles, API_DATA);
      assert.ok(result.labels.has('new-platform'));
      assert.equal(result.hasYamlLoadable, false);
    });

    it('non-platform file addition produces no labels', async () => {
      const prFiles = [
        { filename: 'esphome/components/my_sensor/sensor.py', status: 'added' },
      ];
      // Override platformComponents so 'sensor' is not a recognized platform -> no label expected.
      const nonPlatformApiData = { ...API_DATA, platformComponents: ['cover'] };
      const result = await detectNewPlatforms(makeGithub(WITH_SCHEMA), CONTEXT, prFiles, nonPlatformApiData);
      assert.equal(result.labels.size, 0);
      assert.equal(result.hasYamlLoadable, false);
    });
  });
});

// ---------------------------------------------------------------------------
// detectNewComponents
// ---------------------------------------------------------------------------

describe('detectNewComponents', () => {
  it('new top-level __init__.py sets new-component', async () => {
    const prFiles = [
      { filename: 'esphome/components/actuator/__init__.py', status: 'added', },
    ];
    const result = await detectNewComponents(makeGithub(WITHOUT_SCHEMA), CONTEXT, prFiles);
    assert.ok(result.labels.has('new-component'));
    assert.equal(result.hasYamlLoadable, false);
  });

  it('new top-level __init__.py with CONFIG_SCHEMA sets hasYamlLoadable', async () => {
    const prFiles = [
      { filename: 'esphome/components/my_component/__init__.py', status: 'added' },
    ];
    const result = await detectNewComponents(makeGithub(WITH_SCHEMA), CONTEXT, prFiles);
    assert.ok(result.labels.has('new-component'));
    assert.equal(result.hasYamlLoadable, true);
  });

  it('new top-level __init__.py with IS_TARGET_PLATFORM sets new-target-platform', async () => {
    const prFiles = [
      { filename: 'esphome/components/my_platform/__init__.py', status: 'added' },
    ];
    const result = await detectNewComponents(makeGithub('IS_TARGET_PLATFORM = True'), CONTEXT, prFiles);
    assert.ok(result.labels.has('new-component'));
    assert.ok(result.labels.has('new-target-platform'));
  });

  it('modified __init__.py does not set new-component', async () => {
    const prFiles = [
      { filename: 'esphome/components/existing/__init__.py', status: 'modified' },
    ];
    const result = await detectNewComponents(makeGithub(WITH_SCHEMA), CONTEXT, prFiles);
    assert.equal(result.labels.size, 0);
  });

  it('nested __init__.py does not set new-component', async () => {
    const prFiles = [
      { filename: 'esphome/components/endstop/cover/__init__.py', status: 'added' },
    ];
    const result = await detectNewComponents(makeGithub(WITH_SCHEMA), CONTEXT, prFiles);
    assert.equal(result.labels.size, 0);
  });
});

// ---------------------------------------------------------------------------
// detectPRTemplateCheckboxes
// ---------------------------------------------------------------------------

const NEW_FEATURE_LINE = '- [x] New feature (non-breaking change which adds functionality)';
const DEV_FEATURE_LINE = '- [x] New developer-facing feature (adds functionality for component developers; no end-user configuration change)';
const DEV_FEATURE_LINE_UNTICKED = '- [ ] New developer-facing feature (adds functionality for component developers; no end-user configuration change)';

function makeBodyContext(body) {
  return { payload: { pull_request: { body } } };
}

describe('detectPRTemplateCheckboxes', () => {
  it('ticked developer-facing feature checkbox adds new-feature-developer only', async () => {
    const labels = await detectPRTemplateCheckboxes(makeBodyContext(DEV_FEATURE_LINE));
    assert.ok(labels.has('new-feature-developer'));
    assert.ok(!labels.has('new-feature'));
  });

  it('unticked developer-facing feature checkbox adds no label', async () => {
    const labels = await detectPRTemplateCheckboxes(makeBodyContext(DEV_FEATURE_LINE_UNTICKED));
    assert.ok(!labels.has('new-feature-developer'));
  });

  it('ticked new feature checkbox does not add new-feature-developer', async () => {
    const labels = await detectPRTemplateCheckboxes(makeBodyContext(NEW_FEATURE_LINE));
    assert.ok(labels.has('new-feature'));
    assert.ok(!labels.has('new-feature-developer'));
  });
});

// ---------------------------------------------------------------------------
// detectRequirements
// ---------------------------------------------------------------------------

describe('detectRequirements', () => {
  // PR body without any docs-PR link.
  const NO_DOCS_CONTEXT = makeBodyContext('Just a description, no docs link.');
  const USER_DOCS_CONTEXT = makeBodyContext('Docs: esphome/esphome.io#1234');
  const DEV_DOCS_CONTEXT = makeBodyContext('Docs: esphome/developers.esphome.io#1234');
  const DEV_DOCS_URL_CONTEXT = makeBodyContext('Docs: https://github.com/esphome/developers.esphome.io/pull/1234');

  // File sets: a normal source change vs. one confined to the exempt core validators.
  const SOURCE_FILES = [
    { filename: 'esphome/components/foo/foo.py' },
    { filename: 'tests/components/foo/common.yaml' },
  ];
  const VALIDATOR_FILES = [
    { filename: 'esphome/config_validation.py' },
    { filename: 'tests/unit_tests/test_config_validation.py' },
  ];

  it('new-feature-developer without has-tests adds needs-tests but not needs-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer']), SOURCE_FILES, NO_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-tests'));
    assert.ok(!labels.has('needs-docs'));
  });

  it('new-feature-developer with has-tests does not add needs-tests', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), SOURCE_FILES, NO_DOCS_CONTEXT, false);
    assert.ok(!labels.has('needs-tests'));
  });

  it('new-feature without a docs link still adds needs-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature', 'has-tests']), [], NO_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-docs'));
  });

  it('new-feature-developer without a developer docs link adds needs-developer-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), SOURCE_FILES, NO_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-developer-docs'));
  });

  it('a developers.esphome.io shorthand link satisfies needs-developer-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), SOURCE_FILES, DEV_DOCS_CONTEXT, false);
    assert.ok(!labels.has('needs-developer-docs'));
  });

  it('a developers.esphome.io URL link satisfies needs-developer-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), SOURCE_FILES, DEV_DOCS_URL_CONTEXT, false);
    assert.ok(!labels.has('needs-developer-docs'));
  });

  it('a user docs (esphome.io) link does not satisfy needs-developer-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), SOURCE_FILES, USER_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-developer-docs'));
  });

  it('a developer docs link does not satisfy needs-docs for new-feature', async () => {
    const labels = await detectRequirements(new Set(['new-feature', 'has-tests']), [], DEV_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-docs'));
  });

  it('changes confined to core validator files are exempt from needs-developer-docs', async () => {
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), VALIDATOR_FILES, NO_DOCS_CONTEXT, false);
    assert.ok(!labels.has('needs-developer-docs'));
  });

  it('validator changes mixed with other source files are not exempt', async () => {
    const prFiles = [...VALIDATOR_FILES, { filename: 'esphome/components/foo/foo.py' }];
    const labels = await detectRequirements(new Set(['new-feature-developer', 'has-tests']), prFiles, NO_DOCS_CONTEXT, false);
    assert.ok(labels.has('needs-developer-docs'));
  });
});

// ---------------------------------------------------------------------------
// MANAGED_LABELS
// ---------------------------------------------------------------------------

describe('MANAGED_LABELS', () => {
  it('includes new-feature-developer so the workflow syncs it', () => {
    assert.ok(MANAGED_LABELS.includes('new-feature-developer'));
  });

  it('includes needs-developer-docs so the workflow syncs it', () => {
    assert.ok(MANAGED_LABELS.includes('needs-developer-docs'));
  });
});

// ---------------------------------------------------------------------------
// detectPRSize
// ---------------------------------------------------------------------------

describe('detectPRSize', () => {
  const SMALL = 30;
  const MEDIUM = 100;
  const TOO_BIG = 1000;

  function size(prFiles, isMegaPR = false) {
    const totalAdditions = prFiles.reduce((sum, file) => sum + (file.additions || 0), 0);
    const totalDeletions = prFiles.reduce((sum, file) => sum + (file.deletions || 0), 0);
    return detectPRSize(prFiles, totalAdditions, totalDeletions, isMegaPR, SMALL, MEDIUM, TOO_BIG);
  }

  it('counts only non-test changes toward small-pr', async () => {
    // 10 source + 5000 test lines -> non-test churn of 10 is still small.
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 10, deletions: 0 },
      { filename: 'tests/components/foo/test.esp32-idf.yaml', additions: 5000, deletions: 0 },
    ]);
    assert.ok(labels.has('small-pr'));
    assert.equal(labels.size, 1);
  });

  it('counts additions and deletions as churn (not net delta)', async () => {
    // A balanced refactor (40 added, 40 removed) is 80 lines of churn -> medium, not small.
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 40, deletions: 40 },
    ]);
    assert.ok(labels.has('medium-pr'));
    assert.equal(labels.size, 1);
  });

  it('labels medium-pr when non-test changes exceed small threshold', async () => {
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 60, deletions: 0 },
      { filename: 'tests/components/foo/test.esp32-idf.yaml', additions: 5000, deletions: 0 },
    ]);
    assert.ok(labels.has('medium-pr'));
    assert.equal(labels.size, 1);
  });

  it('uses net delta (not churn) for too-big', async () => {
    // 600 added + 600 removed: 1200 churn (above too-big) but 0 net delta -> not too-big.
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 600, deletions: 600 },
    ]);
    assert.equal(labels.size, 0);
  });

  it('labels too-big when non-test changes exceed the big threshold', async () => {
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 2000, deletions: 0 },
      { filename: 'tests/components/foo/test.esp32-idf.yaml', additions: 5000, deletions: 0 },
    ]);
    assert.ok(labels.has('too-big'));
    assert.equal(labels.size, 1);
  });

  it('does not label too-big when mega-pr is set', async () => {
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 2000, deletions: 0 },
    ], true);
    assert.equal(labels.size, 0);
  });

  it('produces no size label for a large mega-pr in the gap above medium', async () => {
    // Non-test changes land between MEDIUM and TOO_BIG: not small/medium, and mega-pr suppresses too-big.
    const labels = await size([
      { filename: 'esphome/components/foo/foo.cpp', additions: 500, deletions: 0 },
    ], true);
    assert.equal(labels.size, 0);
  });
});
