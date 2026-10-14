// Apply labels to PR
async function applyLabels(github, context, finalLabels) {
  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;

  if (finalLabels.length > 0) {
    console.log(`Adding labels: ${finalLabels.join(', ')}`);
    await github.rest.issues.addLabels({
      owner,
      repo,
      issue_number: pr_number,
      labels: finalLabels
    });
  }
}

// Remove old managed labels
async function removeOldLabels(github, context, managedLabels, finalLabels) {
  const { owner, repo } = context.repo;
  const pr_number = context.issue.number;

  const labelsToRemove = managedLabels.filter(label => !finalLabels.includes(label));
  for (const label of labelsToRemove) {
    console.log(`Removing label: ${label}`);
    try {
      await github.rest.issues.removeLabel({
        owner,
        repo,
        issue_number: pr_number,
        name: label
      });
    } catch (error) {
      console.log(`Failed to remove label ${label}:`, error.message);
    }
  }
}

module.exports = {
  applyLabels,
  removeOldLabels
};
