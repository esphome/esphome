const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const { generateReviewMessages } = require('../reviews');

const MAX_LABELS = 15;
const TOO_BIG_THRESHOLD = 1000;

function tooBigMessage(labelCount, additions) {
  const files = [{ filename: 'esphome/components/a/a.cpp', additions, deletions: 0 }];
  const messages = generateReviewMessages(['too-big'], labelCount, null, files, additions, 0, 'someone', MAX_LABELS, TOO_BIG_THRESHOLD);
  assert.equal(messages.length, 1);
  return messages[0];
}

describe('too-big review message', () => {
  it('names the line cap when the PR is too long', () => {
    const message = tooBigMessage(3, 1500);
    assert.match(message, /will not be reviewed until it is at most 1000 lines \(excluding tests\)\./);
    assert.match(message, /removed automatically once the PR is at most 1000 lines/);
    assert.doesNotMatch(message, /components\/areas\./);
  });

  it('names the area cap when the PR touches too many areas but is short', () => {
    const message = tooBigMessage(20, 200);
    assert.match(message, /will not be reviewed until it touches at most 15 components\/areas\./);
    assert.doesNotMatch(message, /until it is at most 1000 lines/);
  });

  it('names both caps when both are exceeded', () => {
    const message = tooBigMessage(20, 1500);
    assert.match(message, /is at most 1000 lines \(excluding tests\) and touches at most 15 components\/areas\./);
  });

  it('tells new-component authors to bring one platform first', () => {
    assert.match(tooBigMessage(3, 1500), /hub and a single platform/);
    assert.match(tooBigMessage(3, 1500), /mega-pr/);
  });
});
