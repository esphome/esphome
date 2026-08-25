const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const {
  isBot,
  isRewrite,
  hasHumanReview,
  sanitizeForProse,
  buildWarningBody,
} = require('../lib');

function makeCore() {
  const messages = [];
  return { core: { info: msg => messages.push(msg) }, messages };
}

describe('isBot', () => {
  it('treats a GitHub App as a bot via type', () => {
    assert.equal(isBot({ type: 'Bot', login: 'dependabot[bot]' }), true);
  });

  it('treats known PAT-backed bot logins as bots regardless of case', () => {
    assert.equal(isBot({ type: 'User', login: 'EsphBot' }), true);
    assert.equal(isBot({ type: 'User', login: 'bluetoothbot' }), true);
    assert.equal(isBot({ type: 'User', login: 'esphomebot' }), true);
  });

  it('does not flag a human login that merely ends in "bot"', () => {
    assert.equal(isBot({ type: 'User', login: 'talbot' }), false);
  });

  it('treats a plain human user as not a bot', () => {
    assert.equal(isBot({ type: 'User', login: 'bdraco' }), false);
  });

  it('handles a missing user gracefully', () => {
    assert.equal(isBot(undefined), false);
  });
});

describe('isRewrite', () => {
  it('returns false for a fast-forward (status ahead, behind_by 0)', async () => {
    const github = {
      rest: { repos: { compareCommits: async () => ({ data: { status: 'ahead', behind_by: 0 } }) } },
    };
    const { core } = makeCore();
    assert.equal(await isRewrite(github, 'o', 'r', 'b', 'a', core), false);
  });

  it('returns true when diverged', async () => {
    const github = {
      rest: { repos: { compareCommits: async () => ({ data: { status: 'diverged', behind_by: 2 } }) } },
    };
    const { core } = makeCore();
    assert.equal(await isRewrite(github, 'o', 'r', 'b', 'a', core), true);
  });

  it('returns true when behind_by > 0 even if status is not "diverged"', async () => {
    const github = {
      rest: { repos: { compareCommits: async () => ({ data: { status: 'behind', behind_by: 3 } }) } },
    };
    const { core } = makeCore();
    assert.equal(await isRewrite(github, 'o', 'r', 'b', 'a', core), true);
  });

  it('treats a 404 as a rewrite when `after` still resolves', async () => {
    const github = {
      rest: {
        repos: {
          compareCommits: async () => {
            throw { status: 404 };
          },
          getCommit: async () => ({ data: {} }),
        },
      },
    };
    const { core } = makeCore();
    assert.equal(await isRewrite(github, 'o', 'r', 'b', 'a', core), true);
  });

  it('stays silent on a 404 when `after` also fails to resolve', async () => {
    const github = {
      rest: {
        repos: {
          compareCommits: async () => {
            throw { status: 404 };
          },
          getCommit: async () => {
            throw { status: 404 };
          },
        },
      },
    };
    const { core } = makeCore();
    assert.equal(await isRewrite(github, 'o', 'r', 'b', 'a', core), false);
  });

  it('rethrows a non-404 error from compareCommits', async () => {
    const github = {
      rest: {
        repos: {
          compareCommits: async () => {
            throw { status: 500, message: 'boom' };
          },
        },
      },
    };
    const { core } = makeCore();
    await assert.rejects(() => isRewrite(github, 'o', 'r', 'b', 'a', core));
  });
});

describe('hasHumanReview', () => {
  it('is false with no comments or reviews', () => {
    assert.equal(hasHumanReview([], []), false);
  });

  it('is false when only bots left inline review comments', () => {
    const comments = [{ user: { type: 'Bot', login: 'copilot-pull-request-reviewer[bot]' } }];
    assert.equal(hasHumanReview(comments, []), false);
  });

  it('is false when a human leaves a plain inline comment with no suggestion', () => {
    const comments = [{ user: { type: 'User', login: 'bdraco' }, body: 'why is this needed?' }];
    assert.equal(hasHumanReview(comments, []), false);
  });

  it('is true when a human left an inline code-change suggestion', () => {
    const comments = [
      { user: { type: 'User', login: 'bdraco' }, body: 'use this instead:\n```suggestion\nconst x = 1;\n```' },
    ];
    assert.equal(hasHumanReview(comments, []), true);
  });

  it('is false when the PR author leaves a plain self-note on their own diff', () => {
    const comments = [{ user: { type: 'User', login: 'clydebarrow' }, body: 'this is the part to look at' }];
    assert.equal(hasHumanReview(comments, []), false);
  });

  it('is true for a human top-level review with no inline comments', () => {
    const reviews = [{ user: { type: 'User', login: 'bdraco' }, state: 'APPROVED' }];
    assert.equal(hasHumanReview([], reviews), true);
  });

  it('is true for a DISMISSED review', () => {
    const reviews = [{ user: { type: 'User', login: 'bdraco' }, state: 'DISMISSED' }];
    assert.equal(hasHumanReview([], reviews), true);
  });

  it('ignores a PENDING (not yet submitted) review', () => {
    const reviews = [{ user: { type: 'User', login: 'bdraco' }, state: 'PENDING' }];
    assert.equal(hasHumanReview([], reviews), false);
  });

  it('ignores a review from a PAT-backed bot even with a countable state', () => {
    const reviews = [{ user: { type: 'User', login: 'esphbot' }, state: 'APPROVED' }];
    assert.equal(hasHumanReview([], reviews), false);
  });
});

describe('sanitizeForProse', () => {
  it('strips backticks', () => {
    assert.equal(sanitizeForProse('feat/`inject`'), 'feat/inject');
  });

  it('neutralizes @ with a zero-width space so it cannot mention a user', () => {
    assert.equal(sanitizeForProse('fix-@someone'), 'fix-@​someone');
  });

  it('leaves an ordinary branch name untouched', () => {
    assert.equal(sanitizeForProse('feature/foo'), 'feature/foo');
  });
});

describe('buildWarningBody', () => {
  const args = {
    marker: '<!-- force-push-warning -->',
    branch: 'release@2.0',
    before: 'deadbee',
    after: 'cafebabe1234567',
    owner: 'esphome',
    repo: 'esphome',
  };

  it('includes the marker for dedup', () => {
    assert.match(buildWarningBody(args), /<!-- force-push-warning -->/);
  });

  it('uses the sanitized branch name in prose', () => {
    const body = buildWarningBody(args);
    assert.match(body, /A force-push to `release@​2\.0`/);
  });

  it('uses the raw branch name in the copy-pasteable restore command', () => {
    const body = buildWarningBody(args);
    assert.match(body, /git push --force origin deadbee:release@2\.0/);
  });

  it('links to the permalink for the old tip', () => {
    const body = buildWarningBody(args);
    assert.match(body, /https:\/\/github\.com\/esphome\/esphome\/commit\/deadbee/);
  });
});
