#!/usr/bin/env python3
"""Atualizacao local 0.5.22: plan, apply (offline), start com health-check.

Copyright (c) 1988 Eurico Schmidt. Todos os direitos reservados.
Nao executa operacoes em outros hosts nem rollback automatico de dados.
"""
import argparse
import datetime
import fcntl
import glob
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import time
import urllib.request

VERSION = '0.5.22+schip1.r20261020004901'
BASE = '0.5.15+schip1.r20261018000000'
TREE = '1.0.0+schip1.r20261018000100'
PACKAGES = {
    'libschipbase': (BASE, 'b531a3e465d51816c4fafc0cfe2f99b8fd068035f9f9721b62477df2161f1c2e'),
    'libschipbase-dev': (BASE, 'bf81d775964476205d502dd9527d7e8adade57b8042cb306a1b331d1e8000b74'),
    'libschiptree': (TREE, 'deea9d264484a7725f44336267e9eab235add1014ea98b525a0d0302500066d6'),
    'libschiptree-dev': (TREE, '9498049973c041ed2a635877c994a3cef197374ea4eba38d5b59eccd871f6d33'),
    'libschip': (VERSION, '8eced08a01b3d31b89a7ea8fd6d88bea9057ae3024540e80483fad2523fc7136'),
    'libschip-dev': (VERSION, '5fa8edd9150ca35299f6088f820c00ac737393d96fc3155f637569d39fb02ca3'),
    'schipmyserver': (VERSION, '35fd0c1feadf3cc5dafeb31bfd44026a041ef30cb940dfe7d5bd5e7c84df0654'),
}
EXPERIMENTS = '''SCHIPMY_DUR_FUTEX SCHIPMY_DUR_PERFIL SCHIPMY_DUR_FILA_PERFIL
SCHIPMY_DUR_WRITEBACK SCHIPMY_SINC_TEMPOS SCHIPMY_SINC_POS_ESCRITA
SCHIPMY_FLEET_SYNC_TICKET SCHIPMY_FLEET_SYNC_PROVA SCHIP_FLEET_SYNC_STATS
SCHIP_FLEET_SYNC_JUNTA SCHIP_FLEET_SYNC_JANELA_US SCHIPMY_FLEET_FALHAS
SCHIPMY_FLEET_RASTRO'''.split()
FLAGS = ['SCHIPMY_FLEET', 'SCHIPMY_SYNC_ON_COMMIT', 'SCHIPMY_GROUP_COMMIT', 'SCHIPMY_PG_WORKERS']
GUARD = 'zz-schip-upgrade-0522.conf'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def durable_json(path, data):
    tmp = path.with_suffix('.tmp')
    with tmp.open('w') as stream:
        json.dump(data, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    tmp.chmod(0o600)
    tmp.replace(path)
    fsync_dir(path.parent)


def fsync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def trusted_parents(path):
    for parent in Path(path).parents:
        info = parent.stat()
        require(info.st_uid == 0 and (not info.st_mode & 0o022 or info.st_mode & stat.S_ISVTX),
                'Diretorio ancestral nao confiavel: ' + str(parent))


def tree(path, exclude_maps=False):
    """Content, mode, ownership, links and directories; never follows symlinks."""
    path = Path(path)
    entries = [path] if path.is_symlink() else [path] + sorted(path.rglob('*')) if path.exists() else []
    out = {}
    for p in entries:
        if exclude_maps and p.name == 'table.locks' and p.parent.name == 'sets':
            continue
        s = p.lstat()
        if stat.S_ISLNK(s.st_mode):
            content = ['link', os.readlink(p)]
        elif stat.S_ISREG(s.st_mode):
            content = ['file', sha(p)]
        elif stat.S_ISDIR(s.st_mode):
            content = ['dir']
        elif stat.S_ISFIFO(s.st_mode):
            content = ['fifo']  # IPC metadata only; never open/read the pipe.
        elif stat.S_ISSOCK(s.st_mode):
            content = ['socket']  # cp -a preserves the node, not a live connection.
        else:
            raise RuntimeError('Arquivo especial inesperado: ' + str(p))
        attrs = {name: hashlib.sha256(os.getxattr(p, name, follow_symlinks=False)).hexdigest()
                 for name in os.listxattr(p, follow_symlinks=False)}
        out[str(p.relative_to(path))] = [s.st_mode, s.st_uid, s.st_gid, content, attrs]
    return out


class Host:
    etc = Path('/etc')
    proc = Path('/proc')

    def __init__(self, work):
        self.work = work

    def path(self, absolute):
        return Path(absolute)

    def run(self, args, env=None):
        with (self.work / 'commands.log').open('a') as log:
            result = subprocess.run(list(map(str, args)), stdout=log, stderr=log,
                                    env=env, timeout=1800)
        require(result.returncode == 0, str(args[0]) + ' falhou; consulte commands.log')

    def read(self, args):
        return subprocess.check_output(list(map(str, args)), text=True, stderr=subprocess.DEVNULL).strip()

    def identity(self):
        return sha(self.etc / 'machine-id')

    def unit(self, name):
        require(re.fullmatch(r'[A-Za-z0-9_.:@\\-]+\.service', name), 'Unidade invalida')
        output = self.read(['systemctl', 'show', name, '-p', 'ActiveState', '-p', 'LoadState',
                            '-p', 'UnitFileState', '-p', 'ExecStart', '-p', 'Conditions',
                            '-p', 'EnvironmentFiles', '-p', 'FragmentPath', '-p', 'DropInPaths'])
        info = dict(line.split('=', 1) for line in output.splitlines() if '=' in line)
        # unidade inexistente tambem vem [unprintable], e `systemctl cat` dela falha
        if info.get('Conditions') == '[unprintable]' and info.get('LoadState') != 'not-found':
            effective = self.read(['systemctl', 'cat', name])
            info['Conditions'] = '\n'.join(line for line in effective.splitlines()
                                            if line.startswith('ConditionPathExists='))
        return info

    def version(self, name):
        try:
            result = self.read(['dpkg-query', '-W', '-f=${db:Status-Status} ${Version}', name]).split()
            return result[1] if result[0] == 'installed' else None
        except (subprocess.CalledProcessError, IndexError):
            return None

    def inventory(self, roots):
        rows = []
        for p in self.proc.glob('[0-9]*'):
            try:
                maps = (p / 'maps').read_text()
                files = [line.split()[-1] for line in maps.splitlines() if line.split()[-1].startswith('/')]
                for fd in (p / 'fd').iterdir():
                    try:
                        files.append(os.readlink(fd))
                    except FileNotFoundError:
                        pass
                exe = (p / 'exe').resolve()
                libraries = [f for f in files if re.search(r'/libschip(?:fleet_auto)?\.so', f)]
                used = any(f.startswith(str(r) + '/') for r in roots for f in files)
                if not (libraries or used or exe.name == 'schipmyserverd'):
                    continue
                unit = self.read(['ps', '-p', p.name, '-o', 'unit='])
                require(unit.endswith('.service') and unit != '-', 'PID fora de servico systemd: ' + p.name)
                global_core = any(f.startswith('/usr/lib/schip/libschip.so') for f in files)
                if exe.name == 'schipmyserverd':
                    require(str(exe) == '/usr/sbin/schipmyserverd' and global_core,
                            'SQL com motor/binario privado exige roteiro especifico: PID ' + p.name)
                dbroots = sorted({str(Path(f.split('/sets/', 1)[0]).parent.resolve())
                                  for f in files if '/sets/' in f})
                require(libraries or exe.name == 'schipmyserverd',
                        'Consumidor estatico ativo exige relink manual antes deste roteiro: PID ' + p.name)
                env = dict(v.split(b'=', 1) for v in (p / 'environ').read_bytes().split(b'\0') if b'=' in v)
                flags = {k: env.get(k.encode(), b'').decode() for k in FLAGS}
                if env.get(b'SCHIP_DBROOT'):
                    dbroots = sorted(set(dbroots) | {str(Path(env[b'SCHIP_DBROOT'].decode()).resolve())})
                # Store only whether Fleet is set, never its addresses/credentials.
                flags['SCHIPMY_FLEET'] = bool(flags['SCHIPMY_FLEET'])
                require(not any(env.get(k.encode(), b'') not in (b'', b'0', b'off') for k in EXPERIMENTS),
                        'Opcoes experimentais ativas no PID ' + p.name)
                require(flags['SCHIPMY_GROUP_COMMIT'] in ('', 'off', '0'), 'Group commit experimental ativo no PID ' + p.name)
                rows.append(dict(unit=unit, sql=exe.name == 'schipmyserverd',
                                 global_core=global_core, roots=dbroots, flags=flags))
            except (FileNotFoundError, ProcessLookupError):
                continue
        return rows

    def native_check(self):
        output = self.read(['dpkg', '--audit'])
        require(not output, 'dpkg tem pendencias; resolva antes de continuar')

    def dependencies(self, packages, allowed):
        text = self.read(['apt-get', '-s', '--no-remove', 'install', *packages])
        names = re.findall(r'^Inst ([^ :]+)', text, re.M)
        require(set(names) <= set(allowed), 'Dependencias externas exigem manutencao previa: ' + str(set(names) - set(allowed)))

    def install(self, packages):
        self.run(['dpkg', '-i', '--auto-deconfigure', '--force-confold', *packages],
                 env=dict(os.environ, DEBIAN_FRONTEND='noninteractive', DPKG_FRONTEND_LOCKED='true'))

    def link_check(self):
        output = self.read(['env', 'LD_LIBRARY_PATH=/usr/lib/schip', 'ldd', '-r', '/usr/sbin/schipmyserverd'])
        require(not re.search(r'not found|undefined symbol', output), 'Dependencia ELF ausente')

    def sql_flags(self, roots):
        return {r['unit']: r['flags'] for r in self.inventory(roots) if r['sql']}

    def health(self, command):
        with (self.work / 'health-check.log').open('a') as log:
            result = subprocess.run([command], stdout=log, stderr=log, timeout=120)
        require(result.returncode == 0, 'Health-check falhou; aplicacoes permanecem bloqueadas')

    def health_file(self, command):
        path = Path(command).resolve(strict=True)
        trusted_parents(path)
        s = path.stat()
        require(s.st_uid == 0 and not s.st_mode & 0o022 and os.access(path, os.X_OK),
                'Health-check deve ser executavel root, sem escrita de grupo/outros')
        return str(path)

    def managed_roots(self, helper):
        return helper.managed_roots()

    def fleet_protocol_check(self):
        module = self.path('/usr/lib/schip/libschipfleet_auto.so')
        require(module.is_file() and sha(module) == '4e068beb1d40c6e9fc1a7f55e02375f3f4d4d8b0229ef77e3f624ea9ff4b75ed',
                'Fleet ativo com modulo diferente do v4 qualificado; exige plano de compatibilidade antes da atualizacao')

    def prepare_packages(self):
        packages = self.work / 'packages'
        packages.mkdir(exist_ok=True)
        for name, (version, expected) in PACKAGES.items():
            filename = name + '_' + version + '_amd64.deb'
            path = packages / filename
            if not path.exists():
                with urllib.request.urlopen('https://deb.schip.com.br/pool/main/' + name + '/' + filename, timeout=60) as source:
                    with path.with_suffix('.part').open('wb') as target:
                        shutil.copyfileobj(source, target)
                path.with_suffix('.part').replace(path)
            require(sha(path) == expected, 'SHA256 incorreto: ' + filename)
            require(self.read(['dpkg-deb', '-f', path, 'Package']) == name, 'Nome de pacote divergente')
            require(self.read(['dpkg-deb', '-f', path, 'Version']) == version, 'Versao divergente')
            extracted = self.work / 'extracted' / name
            if not extracted.exists():
                extracted.parent.mkdir(parents=True, exist_ok=True)
                self.run(['dpkg-deb', '-x', path, extracted])
        return self.work / 'extracted/libschip/usr/lib/schip/lock-map-upgrade.py'


class Upgrade:
    def __init__(self, host):
        self.h = host
        self.work = host.work
        self.statefile = self.work / 'state.json'
        self.data = json.loads(self.statefile.read_text()) if self.statefile.exists() else None

    def save(self):
        durable_json(self.statefile, self.data)

    def helper(self):
        path = self.h.prepare_packages()
        spec = importlib.util.spec_from_file_location('upgrade_lock_maps', path)
        helper = importlib.util.module_from_spec(spec)
        previous = sys.dont_write_bytecode
        try:
            sys.dont_write_bytecode = True
            spec.loader.exec_module(helper)
        finally:
            sys.dont_write_bytecode = previous
        return helper

    def roots(self):
        return list(map(Path, self.data['roots']))

    def offline(self):
        self.helper().offline(proc=self.h.proc, roots=self.roots())
        for unit in self.data['units']:
            info = self.h.unit(unit)
            require(info['ActiveState'] == 'inactive', 'Unidade nao parou: ' + unit)
            require(self.data['allow_path'] in info.get('Conditions', ''), 'Bloqueio de partida ausente: ' + unit)

    def config_paths(self):
        paths = set(self.h.etc.glob('schipmyserver*.conf'))
        paths.add(self.h.etc / 'schip')
        for unit in self.data['units']:
            paths.add(self.h.etc / 'systemd/system' / unit)
            paths.add(self.h.etc / 'systemd/system' / (unit + '.d'))
            info = self.h.unit(unit)
            for value in re.findall(r'(/.*?)\s+\(ignore_errors=(?:yes|no)\)', info.get('EnvironmentFiles', '')):
                paths.update(Path(p) for p in (glob.glob(value) or [value]))
            fragment = info.get('FragmentPath', '')
            if fragment and not fragment.startswith(('/usr/lib/', '/lib/')):
                paths.add(Path(fragment))
        paths.update(map(Path, self.data.get('configs', [])))
        return sorted(paths)

    def configs(self):
        result = {}
        for p in self.config_paths():
            value = tree(p)
            value = {k: v for k, v in value.items() if k != GUARD}
            # Creating a drop-in directory for our guard is not a config change.
            if str(p).endswith('.service.d') and not value.keys() - {'.'}:
                value = {}
            result[str(p)] = value
        return result

    def plan(self, roots, units, sql_units, producers, configs):
        require(self.data is None, 'Ja existe plano neste diretorio')
        self.h.native_check()
        require(self.h.version('schipmyserver'), 'Instalacao existente de schipmyserver obrigatoria')
        require(not self.h.path('/var/lib/schip/apt-maintenance/expected').exists(), 'APT tem manutencao pendente')
        helper = self.helper()
        roots = {Path(p).resolve(strict=True) for p in roots} | set(self.h.managed_roots(helper))
        rows = self.h.inventory(roots)
        for r in rows:
            if r['global_core'] or r['sql']:
                roots.update(Path(p).resolve(strict=True) for p in r['roots'])
        roots = sorted(roots)
        require(roots, 'Nenhuma raiz encontrada; informe --root')
        require(all(r.is_dir() for r in roots), 'Toda --root deve ser um diretorio de bancos')
        require(all(self.work != r and r not in self.work.parents for r in roots), 'Backup dentro de uma raiz de banco')
        require(not any(a in b.parents for a in roots for b in roots if a != b), 'Raizes sobrepostas')
        for root in roots:
            for path in root.glob('*/sets/table.locks'):
                require(not path.is_symlink(), 'Mapa de travas e link: ' + str(path))
                with path.open('rb') as stream:
                    header = stream.read(8)
                require(len(header) == 8 and struct.unpack('<II', header) in ((0x544c4b31, 1), (0x544c4b31, 2)),
                        'Formato desconhecido do mapa: ' + str(path))
        for r in rows:
            if not r['global_core'] and not r['sql']:
                require(not set(r['roots']) & set(map(str, roots)), 'Biblioteca privada compartilha raiz migrada: ' + r['unit'])
        units = set(units) | set(sql_units) | set(producers) | {r['unit'] for r in rows} | {'schipmyserverd.service'}
        sql_units = set(sql_units) | {r['unit'] for r in rows if r['sql']} | {'schipmyserverd.service'}
        snapshots = {}
        for unit in sorted(units):
            info = self.h.unit(unit)
            require(info.get('LoadState') == 'loaded', 'Unidade nao carregada: ' + unit)
            require(info['ActiveState'] in ('active', 'inactive'), 'Unidade em transicao: ' + unit)
            require(not (self.h.etc / 'systemd/system' / (unit + '.d') / GUARD).exists(), 'Bloqueio de outra execucao: ' + unit)
            snapshots[unit] = {k: info[k] for k in ('ActiveState', 'UnitFileState')}
        sql_flags = {r['unit']: r['flags'] for r in rows if r['sql']}
        if any(flags['SCHIPMY_FLEET'] for flags in sql_flags.values()):
            self.h.fleet_protocol_check()
        selected = [n for n in PACKAGES if not n.endswith('-dev') or self.h.version(n)]
        for name in selected:
            if self.h.version(name):
                self.h.read(['dpkg', '--compare-versions', PACKAGES[name][0], 'ge', self.h.version(name)])
        self.data = dict(version=VERSION, host=self.h.identity(), roots=list(map(str, roots)),
                         units=snapshots, sql_units=sorted(sql_units), producers=sorted(producers),
                         configs=[str(Path(p).resolve(strict=True)) for p in configs], sql_flags=sql_flags,
                         selected=selected, completed=[], allow_path='/run/schip-upgrade-0522-' + self.work.name,
                         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), opened=False)
        for config in self.config_paths():
            require(config != self.work and config not in self.work.parents, 'Configuracao inclui diretorio de trabalho')
            require(not any(config == r or config in r.parents or r in config.parents for r in roots),
                    'Configuracao sobreposta a raiz de banco; separe --config de --root')
        self.data['config_before'] = self.configs()
        self.h.dependencies(self.package_paths(), selected)
        self.check_space()
        self.save()
        return dict(units=snapshots, roots=self.data['roots'], packages=selected,
                    sql_flags=sql_flags, state=str(self.statefile))

    def package_paths(self):
        return [str(self.work / 'packages' / (n + '_' + PACKAGES[n][0] + '_amd64.deb')) for n in self.data['selected']]

    def check_space(self):
        total = 0
        for root in self.roots():
            for p in root.rglob('*'):
                require(not p.is_symlink() or p.resolve().is_relative_to(root), 'Link externo sem backup: ' + str(p))
                if p.is_file():
                    total += p.stat().st_size
            banks = sum(1 for p in root.glob('*/sets') if p.is_dir())
            require(shutil.disk_usage(root).free > banks * 16 * 1024**2 + 64 * 1024**2,
                    'Espaco insuficiente para mapas v2: ' + str(root))
        require(shutil.disk_usage(self.work).free > total + 256 * 1024**2, 'Espaco insuficiente para backup frio')

    def block(self):
        require(self.configs() == self.data['config_before'], 'Configuracao mudou depois do plano; refaca o inventario')
        if not self.data.get('blocking_started'):
            for unit, before in self.data['units'].items():
                require(all(self.h.unit(unit)[k] == v for k, v in before.items()), 'Estado mudou depois do plano: ' + unit)
            require(self.h.sql_flags(self.roots()) == self.data['sql_flags'], 'Flags SQL mudaram depois do plano')
            require({r['unit'] for r in self.h.inventory(self.roots())} <= set(self.data['units']), 'Novo consumidor depois do plano')
            self.data['blocking_started'] = True
            self.save()
        require(not self.h.path(self.data['allow_path']).exists(), 'Caminho de liberacao inesperado')
        for unit in self.data['units']:
            folder = self.h.etc / 'systemd/system' / (unit + '.d')
            folder.mkdir(parents=True, exist_ok=True)
            (folder / GUARD).write_text('[Unit]\nConditionPathExists=' + self.data['allow_path'] + '\n')
        self.h.run(['systemctl', 'daemon-reload'])
        for group in (self.data['producers'], [u for u in self.data['units'] if u not in self.data['producers'] and u not in self.data['sql_units']], self.data['sql_units']):
            if group:
                self.h.run(['systemctl', 'stop', *group])
        self.offline()
        for unit in self.data['units']:
            self.h.run(['systemctl', 'start', unit])
            require(self.h.unit(unit)['ActiveState'] == 'inactive', 'Bloqueio ineficaz: ' + unit)

    def backup(self):
        self.offline()
        self.check_space()
        backup = self.work / 'backup'
        backup.mkdir(exist_ok=True)
        manifests = {}
        full_manifests = {}
        for i, root in enumerate(self.roots()):
            before = tree(root)
            target = backup / ('data-' + str(i))
            if not target.exists():
                self.h.run(['cp', '-a', '--reflink=auto', root, target])
            require(before == tree(target) == tree(root), 'Backup frio divergente: ' + str(root))
            manifests[str(root)] = tree(root, exclude_maps=True)
            full_manifests[str(root)] = before
        resources = self.config_paths() + [self.h.path('/usr/lib/schip'), self.h.path('/usr/sbin/schipmyserverd'),
                                          self.h.etc / 'systemd/system', self.h.path('/var/lib/dpkg/status')]
        resource_manifest = {}
        for i, source in enumerate(sorted(set(resources))):
            if source.exists():
                target = backup / ('resource-' + str(i))
                if not target.exists():
                    self.h.run(['cp', '-a', '--reflink=auto', source, target])
                require(tree(source) == tree(target), 'Backup de recurso divergente: ' + str(source))
                resource_manifest[str(source)] = dict(copy=target.name, entries=tree(target))
        for p in backup.rglob('*'):
            if p.is_file() and not p.is_symlink():
                with p.open('rb') as stream:
                    os.fsync(stream.fileno())
        for folder in sorted([backup] + [p for p in backup.rglob('*') if p.is_dir() and not p.is_symlink()], key=lambda p: len(p.parts), reverse=True):
            fsync_dir(folder)
        self.data['cold_data'] = manifests
        durable_json(backup / 'data-manifest.json', full_manifests)
        durable_json(backup / 'resource-manifest.json', resource_manifest)
        self.data['config_expected'] = self.configs()

    def migrate(self):
        self.offline()
        parent = self.work / 'map-backups'
        parent.mkdir(exist_ok=True)
        backup = parent / ('attempt-' + str(time.time_ns()))
        report = self.helper().retire(self.roots(), backup=backup, apply=True, proc=self.h.proc)
        durable_json(self.work / 'migration.json', report)
        registry = self.h.etc / 'schip/lock-map-roots'
        registry.parent.mkdir(exist_ok=True)
        lines = registry.read_text().splitlines() if registry.exists() else []
        for root in self.data['roots']:
            if root not in lines:
                lines.append(root)
        registry.write_text('\n'.join(lines) + '\n')
        self.data['config_expected'] = self.configs()

    def snapshot_configs(self):
        # --force-confold only handles locally modified conffiles. Dpkg replaces
        # unmodified defaults, so keep and restore every original config file too.
        snapshot = self.work / 'config-before-install'
        records = snapshot / 'paths.json'
        if not records.exists():
            snapshot.mkdir(exist_ok=True)
            paths = {}
            for i, source in enumerate(self.config_paths()):
                if source.exists() or source.is_symlink():
                    target = snapshot / str(i)
                    require(not target.exists(), 'Snapshot de configuracao parcial; confira antes de repetir')
                    self.h.run(['cp', '-a', source, target])
                    require(tree(source) == tree(target), 'Snapshot de configuracao divergente')
                    paths[str(source)] = str(i)
            durable_json(records, paths)
        return records

    def restore_configs(self):
        snapshot = self.work / 'config-before-install'
        records = snapshot / 'paths.json'
        require(records.exists(), 'Snapshot de configuracao ausente')
        for original, index in json.loads(records.read_text()).items():
            source = snapshot / index
            entries = [source] + sorted(source.rglob('*'))
            for item in entries:
                target = Path(original) / item.relative_to(source) if item != source else Path(original)
                if item.is_dir() and not item.is_symlink():
                    require(target.is_dir() and not target.is_symlink(), 'Diretorio de configuracao mudou de tipo: ' + str(target))
                elif tree(item) != tree(target):
                    self.h.run(['cp', '-a', '--remove-destination', item, target])

    def install(self):
        self.offline()
        self.helper().ready(roots=self.roots(), proc=self.h.proc)
        self.snapshot_configs()
        self.h.install(self.package_paths())
        self.offline()
        self.restore_configs()
        # Package postinst may enable the default service; restore its prior policy.
        for unit, before in self.data['units'].items():
            wanted = before['UnitFileState']
            actual = self.h.unit(unit)['UnitFileState']
            if wanted in ('enabled', 'disabled') and wanted != actual:
                self.h.run(['systemctl', 'enable' if wanted == 'enabled' else 'disable', unit])

    def validate(self):
        self.offline()
        self.h.native_check()
        require(self.configs() == self.data['config_expected'], 'Configuracao alterada; nao liberar os servicos')
        for name in self.data['selected']:
            require(self.h.version(name) == PACKAGES[name][0], 'Versao incoerente: ' + name)
            extracted = self.work / 'extracted' / name
            for p in extracted.rglob('*'):
                relative = p.relative_to(extracted)
                if relative.parts[0] == 'etc' or (p.is_dir() and not p.is_symlink()):
                    continue
                target = self.h.path('/' + str(relative))
                if p.is_symlink():
                    require(target.is_symlink() and os.readlink(p) == os.readlink(target), 'Link instalado divergente: ' + str(relative))
                elif p.is_file():
                    require(target.is_file() and sha(p) == sha(target), 'Arquivo instalado divergente: ' + str(relative))
        for unit, before in self.data['units'].items():
            require(self.h.unit(unit)['UnitFileState'] == before['UnitFileState'], 'Politica de habilitacao mudou: ' + unit)
        if not self.data['opened']:
            for root, expected in self.data['cold_data'].items():
                require(tree(Path(root), exclude_maps=True) == expected, 'Dados mudaram durante manutencao offline')
        self.helper().ready(roots=self.roots(), proc=self.h.proc)
        self.h.link_check()

    def apply(self, reviewed):
        require(reviewed, 'Revise o plano, tarefas externas e consumidores estaticos; use --inventory-reviewed')
        require(self.data and self.data['host'] == self.h.identity(), 'Plano pertence a outro host')
        require('started' not in self.data['completed'], 'Atualizacao e retorno ja concluidos')
        validated_before = 'validated' in self.data['completed']
        self.h.prepare_packages()
        for name, action in [('blocked', self.block), ('backup', self.backup), ('migrated', self.migrate),
                             ('installed', self.install), ('validated', self.validate)]:
            if name not in self.data['completed']:
                action()
                self.data['completed'].append(name)
                self.save()
        if validated_before:
            self.validate()
        return dict(offline_validated=True, services_still_blocked=True, backup=str(self.work / 'backup'))

    def start(self, health_check):
        require(self.data and 'validated' in self.data['completed'], 'Execute apply antes de start')
        require(self.data['host'] == self.h.identity(), 'Plano pertence a outro host')
        require('started' not in self.data['completed'], 'Retorno ja concluido')
        self.validate()
        check = self.h.health_file(health_check)
        active_sql = [u for u in self.data['sql_units'] if self.data['units'][u]['ActiveState'] == 'active']
        try:
            self.data['opened'] = True
            self.save()
            for unit in active_sql:
                (self.h.etc / 'systemd/system' / (unit + '.d') / GUARD).unlink()
            self.h.run(['systemctl', 'daemon-reload'])
            if active_sql:
                self.h.run(['systemctl', 'start', *active_sql])
            require(all(self.h.unit(u)['ActiveState'] == 'active' for u in active_sql), 'SQL nao iniciou')
            current = self.h.sql_flags(self.roots())
            for unit in active_sql:
                require(current.get(unit) == self.data['sql_flags'].get(unit), 'Flags SQL/Fleet/durabilidade mudaram: ' + unit)
            # Must verify SQL/auth/Fleet/quorum on THIS host, with a bounded timeout.
            self.h.health(str(check))
        except Exception:
            for unit in active_sql:
                folder = self.h.etc / 'systemd/system' / (unit + '.d')
                (folder / GUARD).write_text('[Unit]\nConditionPathExists=' + self.data['allow_path'] + '\n')
            self.h.run(['systemctl', 'daemon-reload'])
            if active_sql:
                self.h.run(['systemctl', 'stop', *active_sql])
            raise
        apps = [u for u, before in self.data['units'].items() if before['ActiveState'] == 'active' and u not in self.data['sql_units']]
        try:
            for unit in self.data['units']:
                (self.h.etc / 'systemd/system' / (unit + '.d') / GUARD).unlink(missing_ok=True)
            self.h.run(['systemctl', 'daemon-reload'])
            if apps:
                self.h.run(['systemctl', 'start', *apps])
            require(all(self.h.unit(u)['ActiveState'] == before['ActiveState'] for u, before in self.data['units'].items()),
                    'Estado de servicos diferente do inventario; confira manualmente')
        except Exception:
            for unit in self.data['units']:
                folder = self.h.etc / 'systemd/system' / (unit + '.d')
                (folder / GUARD).write_text('[Unit]\nConditionPathExists=' + self.data['allow_path'] + '\n')
            self.h.run(['systemctl', 'daemon-reload'])
            if apps:
                self.h.run(['systemctl', 'stop', *apps])
            if active_sql:
                self.h.run(['systemctl', 'stop', *active_sql])
            raise
        self.data['completed'].append('started')
        self.save()
        return dict(started=True, health_check_passed=True, version=VERSION)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['plan', 'apply', 'verify', 'start', 'status'])
    parser.add_argument('--work', type=Path, required=True, help='Diretorio exclusivo, fora das raizes de bancos')
    parser.add_argument('--root', action='append', default=[], help='Raiz adicional de bancos')
    parser.add_argument('--unit', action='append', default=[], help='Consumidor adicional, inclusive inativo')
    parser.add_argument('--sql-unit', action='append', default=[], help='Instancia SQL adicional')
    parser.add_argument('--producer-unit', action='append', default=[], help='Produtor que deve parar antes do SQL, ex. zabbix-server.service')
    parser.add_argument('--config', action='append', default=[], help='Configuracao adicional a preservar')
    parser.add_argument('--inventory-reviewed', action='store_true', help='Inventario, cron/containers, quorum e clientes estaticos foram revisados')
    parser.add_argument('--health-check', help='Executavel local root: verificar SQL/Fleet antes de liberar apps; limite 120s')
    args = parser.parse_args()
    require(os.geteuid() == 0, 'Execute como root; --help nao exige root')
    require(args.work.is_absolute() and not args.work.is_symlink(), '--work deve ser caminho absoluto sem link')
    work = args.work.resolve()
    trusted_parents(work)
    if args.phase == 'plan':
        require(not work.exists(), '--work precisa ser novo para plan')
        work.mkdir(mode=0o700)
    s = work.stat()
    require(s.st_uid == 0 and not s.st_mode & 0o077, '--work precisa pertencer ao root com modo 0700')
    host = Host(work)
    release = (host.etc / 'os-release').read_text()
    require(re.search(r'^ID=["\x27]?debian["\x27]?$', release, re.M)
            and re.search(r'^VERSION_CODENAME=["\x27]?trixie["\x27]?$', release, re.M), 'Somente Debian 13/trixie')
    require(host.read(['dpkg', '--print-architecture']) == 'amd64', 'Somente amd64')
    with (work / 'execution.lock').open('w') as lock, host.path('/var/lib/dpkg/lock-frontend').open('a') as apt_lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        fcntl.lockf(apt_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        engine = Upgrade(host)
        if args.phase == 'plan':
            result = engine.plan(args.root, args.unit, args.sql_unit, args.producer_unit, args.config)
        else:
            require(engine.data and engine.data['host'] == host.identity(), 'Estado ausente ou de outro host')
            if args.phase == 'apply':
                result = engine.apply(args.inventory_reviewed)
            elif args.phase == 'verify':
                require('installed' in engine.data['completed'], 'Instalacao offline ainda nao concluida')
                engine.validate()
                result = dict(offline_validated=True, services_still_blocked=True)
            elif args.phase == 'start':
                require(args.health_check, 'Informe --health-check; nao ha liberacao sem conferir o banco')
                result = engine.start(args.health_check)
            else:
                result = {k: engine.data[k] for k in ('version', 'roots', 'units', 'completed', 'opened')}
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        print('Atualizacao interrompida: ' + str(error), file=sys.stderr)
        print('Os bloqueios ja aplicados permanecem. Preserve --work e os backups; nao apague mapas nem restaure dados automaticamente.', file=sys.stderr)
        sys.exit(1)
