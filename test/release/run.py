#!/usr/bin/env python3
"""Test release notes and atomic changelog/tag updates against disposable remotes."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('release_changelog', ROOT / '.github/release_changelog.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
TEXT = '# Changelog\n\n### Unreleased\n\n- New feature\n- Fix\n\n#### Details\nMore\n\n### 3.0.1 (2026-11-13)\n\n- Old\n'


class ChangelogTests(unittest.TestCase):
    def test_stable_rollover_and_idempotence(self):
        updated = release.prepare(TEXT, '3.0.2', '2026-11-16')
        self.assertIn('### Unreleased\n\n### 3.0.2 (2026-11-16)\n\n- New feature\n- Fix', updated)
        self.assertTrue(updated.endswith('### 3.0.1 (2026-11-13)\n\n- Old\n'))
        self.assertEqual(release.prepare(updated, '3.0.2', '2026-11-17'), updated)
        self.assertEqual(release.notes(updated, '3.0.2'), '- New feature\n- Fix\n\n#### Details\nMore\n')
        self.assertNotIn('Old', release.notes(updated, '3.0.2'))

    def test_prereleases_preserve_all_entries(self):
        for suffix in ('beta', 'beta.0', 'beta.1', 'rc.1', 'testing', 'testing.2', 'alpha.1'):
            tag = '3.0.2-' + suffix
            with self.subTest(tag=tag):
                self.assertEqual(release.prepare(TEXT, tag, '2026-11-16'), TEXT)
                self.assertEqual(release.release_version(tag), (tag, True))
                self.assertNotIn('Old', release.notes(TEXT, tag))

    def test_invalid_tags_and_sections_fail(self):
        for tag in ('v3.0.2', 'v3.0.2-beta.1', 'main', '3.0', '3.0.2-rc.01', '03.0.2', '3.0.2\n', '3.0.2;echo unsafe'):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                release.release_version(tag)
        for text in ('# Changelog\n', '### Unreleased\n\n', TEXT + '\n### Unreleased\n- Duplicate\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                release.prepare(text, '3.0.2', '2026-11-16')
        with self.assertRaises(ValueError):
            release.notes(TEXT, '3.0.2')

    def test_next_cycle_does_not_repeat_old_release(self):
        stable = release.prepare(TEXT, '3.0.2', '2026-11-16')
        next_cycle = stable.replace('### Unreleased\n', '### Unreleased\n\n- Next\n', 1)
        self.assertEqual(release.notes(next_cycle, '3.0.3-beta.1'), '- Next\n')


class GitReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.remote = self.root / 'remote.git'
        self.work = self.root / 'work'
        self.git('init', '--bare', str(self.remote), cwd=self.root)
        self.git('init', '-b', 'main', str(self.work), cwd=self.root)
        self.git('config', 'user.name', 'Test')
        self.git('config', 'user.email', 'test@example.invalid')
        (self.work / '.github').mkdir()
        for name in ('prepare_tag_release.sh', 'release_changelog.py'):
            shutil.copy(ROOT / '.github' / name, self.work / '.github' / name)
        (self.work / 'CHANGELOG.md').write_text(TEXT)
        self.git('add', '.')
        self.git('commit', '-m', 'Initial')
        self.git('remote', 'add', 'origin', str(self.remote))
        self.git('push', 'origin', 'main')

    def git(self, *args, cwd=None):
        return subprocess.run(['git', *args], cwd=cwd or self.work, check=True,
                              capture_output=True, text=True).stdout.strip()

    def remote_ref(self, ref):
        return self.git('--git-dir=' + str(self.remote), 'rev-parse', ref)

    def tag(self, tag, annotated=True):
        self.git('tag', *(['-a', tag, '-m', tag] if annotated else [tag]))
        self.git('push', 'origin', tag)

    def prepare(self, tag, success=True):
        env = dict(os.environ, RELEASE_TAG=tag, DEFAULT_BRANCH='main',
                   GITHUB_OUTPUT=str(self.root / 'output'), RUNNER_TEMP=str(self.root))
        result = subprocess.run(['bash', '.github/prepare_tag_release.sh'], cwd=self.work,
                                env=env, capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_stable_updates_both_refs_and_retries_without_new_commit(self):
        self.tag('3.0.2')
        self.prepare('3.0.2')
        head = self.git('rev-parse', 'HEAD')
        self.assertEqual(self.remote_ref('refs/heads/main'), head)
        self.assertEqual(self.remote_ref('refs/tags/3.0.2'), head)
        updated = (self.work / 'CHANGELOG.md').read_text()
        self.assertIn('### Unreleased\n\n### 3.0.2 (', updated)
        output = (self.root / 'output').read_text()
        self.assertIn('prerelease=false', output)
        self.assertIn('commit=' + head, output)
        self.assertEqual((self.root / 'release-notes.md').read_text(), release.notes(updated, '3.0.2'))
        self.prepare('3.0.2')
        self.assertEqual(self.git('rev-parse', 'HEAD'), head)

    def test_beta_does_not_move_tag_or_commit_changelog(self):
        self.tag('3.0.2-beta.1')
        before = self.remote_ref('refs/tags/3.0.2-beta.1')
        self.prepare('3.0.2-beta.1')
        self.assertEqual(self.remote_ref('refs/tags/3.0.2-beta.1'), before)
        self.assertEqual((self.work / 'CHANGELOG.md').read_text(), TEXT)
        self.assertIn('prerelease=true', (self.root / 'output').read_text())

    def test_old_tag_cannot_overwrite_newer_work(self):
        self.tag('3.0.2', annotated=False)
        self.git('commit', '--allow-empty', '-m', 'New work')
        self.git('push', 'origin', 'main')
        before = self.remote_ref('refs/heads/main')
        self.prepare('3.0.2', success=False)
        self.assertEqual(self.remote_ref('refs/heads/main'), before)

    def test_rejected_atomic_push_changes_neither_ref(self):
        self.tag('3.0.2')
        before_tag = self.remote_ref('refs/tags/3.0.2')
        before_branch = self.remote_ref('refs/heads/main')
        hook = self.remote / 'hooks/pre-receive'
        hook.write_text('#!/bin/sh\nexit 1\n')
        hook.chmod(0o755)
        self.prepare('3.0.2', success=False)
        self.assertEqual(self.remote_ref('refs/heads/main'), before_branch)
        self.assertEqual(self.remote_ref('refs/tags/3.0.2'), before_tag)
        self.assertFalse((self.root / 'output').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
