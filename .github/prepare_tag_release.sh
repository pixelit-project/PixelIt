#!/usr/bin/env bash
# Called only in an authenticated Actions checkout; test with local remotes.
set -euo pipefail
: "${RELEASE_TAG:?}"
: "${DEFAULT_BRANCH:?}"
: "${GITHUB_OUTPUT:?}"
: "${RUNNER_TEMP:?}"

# Validate before fetching or changing any reference.
prerelease=$(python3 .github/release_changelog.py prerelease "$RELEASE_TAG")
git check-ref-format "refs/tags/$RELEASE_TAG"
git check-ref-format "refs/heads/$DEFAULT_BRANCH"
git fetch --no-tags origin \
  "+refs/heads/$DEFAULT_BRANCH:refs/remotes/origin/$DEFAULT_BRANCH" \
  "+refs/tags/$RELEASE_TAG:refs/tags/$RELEASE_TAG"
old_tag=$(git rev-parse "refs/tags/$RELEASE_TAG")
tag_commit=$(git rev-parse "refs/tags/$RELEASE_TAG^{commit}")
branch_commit=$(git rev-parse "refs/remotes/origin/$DEFAULT_BRANCH")
if [[ "$tag_commit" != "$branch_commit" ]]; then
  echo '::error::Release tags must point to the current default branch tip. Pull the branch and create a new version tag.'
  exit 1
fi
git checkout --detach "$tag_commit"
python3 .github/release_changelog.py prepare "$RELEASE_TAG" --date "$(date -u +%F)"
python3 .github/release_changelog.py notes "$RELEASE_TAG" --output "$RUNNER_TEMP/release-notes.md"
if ! git diff --quiet -- CHANGELOG.md; then
  git config user.name 'github-actions[bot]'
  git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
  git add CHANGELOG.md
  git commit -m "Prepare release $RELEASE_TAG"
  # Update both refs together; reject concurrent branch or tag changes.
  git push --atomic \
    "--force-with-lease=refs/heads/$DEFAULT_BRANCH:$branch_commit" \
    "--force-with-lease=refs/tags/$RELEASE_TAG:$old_tag" \
    origin "HEAD:refs/heads/$DEFAULT_BRANCH" "HEAD:refs/tags/$RELEASE_TAG"
fi
printf 'commit=%s\nprerelease=%s\n' "$(git rev-parse HEAD)" "$prerelease" >> "$GITHUB_OUTPUT"
