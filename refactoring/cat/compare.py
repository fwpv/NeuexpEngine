#!/usr/bin/env python3
"""Сравнение диагностических копий; исходники проекта не инструментируются."""
import argparse
import pathlib
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('original', type=pathlib.Path)
parser.add_argument('candidate', type=pathlib.Path)
parser.add_argument('--render', action='store_true', help='Read native OpenGL back buffers; needs X11 and built engine libraries')
args = parser.parse_args()
harness = pathlib.Path(__file__).with_name('render.cpp' if args.render else 'snapshot.cpp')
with tempfile.TemporaryDirectory(prefix='cat-equivalence-') as temporary:
    outputs = []
    for label, root in [('original', args.original.resolve()), ('candidate', args.candidate.resolve())]:
        folder = pathlib.Path(temporary) / label
        folder.mkdir()
        for source in (root / 'demos/cat').iterdir():
            if source.suffix in ('.h', '.cpp'):
                text = source.read_text().replace('private:', 'public:')
                (folder / source.name).write_text(text)
        modern = (folder / 'world.h').exists()
        sources = [folder / name for name in ('cat.cpp', 'game.cpp', 'graphics.cpp')]
        if modern:
            sources.append(folder / 'world.cpp')
        executable = folder / 'snapshot'
        # Удаление неиспользованных секций исключает нативную графику из диагностики состояния.
        command = ['c++', '-std=c++17', '-O2', '-ffunction-sections', '-fdata-sections',
                   '-Wl,--gc-sections', '-I' + str(folder), '-I' + str(root / 'engine')]
        if modern:
            command.append('-DREFACTORED')
        command += [str(harness), *map(str, sources)]
        if args.render:
            shutil.copytree(root / 'assets', folder / 'assets')
            libraries = ['engine/libengine_renderer.a', 'engine/libengine_ui.a',
                         'engine/libengine_camera.a', 'engine/libengine_resources.a',
                         'platform/libplatform.a', 'platform/libengine_input.a',
                         'engine/libengine_math.a', 'libstb_image.a']
            command += ['-Wl,--start-group', *[str(root / 'build' / lib) for lib in libraries],
                        '-Wl,--end-group', '-lGL', '-lX11']
        else:
            command += [str(root / 'engine/math2d.cpp'),
                    str(root / 'engine/geometry.cpp'), str(root / 'engine/transform.cpp'),
                    str(root / 'engine/camera.cpp')]
        command += ['-o', str(executable)]
        subprocess.run(command, check=True)
        output = folder / 'snapshot.txt'
        with output.open('wb') as stream:
            subprocess.run([str(executable)], stdout=stream, check=True, timeout=120)
        outputs.append(output.read_bytes())
    if outputs[0] != outputs[1]:
        if args.render:
            raise SystemExit('Native rendered pixels differ')
        first = outputs[0].splitlines()
        second = outputs[1].splitlines()
        for index, (before, after) in enumerate(zip(first, second)):
            if before != after:
                raise SystemExit(f'Mismatch at line {index + 1}:\n{before!r}\n{after!r}')
        raise SystemExit('Snapshot lengths differ')
    if args.render:
        print(f'Identical: 36 native frames, {len(outputs[0])} RGBA bytes; zero pixel tolerance')
        raise SystemExit(0)
    print(f'Identical: {len(outputs[0].splitlines())} snapshots, {len(outputs[0])} bytes; exact hexfloat comparison')
