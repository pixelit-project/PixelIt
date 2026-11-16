"""Finalize PixelIt's changelog and extract curated GitHub release notes."""

import argparse
from datetime import date
from pathlib import Path
import re


def release_version(tag):
    """Accept unprefixed SemVer tags, including beta/RC/testing conventions."""
    match = re.fullmatch(
        r'((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))'
        r'(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?', tag)
    if not match:
        raise ValueError('Unsupported release tag: ' + repr(tag))
    base, suffix = match.groups()
    if suffix and any(part.isdigit() and len(part) > 1 and part.startswith('0')
                      for part in suffix.split('.')):
        raise ValueError('Leading zero in prerelease number: ' + repr(tag))
    return base + ('-' + suffix if suffix else ''), suffix is not None


def section(text, title):
    """Find exactly one level-three version heading, retaining nested headings."""
    headings = list(re.finditer(r'^### ([^\n]+)\n', text, re.MULTILINE))
    matches = [(index, heading) for index, heading in enumerate(headings)
               if heading[1].strip() == title
               or re.fullmatch(re.escape(title) + r' \(\d{4}-\d{2}-\d{2}\)', heading[1].strip())]
    if len(matches) != 1:
        raise ValueError('Expected exactly one changelog section for ' + repr(title))
    index, heading = matches[0]
    end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
    body = text[heading.end():end].strip()
    if not body:
        raise ValueError('Empty changelog section: ' + title)
    return heading.start(), end, body


def notes(text, tag):
    version, prerelease = release_version(tag)
    return section(text, 'Unreleased' if prerelease else version)[2] + '\n'


def prepare(text, tag, released_on):
    version, prerelease = release_version(tag)
    if prerelease:
        notes(text, tag)
        return text
    date.fromisoformat(released_on)
    if re.search(r'^### ' + re.escape(version) + r'(?: \(\d{4}-\d{2}-\d{2}\))?$', text, re.MULTILINE):
        notes(text, tag)
        return text
    start, end, body = section(text, 'Unreleased')
    return (text[:start] + '### Unreleased\n\n### ' + version + ' (' + released_on
            + ')\n\n' + body + '\n\n' + text[end:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('prepare', 'notes', 'prerelease'))
    parser.add_argument('tag')
    parser.add_argument('--date', default=date.today().isoformat())
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    _, prerelease = release_version(args.tag)
    if args.command == 'prerelease':
        print(str(prerelease).lower())
        return
    path = Path('CHANGELOG.md')
    text = path.read_text(encoding='utf-8')
    if args.command == 'prepare':
        path.write_text(prepare(text, args.tag, args.date), encoding='utf-8')
    else:
        if args.output is None:
            parser.error('notes requires --output')
        args.output.write_text(notes(text, args.tag), encoding='utf-8')


if __name__ == '__main__':
    main()
