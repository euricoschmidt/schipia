#!/usr/bin/env python3
"""Use only the files delivered in a public Schip binary kit."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parent
META = json.loads((ROOT / 'release.json').read_text())
BUILD = ROOT / 'build'
ENV = dict(os.environ, LD_LIBRARY_PATH=str(ROOT / 'lib'))
for key in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH'):
    ENV.pop(key, None)


def run(args, cwd=ROOT, **kwargs):
    subprocess.run(list(map(str, args)), cwd=cwd, env=ENV, check=True, **kwargs)


def verify():
    for line in (ROOT / 'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ', 1)
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError('Checksum diferente: ' + name)
    print('OK: integridade da distribuição.', flush=True)


def build():
    verify()
    kind = META['kind']
    if kind in ('libschip', 'schipmyserver'):
        run(['make', '-C', 'examples', 'all', 'CC=' + os.environ.get('CC', 'cc')])
        return
    dirs = ('examples', 'exemplos')
    libs = ['-lschiptree', '-lpthread'] if kind == 'libschiptree' else ['-lschipbase', '-lssl', '-lcrypto', '-lz', '-lpthread']
    for directory in dirs:
        for src in sorted((ROOT / directory).glob('*.c')):
            output = BUILD / 'examples' / src.stem
            output.parent.mkdir(parents=True, exist_ok=True)
            run([os.environ.get('CC', 'cc'), '-O2', '-Wall', '-Wextra', '-Wno-missing-field-initializers',
                 '-I' + str(ROOT / 'include'), src, '-L' + str(ROOT / 'lib'),
                 '-Wl,-rpath,' + str(ROOT / 'lib'), *libs, '-o', output])


def server_check():
    data = BUILD / 'example-data'
    shutil.rmtree(data, ignore_errors=True)
    data.mkdir(parents=True, exist_ok=True)
    env = dict(ENV, SCHIP_DBROOT=str(data))
    subprocess.run([str(ROOT / 'bin/myimport'), str(ROOT / 'examples/loja.sql'), 'loja'], cwd=data, env=env, check=True)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    with (BUILD / 'server.log').open('w') as log:
        server = subprocess.Popen([str(ROOT / 'bin/schipmyserverd'), '-p', str(port), '-s', 'segredo', '-d', 'loja', '-w', '1'],
                                  cwd=data, env=env, stdout=log, stderr=log, start_new_session=True)
        try:
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError('Servidor do exemplo encerrou; veja build/server.log')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.1):
                        break
                except OSError:
                    time.sleep(.05)
            else:
                raise RuntimeError('Servidor do exemplo não ficou pronto')
            client_env = dict(ENV, SMY_HOST='127.0.0.1', SMY_PORT=str(port), SMY_PASS='segredo', SMY_DB='loja')
            for src in sorted((ROOT / 'examples').glob('[0-9]*.c')):
                subprocess.run([str(src.with_suffix(''))], cwd=data, env=client_env, check=True, timeout=30)
        finally:
            if server.poll() is None:
                os.killpg(server.pid, signal.SIGTERM)
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(server.pid, signal.SIGKILL)
                    server.wait()


def check():
    build()
    kind = META['kind']
    if kind == 'libschip':
        run(['make', '-C', 'examples', 'check', 'CC=' + os.environ.get('CC', 'cc')])
    elif kind == 'schipmyserver':
        server_check()
    elif kind == 'libschiptree':
        demo = BUILD / 'demo'
        demo.mkdir(exist_ok=True)
        for name in ('exemplo_basico', 'exemplo_duplicatas'):
            run([BUILD / 'examples' / name], cwd=demo, timeout=30)
    else:
        for src in sorted((ROOT / 'tests').glob('test_*.c')):
            if src.name.startswith('test_https_'):
                continue
            output = BUILD / src.stem
            run([os.environ.get('CC', 'cc'), '-O2', '-I' + str(ROOT / 'include'), src,
                 '-L' + str(ROOT / 'lib'), '-Wl,-rpath,' + str(ROOT / 'lib'),
                 '-lschipbase', '-lssl', '-lcrypto', '-lz', '-lpthread', '-o', output])
            run([output], timeout=60)
        for binary in sorted((BUILD / 'examples').glob('*')):
            if binary.name.startswith('json_') or binary.name == 'hello':
                run([binary], timeout=30)
    print('OK: exemplos compilados e testes executados somente com este kit.', flush=True)


def tls():
    verify()
    BUILD.mkdir(exist_ok=True)
    for src in sorted((ROOT / 'tests').glob('test_https_*.c')):
        run([os.environ.get('CC', 'cc'), '-O2', '-I' + str(ROOT / 'include'), src,
             '-L' + str(ROOT / 'lib'), '-Wl,-rpath,' + str(ROOT / 'lib'),
             '-lschipbase', '-lssl', '-lcrypto', '-lz', '-lpthread', '-o', BUILD / src.stem])
    run(['bash', ROOT / 'tests/test_https.sh'], timeout=60)


def clean():
    if META['kind'] in ('libschip', 'schipmyserver'):
        run(['make', '-C', 'examples', 'clean'])
    shutil.rmtree(BUILD, ignore_errors=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('build', 'check', 'verify', 'clean', 'tls'))
    args = parser.parse_args()
    {'build': build, 'check': check, 'verify': verify, 'clean': clean, 'tls': tls}[args.action]()
