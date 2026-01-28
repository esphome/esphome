const {
  BOT_COMMENT_MARKER,
  CODEOWNERS_MARKER,
  TOO_BIG_MARKER,
} = require('./constants');

// Generate review messages
function generateReviewMessages(finalLabels, originalLabelCount, prFiles, totalAdditions, totalDeletions, prAuthor, MAX_LABELS, TOO_BIG_THRESHOLD) {
  const messages = [];

  // Too big message
  if (finalLabels.includes('too-big')) {
    const testAdditions = prFiles
      .filter(file => file.filename.startsWith('tests/'))
      .reduce((sum, file) => sum + (file.additions || 0), 0);
    const testDeletions = prFiles
      .filter(file => file.filename.startsWith('tests/'))
      .reduce((sum, file) => sum + (file.deletions || 0), 0);
    const nonTestChanges = (totalAdditions - testAdditions) - (totalDeletions - testDeletions);

    const tooManyLabels = originalLabelCount > MAX_LABELS;
    const tooManyChanges = nonTestChanges > TOO_BIG_THRESHOLD;

    let message = `${TOO_BIG_MARKER}\n### 📦 Pull Request Size\n\n`;

    if (tooManyLabels && tooManyChanges) {
      message += `This PR is too large with ${nonTestChanges} line changes (excluding tests) and affects ${originalLabelCount} different components/areas.`;
    } else if (tooManyLabels) {
      message += `This PR affects ${originalLabelCount} different components/areas.`;
    } else {
      message += `This PR is too large with ${nonTestChanges} line changes (excluding tests).`;
    }

    message += ` Please consider breaking it down into smaller, focused PRs to make review easier and reduce the risk of conflicts.\n\n`;
    message += `For guidance on breaking down large PRs, see: https://developers.esphome.io/contributing/submitting-your-work/#how-to-approach-large-submissions`;

    messages.push(message);
  }

  // CODEOWNERS message
  if (finalLabels.includes('needs-codeowners')) {
    const message = `${CODEOWNERS_MARKER}\n### 👥 Code Ownership\n\n` +
      `Hey there @${prAuthor},\n` +
      `Thanks for submitting this pull request! Can you add yourself as a codeowner for this integration? ` +
      `This way we can notify you if a bug report for this integration is reported.\n\n` +
      `In \`__init__.py\` of the integration, please add:\n\n` +
      `\`\`\`python\nCODEOWNERS = ["@${prAuthor}"]\n\`\`\`\n\n` +
      `And run \`script/build_codeowners.py\``;

    messages.push(message);
  }

  return messages;
}

// Handle reviews
async function handleReviews(github, context, finalLabels, originalLabelCount, prFiles, totalAdditions, totalDeletions, MAX_LABELS, TOO_BIG_THRESHOLD) {
  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;
  const prAuthor = context.payload.pull_request.user.login;

  const reviewMessages = generateReviewMessages(finalLabels, originalLabelCount, prFiles, totalAdditions, totalDeletions, prAuthor, MAX_LABELS, TOO_BIG_THRESHOLD);
  const hasReviewableLabels = finalLabels.some(label =>
    ['too-big', 'needs-codeowners'].includes(label)
  );

  const { data: reviews } = await github.rest.pulls.listReviews({
    owner,
    repo,
    pull_number: pr_number
  });

  const botReviews = reviews.filter(review =>
    review.user.type === 'Bot' &&
    review.state === 'CHANGES_REQUESTED' &&
    review.body && review.body.includes(BOT_COMMENT_MARKER)
  );

  if (hasReviewableLabels) {
    const reviewBody = `${BOT_COMMENT_MARKER}\n\n${reviewMessages.join('\n\n---\n\n')}`;

    if (botReviews.length > 0) {
      // Update existing review
      await github.rest.pulls.updateReview({
        owner,
        repo,
        pull_number: pr_number,
        review_id: botReviews[0].id,
        body: reviewBody
      });
      console.log('Updated existing bot review');
    } else {
      // Create new review
      await github.rest.pulls.createReview({
        owner,
        repo,
        pull_number: pr_number,
        body: reviewBody,
        event: 'REQUEST_CHANGES'
      });
      console.log('Created new bot review');
    }
  } else if (botReviews.length > 0) {
    // Dismiss existing reviews
    for (const review of botReviews) {
      try {
        await github.rest.pulls.dismissReview({
          owner,
          repo,
          pull_number: pr_number,
          review_id: review.id,
          message: 'Review dismissed: All requirements have been met'
        });
        console.log(`Dismissed bot review ${review.id}`);
      } catch (error) {
        console.log(`Failed to dismiss review ${review.id}:`, error.message);
      }
    }
  }
}

module.exports = {
  handleReviews
};
