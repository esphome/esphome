const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { detectNewPlatforms, detectNewComponents, detectPRSize } = require('../detectors');

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
