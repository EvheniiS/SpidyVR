"""Read the game's config assets offline, from its asset archive.

  python tools/game_configs.py list swingsetup       asset paths matching a regex
  python tools/game_configs.py dump configs/hero/hero_swingsetup_standard.config

`dump` prints the config's values as JSON. Needs the `lz4` package. The game
folder is Steam's default library's, or given with --game.

Formats (Spider-Man Remastered, build 23986256):
- asset_archive/toc and dag: zlib'd DAT1 after a short header (toc: magic,
  size; dag: three words). DAT1: 'DAT1' magic, type, size, section count,
  then (tag, offset, size) per section, offsets from the DAT1 start.
- toc sections: 0x398abff0 archive files (72 bytes: 8, then the name),
  0x506d7b8a asset ids (u64, sorted), 0x65bcf461 sizes (u32 1, size, u32),
  0xdcd720b5 locations (archive index, offset).
- dag sections: 0xd101a6cc name offsets (u32). The dag's ids are not the
  toc's: a toc id is CRC-64 (ECMA, reflected, seeded with its own polynomial
  0xc96c5795d7870f42, no final xor) of the lower-case path with forward
  slashes, shifted right 2 with bit 63 set.
- archives: 'DSAR', version, block count, header size, total size; 32-byte
  blocks from 0x20 (real offset, stored offset, real size, stored size,
  compression; 3 is LZ4 block). Toc offsets are into the real stream.
- a config: DAT1 with sections 0x4a128222 (its type) and 0xe501186f (its
  values). A value node: magic 0x03150044, field count, size (from after
  these three words), then per field (name hash, kind << 24 | count << 4:
  0x10 one value, 0x30 three), then name offsets from the DAT1 start, then
  the values in field order: kinds 0-7 u8 u16 u32 u64 i8 i16 i32 i64,
  8 float, 9 double, 0xa string (length, hash 4, hash 8, text, NUL,
  4-aligned), 0xd an object (a zero word, then its node), 0xf bool, 0x11
  8 bytes (shown as u64), 0x13 one byte even with a count of 0 (shown as
  u8). These sizes leave every node of all 1890 configs in the archive
  exactly its declared size. Fields come sorted with objects first; whole
  numbers are stored in the smallest kind that holds them. Fields left at
  their compiled defaults are absent.
"""
import argparse
import json
import os
import re
import struct
import sys
import zlib

TOC_ARCHIVES, TOC_IDS, TOC_SIZES, TOC_PLACES = 0x398ABFF0, 0x506D7B8A, 0x65BCF461, 0xDCD720B5
DAG_NAMES = 0xD101A6CC
CONFIG_VALUES = 0xE501186F
NODE = 0x03150044
SCALARS = {0: 'B', 1: 'H', 2: 'I', 3: 'Q', 4: 'b', 5: 'h', 6: 'i', 7: 'q', 8: 'f', 9: 'd', 0xF: '?', 0x11: 'Q',
           0x13: 'B'}
POLY = 0xC96C5795D7870F42
TABLE = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ POLY if _c & 1 else _c >> 1
    TABLE.append(_c)
DEFAULT_GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered"


def asset_id(path):
    c = POLY
    for b in path.lower().replace('\\', '/').encode():
        c = TABLE[(c ^ b) & 0xFF] ^ (c >> 8)
    return (c >> 2) | 1 << 63


def dat1(path):
    raw = open(path, 'rb').read()
    data = zlib.decompressobj().decompress(raw[8:] if raw[8] == 0x78 else raw[12:])
    count = struct.unpack_from('<I', data, 12)[0]
    sections = {}
    for i in range(count):
        tag, offset, size = struct.unpack_from('<III', data, 16 + 12 * i)
        sections[tag] = (offset, size)
    return data, sections


