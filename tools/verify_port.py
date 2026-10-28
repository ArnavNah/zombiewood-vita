"""Check binary imports and VPK integrity. This does not establish playability."""
import argparse
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path


def elf_symbols(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Expected a little-endian ELF32 image')
    if struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError('Expected an ARM image')
    shoff = struct.unpack_from('<I', data, 32)[0]
    entsize, count = struct.unpack_from('<HH', data, 46)
    sections = [struct.unpack_from('<10I', data, shoff + i * entsize) for i in range(count)]
    imports, exports = set(), set()
    for section in sections:
        if section[1] != 11:  # SHT_DYNSYM
            continue
        strings = sections[section[6]]
        table = data[strings[4]:strings[4] + strings[5]]
        for off in range(section[4], section[4] + section[5], section[9]):
            name, value, size, info, other, index = struct.unpack_from('<IIIBBH', data, off)
            text = table[name:table.index(b'\0', name)].decode('utf-8')
            if text:
                (exports if index else imports).add(text)
    return imports, exports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--vpk', type=Path, required=True)
    parser.add_argument('--eboot', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    imports, exports = elf_symbols(args.data / 'libZombieWood.so')
    bridges = set(re.findall(r'\{\s*"([^"]+)"', (repo / 'source/dynlib.c').read_text()))
    missing = sorted(imports - bridges)
    if missing:
        raise ValueError(f'Unmapped Android imports: {missing}')
    required = {'eboot.bin', 'sce_sys/param.sfo', 'sce_sys/icon0.png',
                'sce_sys/livearea/contents/template.xml', 'cpuinfo', 'meminfo', 'urandom'}
    with zipfile.ZipFile(args.vpk) as archive:
        if archive.testzip() is not None:
            raise ValueError('VPK ZIP CRC check failed')
        if not required.issubset(archive.namelist()):
            raise ValueError('VPK is missing required entries')
        if archive.read('eboot.bin') != args.eboot.read_bytes():
            raise ValueError('VPK contains a stale executable')
        if b'ZOMBW0001' not in archive.read('sce_sys/param.sfo'):
            raise ValueError('Unexpected Vita title ID')
    assets = {}
    for path in sorted(args.data.glob('*.obb')):
        with zipfile.ZipFile(path) as archive:
            if archive.testzip() is not None:
                raise ValueError(f'Asset archive CRC failed: {path.name}')
            assets[path.name] = len(archive.namelist())
    result = {'android_imports_mapped_by_name': len(imports),
              'android_exports': len(exports), 'missing_import_names': missing,
              'vpk_crc': 'pass', 'vpk_eboot_matches_build': True,
              'vpk_sha256': hashlib.sha256(args.vpk.read_bytes()).hexdigest(),
              'obb_entries_crc_checked': assets, 'playability': 'NOT VERIFIED'}
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
