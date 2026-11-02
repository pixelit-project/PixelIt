#!/usr/bin/env python3
"""Check the firmware's actual release version comparator on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/System.cpp').read_text()
source = source[source.index('Version parseVersion'):]
program = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "Version.h"
''' + source + r'''
int main() {
    assert(compareVersions("2.7.0-beta.10", "2.7.0-beta.2") > 0);
    assert(compareVersions("2.7.0-beta.2", "2.7.0-beta.10") < 0);
    assert(compareVersions("2.7.0", "2.7.0-beta.10") > 0);
    assert(compareVersions("2.7.0-beta.1", "2.6.0") > 0);
    assert(compareVersions("v2.7.0-beta.1", "2.7.0-beta.1") == 0);
    assert(compareVersions("2.7.0-beta.1", "2.7.0-beta.1.1") < 0);
    assert(compareVersions("2.7.0-beta.1+abc", "2.7.0-beta.1+xyz") == 0);
    assert(compareVersions("2.7.0-1", "2.7.0-alpha") < 0);
    assert(compareVersions("", "2.7.0") == 0);
    const Version longVersion = parseVersion("2.7.0-beta.123456789012345678901234567890");
    assert(longVersion.prerelease[15] == '\0');
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'versions.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-I', str(root / 'include'),
                    str(path / 'versions.cpp'), '-o', str(path / 'versions')], check=True)
    subprocess.run([str(path / 'versions')], check=True)
print('Firmware release version comparison tests passed')
