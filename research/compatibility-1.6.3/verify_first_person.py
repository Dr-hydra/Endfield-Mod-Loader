"""Audit first-person descriptors against selected classes in an installed client.

Uses BE's existing read-only PE/IL2CPP metadata extractor. This checks signatures,
nonzero entry points and field layouts; it does not launch the game or establish
that the native hooks, icalls, camera and mesh effects work in a live session.
No full metadata dump or artifact hashes are produced.
"""
from __future__ import annotations

import argparse
import collections
import importlib.util
import json
import re
from pathlib import Path

METHOD = re.compile(r'\{\s*"([^"\n]+\.dll)"\s*,\s*"([^"\n]*)"\s*,\s*"([^"\n]+)"\s*,\s*"([^"\n]+)"\s*,\s*(nullptr|"[^"\n]*")\s*,\s*(nullptr|"[^"\n]*")\s*,\s*(\d+)\s*\}', re.S)
FIELD = re.compile(r'\{\s*"([^"\n]+\.dll)"\s*,\s*"([^"\n]*)"\s*,\s*"([^"\n]+)"\s*,\s*"([^"\n]+)"\s*,\s*(nullptr|"[^"\n]*")\s*\}', re.S)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--be-root', required=True, type=Path)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--platform', choices=('windows', 'android'), default='windows')
    parser.add_argument('--metadata', type=Path, help='Existing targeted metadata JSON, including decoded Android field offsets')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    extractor = args.be_root / 'tools/HookInlineScan/extract_targeted_metadata.py'
    spec = importlib.util.spec_from_file_location('be_targeted_metadata', extractor)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    descriptors = []
    selected = {'Vector3', 'BoneWeight', 'AsyncGPUReadbackRequest', 'SubMeshDescriptor',
                'SnapshotCameraController', 'Animator', 'SkinnedMeshRenderer', 'GameObject'}
    for path in sorted((root / 'examples/first-person/native').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.inc'):
            continue
        source = path.read_text(encoding='utf-8')
        for kind, pattern in (('method', METHOD), ('field', FIELD)):
            for match in pattern.finditer(source):
                asm, namespace, short, name, *rest = match.groups()
                full = (namespace + '.' if namespace else '') + short.replace('/', '.').replace('+', '.')
                key_match = re.search(r'\{\s*"([^"\n]+)"\s*,\s*$', source[:match.start()])
                descriptors.append(dict(kind=kind, source=path.relative_to(root).as_posix(),
                    line=source.count('\n', 0, match.start()) + 1, key=key_match[1] if key_match else None,
                    asm=asm, cls=full, name=name, expected=rest))
                selected.add(re.split(r'[./+]', short)[-1])

    if args.metadata:
        extracted = json.loads(args.metadata.read_text(encoding='utf-8'))
        method_rows, class_rows = extracted['methods'], extracted['classes']
    else:
        if args.platform == 'android':
            parser.error('Android requires --metadata from the Android targeted extractor')
        pe = module.Pe(args.game / 'GameAssembly.dll')
        metadata = module.Metadata(args.game / 'Endfield_Data/il2cpp_data/Metadata/global-metadata.dat', pe)
        method_rows, class_rows = metadata.extract(selected, include_ifix=False)
    methods = collections.defaultdict(list)
    for method in method_rows:
        methods[method['asm'], method['cls'], method['name'], len(method['types'])].append(method)
    classes = {(row['asm'], row['cls']): row for row in class_rows}
    failures = []
    for record in descriptors:
        asm, full, name = (record[key] for key in ('asm', 'cls', 'name'))
        if record['kind'] == 'method':
            params, ret, count = record['expected']
            candidates = methods.get((asm, full, name, int(count)), [])
            expected = params.strip('"').split('|') if params.strip('"') else []
            exact = [row for row in candidates
                     if (params == 'nullptr' or row['types'] == expected)
                     and (ret == 'nullptr' or row['ret'] == ret.strip('"'))]
            record['status'] = ('matched' if exact and all(row['rva'] for row in exact)
                                else 'zero-entry' if exact else 'signature-mismatch' if candidates else 'missing')
            if exact:
                record['rvas'] = [hex(row['rva']) for row in exact]
            elif candidates:
                record['candidates'] = [dict(types=row['types'], ret=row['ret']) for row in candidates]
            # Both are explicitly optional and use GetIndices(submesh, true) when stripped.
            if record['status'] == 'missing' and record['key'] in ('fp.mesh.submesh', 'fp.mesh.index_start'):
                record['status'] = 'optional-missing-with-indices-fallback'
        else:
            fields = classes.get((asm, full), {}).get('fields', [])
            found = [row for row in fields if row['name'] == name]
            record['status'] = ('matched' if found and
                (record['expected'][0] == 'nullptr' or found[0]['type'] == record['expected'][0].strip('"'))
                else 'field-mismatch')
            if found:
                record['offset'] = found[0]['offset']
        if record['status'] not in ('matched', 'optional-missing-with-indices-fallback'):
            failures.append(record)

    layout_checks = []
    layouts = {
        'UnityEngine.Vector3': {'x': 16, 'y': 20, 'z': 24},
        'UnityEngine.BoneWeight': {**{f'm_Weight{i}': 16+i*4 for i in range(4)},
                                  **{f'm_BoneIndex{i}': 32+i*4 for i in range(4)}},
        'UnityEngine.Rendering.AsyncGPUReadbackRequest': {'m_Ptr': 16, 'm_Version': 24},
        'UnityEngine.Rendering.SubMeshDescriptor': dict(zip(
            ('<bounds>k__BackingField', '<topology>k__BackingField', '<indexStart>k__BackingField',
             '<indexCount>k__BackingField', '<baseVertex>k__BackingField', '<firstVertex>k__BackingField',
             '<vertexCount>k__BackingField'), (16, 40, 44, 48, 52, 56, 60))),
    }
    for full, expected in layouts.items():
        actual = {row['name']: row['offset'] for row in classes.get(('UnityEngine.CoreModule.dll', full), {}).get('fields', [])}
        for name, offset in expected.items():
            check = dict(cls=full, name=name, expected=offset, actual=actual.get(name), matched=actual.get(name) == offset)
            layout_checks.append(check)
            if not check['matched']:
                failures.append(check)

    source = {}
    inputs = (('libil2cpp.so', 'global-metadata.dat') if args.platform == 'android'
              else ('GameAssembly.dll', 'Endfield_Data/il2cpp_data/Metadata/global-metadata.dat'))
    for name in inputs:
        stat = (args.game / name).stat()
        source[name] = dict(bytes=stat.st_size, mtime_ns=stat.st_mtime_ns)
    report = dict(game=str(args.game.resolve()), platform=args.platform, input_files=source,
        scope='Static descriptor and field audit only; no game launch, icall lookup or full dump',
        selected_classes=len(class_rows), selected_methods=len(method_rows),
        summary=dict(collections.Counter(f"{row['kind']}:{row['status']}" for row in descriptors)),
        descriptors=descriptors, native_layouts=layout_checks, failures=failures)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(output=str(args.output), summary=report['summary'],
                         layout_fields=len(layout_checks), failures=len(failures)), ensure_ascii=False))
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
