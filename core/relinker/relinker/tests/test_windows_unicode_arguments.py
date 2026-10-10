"""The relinker reads input and output paths outside the Windows ANSI code page without loss."""

from pathlib import Path
import subprocess
import sys
import tempfile

from test_optional_plt import fixture
from test_windows_import_modules import executable, provider

NAMES = ('ü', '日本', 'عربي', 'ü日')


def run(relinker, *arguments):
    return subprocess.run([str(relinker), *arguments], capture_output=True, encoding='utf-8', timeout=20)


def check_plain(relinker, work, name):
    source = work / ('input-' + name) / 'game' / 'input.elf'
    source.parent.mkdir(parents=True)
    source.write_bytes(fixture())
    output = work / ('output-' + name) / 'game' / 'output.exe'
    output.parent.mkdir(parents=True)
    result = run(relinker, '--skip-sce-module', '--windows', str(source), str(output))
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    assert output.read_bytes()[:2] == b'MZ', name
    assert str(output) in result.stdout, (name, result.stdout)


def check_guest_modules(relinker, work, name):
    case = work / ('modules-' + name) / 'game'
    (case / 'prx').mkdir(parents=True)
    (case / 'prx' / 'a.prx').write_bytes(provider(0))
    source = case / 'input.elf'
    source.write_bytes(executable('a.prx'))
    output = case / 'output.exe'
    result = run(relinker, '--windows', str(source), str(output))
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    assert output.read_bytes()[:2] == b'MZ', name
    assert (case / 'app0' / 'prx' / 'a.prx.guest.prx').is_file(), (name, result.stdout)


def check_failure_text(relinker, work, name):
    missing = work / ('missing-' + name) / 'input.elf'
    output = work / ('missing-output-' + name) / 'output.exe'
    result = run(relinker, '--skip-sce-module', str(missing), str(output))
    assert result.returncode == 2 and 'Cannot open file: ' + str(missing) in result.stderr, (name, result)
    assert not output.exists(), name


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-unicode-arguments-') as directory:
        work = Path(directory)
        for name in NAMES:
            check_plain(relinker, work, name)
            check_guest_modules(relinker, work, name)
            check_failure_text(relinker, work, name)
    print('Unicode argument tests passed')


if __name__ == '__main__':
    main()
