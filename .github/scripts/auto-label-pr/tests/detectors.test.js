const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { detectNewPlatforms, detectNewComponents } = require('../detectors');

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