class Archive:
    def __init__(self, game):
        self.folder = os.path.join(game, 'asset_archive')
        self.toc, s = dat1(os.path.join(self.folder, 'toc'))
        offset, size = s[TOC_IDS]
        ids = struct.unpack_from('<%dQ' % (size // 8), self.toc, offset)
        self.index = {v: i for i, v in enumerate(ids)}
        self.sizes = s[TOC_SIZES][0]
        self.places = s[TOC_PLACES][0]
        offset, size = s[TOC_ARCHIVES]
        self.archives = [self.toc[offset + 72 * i + 8:offset + 72 * (i + 1)].split(b'\0')[0].decode()
                         for i in range(size // 72)]
        self.blocks = {}

    def names(self):
        data, s = dat1(os.path.join(self.folder, 'dag'))
        offset, size = s[DAG_NAMES]
        for at in struct.unpack_from('<%dI' % (size // 4), data, offset):
            yield data[at:data.index(b'\0', at)].decode('latin1')

    def _blocks(self, path):
        if path not in self.blocks:
            with open(path, 'rb') as f:
                head = f.read(32)
                if head[:4] != b'DSAR':
                    self.blocks[path] = None
                else:
                    count = struct.unpack_from('<I', head, 8)[0]
                    table = f.read(32 * count)
                    self.blocks[path] = [struct.unpack_from('<QQIIB', table, 32 * i) for i in range(count)]
        return self.blocks[path]

    def read(self, asset_path):
        i = self.index.get(asset_id(asset_path))
        if i is None:
            return None
        size = struct.unpack_from('<III', self.toc, self.sizes + 12 * i)[1]
        archive, offset = struct.unpack_from('<II', self.toc, self.places + 8 * i)
        path = os.path.join(self.folder, self.archives[archive])
        blocks = self._blocks(path)
        with open(path, 'rb') as f:
            if blocks is None:
                f.seek(offset)
                return f.read(size)
            import lz4.block
            out = bytearray()
            for real, stored, real_size, stored_size, kind in blocks:
                if real + real_size <= offset or real >= offset + size:
                    continue
                f.seek(stored)
                block = f.read(stored_size)
                if kind == 3:
                    block = lz4.block.decompress(block, uncompressed_size=real_size)
                elif kind:
                    raise ValueError('compression %d' % kind)
                start, end = max(offset, real), min(offset + size, real + real_size)
                out += block[start - real:end - real]
            return bytes(out)


def config_values(asset):
    base = asset.find(b'1TAD')
    count = struct.unpack_from('<I', asset, base + 12)[0]
    sections = {}
    for i in range(count):
        tag, offset, size = struct.unpack_from('<III', asset, base + 16 + 12 * i)
        sections[tag] = base + offset

    def name(at):
        return asset[base + at:asset.index(b'\0', base + at)].decode('latin1')

    def node(at):
        magic, fields, size = struct.unpack_from('<III', asset, at)
        if magic != NODE:
            raise ValueError('no value node at 0x%x' % at)
        p = at + 12
        words = [struct.unpack_from('<II', asset, p + 8 * i)[1] for i in range(fields)]
        p += 8 * fields
        names = [name(struct.unpack_from('<I', asset, p + 4 * i)[0]) for i in range(fields)]
        p += 4 * fields
        out = {}
        for word, key in zip(words, names):
            kind, count = word >> 24, (word & 0xFFFFFF) >> 4
            if kind == 0x13 and not count:
                count = 1
            items = []
            for _ in range(count):
                if kind == 0xD:
                    value, p = node(p + 4)
                elif kind == 0xA:
                    p = (p + 3) // 4 * 4
                    length = struct.unpack_from('<I', asset, p)[0]
                    value = asset[p + 16:p + 16 + length].decode('latin1')
                    p = (p + 16 + length + 1 + 3) // 4 * 4
                elif kind in SCALARS:
                    fmt = SCALARS[kind]
                    value = struct.unpack_from('<' + fmt, asset, p)[0]
                    p += struct.calcsize(fmt)
                    value = round(value, 5) if fmt in 'fd' else value
                else:
                    raise ValueError('unknown kind 0x%x (%s) at 0x%x' % (kind, key, p))
                items.append(value)
            out[key] = items[0] if (word & 0xFFFFFF) in (0, 0x10) else items
        return out, at + 12 + size

    return node(asset.find(struct.pack('<I', NODE), sections[CONFIG_VALUES]))[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('command', choices=['list', 'dump'])
    parser.add_argument('target')
    parser.add_argument('--game', default=DEFAULT_GAME, help="the game's folder")
    args = parser.parse_args()
    if not os.path.isdir(os.path.join(args.game, 'asset_archive')):
        sys.exit('Game folder not found; pass --game')
    archive = Archive(args.game)
    if args.command == 'list':
        pattern = re.compile(args.target, re.I)
        for path in sorted(set(archive.names())):
            if pattern.search(path):
                print(path)
        return
    asset = archive.read(args.target)
    if asset is None:
        sys.exit('Not in the archive: ' + args.target)
    print(json.dumps(config_values(asset), indent=1))


if __name__ == '__main__':
    main()
