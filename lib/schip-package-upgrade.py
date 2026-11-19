#!/usr/bin/env python3
"""Persistent package transaction barrier; never invokes apt/dpkg installation.

Copyright (c) 1988 Eurico Schmidt. Todos os direitos reservados.
"""
import argparse
import datetime
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import sys
import time

HERE = Path(__file__).resolve().parent
kitfile = HERE/'package_upgrade_kit.py'
if not kitfile.exists():
    kitfile = HERE/'schip-upgrade-0522.py'
spec = importlib.util.spec_from_file_location('package_upgrade_kit', kitfile)
kit = importlib.util.module_from_spec(spec); spec.loader.exec_module(kit)
kit.GUARD = 'zz-schip-package-maintenance.conf'
COHORT = ['libschipbase','libschiptree','libschip','schipmyserver',
          'libschipbase-dev','libschiptree-dev','libschip-dev']
MARKER = '/var/lib/schip/package-maintenance/active.json'


class PendingTransaction(RuntimeError):
    pass


def ready(ok, message):
    if not ok:raise PendingTransaction(message)


def env_file(path):
    result = {}
    for line in Path(path).read_text().splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        name, sep, value = line.partition('=')
        if sep:
            words = shlex.split(value, comments=True)
            result[name.strip()] = ' '.join(words)
    return result


class AutoHost(kit.Host):
    def native_check(self):
        if os.environ.get('DPKG_MAINTSCRIPT_NAME')!='preinst':
            return super().native_check()
        # dpkg marks its current package half-installed BEFORE invoking preinst.
        # Planned packages may also have been deconfigured by Breaks, while their
        # old payload is still intact. Do not confuse that with an unrelated fault.
        planned=set(getattr(self,'pre_expected',{}))|{os.environ.get('DPKG_MAINTSCRIPT_PACKAGE','')}
        output=self.read(['dpkg-query','-W','-f=${binary:Package}\t${db:Status-Status}\n'])
        for line in output.splitlines():
            name,status=line.split('\t');name=name.split(':')[0]
            valid=status in ('installed','triggers-pending','triggers-awaited','config-files','not-installed')
            valid=valid or name in planned and status in ('half-installed','half-configured','unpacked')
            kit.require(valid,'Pendencia dpkg fora da transacao: '+name)

    def prepare_packages(self):
        helper = HERE/'package_lock_map.py'
        return helper if helper.exists() else HERE/'lock_map_upgrade.py'

    def fleet_protocol_check(self):
        module = self.path('/usr/lib/schip/libschipfleet_auto.so')
        kit.require(module.is_file(), 'Modulo Fleet ausente: /usr/lib/schip/libschipfleet_auto.so')

    def effective(self, unit):
        info = self.unit(unit)
        environment = self.read(['systemctl','show',unit,'-p','Environment','--value'])
        result = dict(v.split('=',1) for v in shlex.split(environment) if '=' in v)
        for filename in re.findall(r'(/.*?)\s+\(ignore_errors=(?:yes|no)\)', info.get('EnvironmentFiles','')):
            path = Path(filename)
            if path.is_file():
                result.update(env_file(path))
        exec_start = self.read(['systemctl','show',unit,'-p','ExecStart','--value'])
        m_argv = re.search(r'argv\[\]=(.*?)(?: ; |$)', exec_start)
        if m_argv:
            parts = shlex.split(m_argv[1])
            for i, a in enumerate(parts):
                if a == '-S' and i + 1 < len(parts):
                    result['SCHIPMY_SOCKET'] = parts[i+1]
                elif a.startswith('-S') and len(a) > 2:
                    result['SCHIPMY_SOCKET'] = a[2:]
        return result

    def sql_flags(self, roots):
        return {u: self.config_flags(u) for u in self.known_units() if self.unit(u)['ActiveState'] == 'active' and '/schipmyserverd' in self.unit(u).get('ExecStart', '')}

    def config_flags(self, unit):
        cfg = self.effective(unit)
        kit.require(not any(cfg.get(k,'') not in ('','off','0') for k in kit.EXPERIMENTS),
                    'Opcoes experimentais ativas: '+unit)
        flags = {k:cfg.get(k,'') for k in kit.FLAGS}
        flags['SCHIPMY_FLEET'] = bool(flags['SCHIPMY_FLEET'])
        kit.require(flags['SCHIPMY_GROUP_COMMIT'] in ('','off','0'),'Group commit experimental ativo')
        return flags

    def known_units(self):
        text = self.read(['systemctl','list-unit-files','--type=service','--no-legend','--no-pager'])
        return [line.split()[0] for line in text.splitlines() if line.strip() and '@.' not in line.split()[0]]

    def native_ready(self, expected):
        output = self.read(['dpkg-query','-W','-f=${binary:Package}\t${db:Status-Status}\t${Version}\n'])
        rows = {r[0].split(':')[0]:(r[1],r[2]) for line in output.splitlines() if len(r:=line.split('\t'))==3}
        actor=os.environ.get('DPKG_MAINTSCRIPT_PACKAGE','')
        if os.environ.get('SCHIP_UPGRADE_TRIGGER')=='1' and actor in COHORT and rows.get(actor,('',))[0]=='half-configured':
            rows[actor]=('installed',rows[actor][1])
        allowed = ('installed','triggers-pending','triggers-awaited','config-files','not-installed')
        ready(all(status in allowed for status,_ in rows.values()),'dpkg ainda tem pacotes incompletos')
        for name, version in expected.items():
            ready(rows.get(name,('',''))[1] == version and rows[name][0] in allowed[:3],
                        'Transacao incompleta: '+name)
        for name in COHORT:
            if name not in rows or rows[name][0] not in allowed[:3]:
                continue
            depends=self.read(['dpkg-query','-W','-f=${Depends}',name])
            for library,operation,version in re.findall(r'(libschip(?:base|tree)?)\s*\((=|>=)\s*([^)]+)\)',depends):
                actual=rows.get(library,('',''))[1]
                ready(bool(actual),'Biblioteca necessaria ausente: '+library)
                try:self.read(['dpkg','--compare-versions',actual,'eq' if operation=='=' else 'ge',version.strip()])
                except kit.subprocess.CalledProcessError:raise PendingTransaction('Dependencia incoerente: '+name+' -> '+library)
            if name.endswith('-dev'):
                ready(rows[name][1]==rows.get(name[:-4],('',''))[1], 'SDK incoerente: '+name)
        return rows

    def owned_paths(self):
        paths=set();versions={}
        for name in COHORT:
            try:version=self.read(['dpkg-query','-W','-f=${Version}',name])
            except kit.subprocess.CalledProcessError:continue
            if not version:continue
            versions[name]=version
            paths.update(Path(p) for p in self.read(['dpkg-query','-L',name]).splitlines() if p.startswith('/'))
            paths.update(self.path('/var/lib/dpkg/info').glob(name+'.*'))
        # dpkg -L includes merged-/usr directory aliases such as /lib. They are
        # filesystem layout, not cohort files; copying them would redirect backup.
        return sorted(p for p in paths if p.is_file() or p.is_symlink() and not p.is_dir()),versions

    def payload_ready(self, expected):
        for name in expected:
            if name not in COHORT:
                continue
            manifest = self.path('/usr/share/schip/package-upgrade/'+name+'.json')
            kit.require(manifest.is_file(),'Manifesto SHA256 ausente: '+name)
            record = json.loads(manifest.read_text())
            for relative, value in record['files'].items():
                target = self.path('/'+relative)
                if value[0]=='link':
                    kit.require(target.is_symlink() and os.readlink(target)==value[1],'Link instalado divergente: '+relative)
                else:
                    kit.require(target.is_file() and kit.sha(target)==value[1],'SHA256 instalado divergente: '+relative)
        self.run(['ldconfig'])
        if self.version('schipmyserver'):
            self.link_check()

    def validate_legacy_client(self, unit, sql):
        info = self.unit(unit)
        if sql:
            kit.require('/usr/sbin/schipmyserverd' in info.get('ExecStart',''),'Instancia SQL privada: '+unit)
        match = re.search(r'path=([^ ;]+)',info.get('ExecStart',''))
        if not match:
            return
        executable = match[1]
        if executable.endswith(('/python3','/sh','/bash')):
            return
        linked = self.read(['ldd',executable])
        if 'libschip.so' in linked:
            kit.require('/usr/lib/schip/libschip.so' in linked,
                        'Consumidor privado sem inventario anterior ao hook antigo: '+unit)

    def detect_producers(self, sql_units):
        unit='zabbix-server.service'
        if unit not in self.known_units() or self.unit(unit)['ActiveState']!='active':return []
        path=self.etc/'zabbix/zabbix_server.conf'
        if not path.is_file():return []
        values={}
        for line in path.read_text().splitlines():
            name,sep,value=line.partition('=')
            if sep and name.strip() in ('DBHost','DBPort'):values[name.strip()]=value.strip()
        ports={self.effective(u).get(k,'') for u in sql_units for k in ('SCHIPMY_PORT','SCHIPMY_PG_PORT')}
        return [unit] if values.get('DBHost','localhost') in ('localhost','127.0.0.1','::1','') and values.get('DBPort') in ports else []

    def health_file(self, command):
        return command

    def health(self, command):
        config = load_config(self)
        custom = config.get('health_command')
        if custom:
            custom = kit.Host.health_file(self,custom)
            return kit.Host.health(self,custom)
        import package_sql_probe
        state = json.loads((self.work/'state.json').read_text())
        nodes = [u for u in state['sql_units'] if state['units'][u]['ActiveState']=='active']
        deadline = time.monotonic()+75
        while True:
            try:
                for unit in nodes:
                    package_sql_probe.check(self.effective(unit), state['sql_flags'][unit]['SCHIPMY_FLEET'])
                return
            except (RuntimeError,OSError):
                if time.monotonic()>=deadline:
                    raise RuntimeError('Verificacao SQL/Fleet falhou; aplicacoes nao serao liberadas')
                time.sleep(2)


def load_config(host):
    path = host.etc/'schip/package-upgrade.json'
    if not path.exists():
        return {}
    info = path.stat()
    kit.require(info.st_uid==0 and not info.st_mode & 0o077,'package-upgrade.json deve ser root, modo 0600')
    data = json.loads(path.read_text())
    allowed = {'backup_parent','roots','units','sql_units','producer_units','health_command'}
    kit.require(isinstance(data,dict) and not set(data)-allowed,'Configuracao automatica desconhecida')
    return data


class Transaction(kit.Upgrade):
    def backup(self):
        super().backup()
        if not hasattr(self.h,'owned_paths'):return
        paths,versions=self.h.owned_paths();manifest={}
        for source in paths:
            target=self.work/'backup/old-package-files'/str(source).lstrip('/')
            target.parent.mkdir(parents=True,exist_ok=True)
            if not target.exists() and not target.is_symlink():self.h.run(['cp','-a',source,target])
            kit.require(kit.tree(source)==kit.tree(target),'Arquivo antigo nao foi preservado: '+str(source))
            manifest[str(source)]=kit.tree(target)
            if target.is_file() and not target.is_symlink():
                with target.open('rb') as stream:os.fsync(stream.fileno())
        for p in sorted((self.work/'backup').rglob('*'),key=lambda p:len(p.parts),reverse=True):
            if p.is_dir() and not p.is_symlink():kit.fsync_dir(p)
        kit.durable_json(self.work/'backup/old-packages.json',dict(versions=versions,files=manifest))

    def prepare(self, expected, legacy, config):
        if self.data:
            kit.require(self.data['host']==self.h.identity(),'Transacao de outro host')
            self.data['expected'].update(expected);self.save()
            kit.require('backup' in self.data['completed'],'Preparacao incompleta; preserve o backup parcial e investigue')
            return
        self.h.pre_expected=expected
        self.h.native_check()
        roots = set(self.h.managed_roots(self.helper())) | {Path(p).resolve(strict=True) for p in config.get('roots',[])}
        rows = self.h.inventory(roots)
        for r in rows:
            if r['global_core'] or r['sql']:
                roots.update(Path(p).resolve(strict=True) for p in r['roots'])
        roots = sorted(roots)
        kit.require(roots and all(p.is_dir() for p in roots),'Raizes de banco invalidas')
        kit.require(not any(a in b.parents for a in roots for b in roots if a!=b),'Raizes sobrepostas')
        kit.require(all(self.work!=r and r not in self.work.parents for r in roots),'Backup dentro dos dados')
        units = set(legacy)|set(config.get('units',[]))|set(config.get('sql_units',[]))|set(config.get('producer_units',[]))|{r['unit'] for r in rows}
        sql = set(config.get('sql_units',[]))|{r['unit'] for r in rows if r['sql']}
        for unit in self.h.known_units():
            info = self.h.unit(unit)
            if '/usr/sbin/schipmyserverd' in info.get('ExecStart',''):
                units.add(unit);sql.add(unit)
        producers=sorted(set(config.get('producer_units',[]))|set(self.h.detect_producers(sql)))
        units.update(producers)
        # host so com libs/shadow (sem o pacote schipmyserver) nao tem a unidade
        if self.h.unit('schipmyserverd.service').get('LoadState')=='loaded':
            units.add('schipmyserverd.service');sql.add('schipmyserverd.service')
        snapshots = {}
        for unit in sorted(units):
            info=self.h.unit(unit)
            kit.require(info.get('LoadState')=='loaded' and info['ActiveState'] in ('active','inactive'),'Unidade nao suportada: '+unit)
            snapshots[unit]=dict(ActiveState='active' if unit in legacy else info['ActiveState'],UnitFileState=info['UnitFileState'])
            if unit in legacy:
                self.h.validate_legacy_client(unit,unit in sql)
        flags={u:self.h.config_flags(u) for u in sql if snapshots[u]['ActiveState']=='active'}
        flags.update({r['unit']:r['flags'] for r in rows if r['sql']})
        if any(v['SCHIPMY_FLEET'] for v in flags.values()):self.h.fleet_protocol_check()
        self.data=dict(version='package-transaction-v1',host=self.h.identity(),roots=list(map(str,roots)),
            units=snapshots,sql_units=sorted(sql),sql_flags=flags,producers=producers,
            configs=[],selected=[],completed=[],opened=False,expected=expected,blocking_started=True,
            allow_path='/run/schip-package-upgrade-'+self.work.name)
        self.data['config_before']=self.configs();self.check_space();self.save()
        self.block();self.data['completed'].append('blocked');self.save()
        self.backup();self.data['completed'].append('backup');self.save()
        self.register_roots()
        self.snapshot_configs()
        self.data['config_expected']=self.configs();self.save()

    def register_roots(self):
        registry=self.h.etc/'schip/lock-map-roots';registry.parent.mkdir(exist_ok=True)
        lines=registry.read_text().splitlines() if registry.exists() else []
        for root in self.data['roots']:
            if root not in lines:lines.append(root)
        registry.write_text('\n'.join(lines)+'\n')

    def core_preinst(self):
        kit.require('backup' in self.data['completed'],'Backup frio nao concluido')
        if 'migrated' not in self.data['completed']:
            self.migrate();self.data['completed'].append('migrated');self.save()

    def validate(self):
        self.offline()
        self.h.native_ready(self.data['expected'])
        self.restore_configs()
        kit.require(self.configs()==self.data['config_expected'],'Configuracao mudou; servicos mantidos bloqueados')
        self.h.payload_ready(self.data['expected'])
        if not self.data['opened']:
            for root, expected in self.data['cold_data'].items():
                kit.require(kit.tree(Path(root),exclude_maps=True)==expected,'Dados mudaram durante transacao offline')
        if 'migrated' in self.data['completed']:
            self.helper().ready(roots=self.roots(),proc=self.h.proc)
        for unit,before in self.data['units'].items():
            wanted=before['UnitFileState']
            if wanted in ('enabled','disabled') and self.h.unit(unit)['UnitFileState']!=wanted:
                self.h.run(['systemctl','enable' if wanted=='enabled' else 'disable',unit])

    def finish(self):
        kit.require('backup' in self.data['completed'],'Backup nao concluido; nao retomar automaticamente')
        self.validate()
        if 'validated' not in self.data['completed']:
            self.data['completed'].append('validated');self.save()
        return self.start('builtin')


def legacy_state(host):
    folder=host.path('/var/lib/schip/apt-maintenance')
    expected={}
    if (folder/'expected').exists():
        for line in (folder/'expected').read_text().splitlines():
            name, version=line.split();expected[name.split(':')[0]]=version
    units=(folder/'units').read_text().splitlines() if (folder/'units').exists() else []
    return expected,units


def apt_plan(stream):
    kit.require(next(stream,'').strip()=='VERSION 2','Protocolo APT inesperado')
    for line in stream:
        if not line.strip():break
    result={}
    for line in stream:
        fields=line.split()
        if len(fields)==5 and fields[4]!='**REMOVE**':
            name=fields[0].split(':')[0]
            if name in COHORT:result[name]=fields[3]
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['pre','configured','finalize','apt','status'])
    parser.add_argument('argument',nargs='?')
    parser.add_argument('--target',default='')
    args=parser.parse_args()
    kit.require(os.geteuid()==0,'Execute como root')
    os.umask(0o077)
    stateparent=Path('/var/lib/schip/package-maintenance');stateparent.mkdir(parents=True,exist_ok=True,mode=0o700)
    kit.trusted_parents(stateparent)
    kit.require(stateparent.stat().st_uid==0 and not stateparent.stat().st_mode & 0o077,'Estado da manutencao deve ser root, modo 0700')
    with (stateparent/'lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        marker=Path(MARKER)
        expected={}
        if args.action=='apt' and args.argument not in ('post','resume'):
            expected=apt_plan(iter(sys.stdin))
            if not any(n in expected for n in COHORT if not n.endswith('-dev')):return
        if marker.exists():
            work=Path(json.loads(marker.read_text())['work'])
        elif args.action in ('pre','apt') and args.argument not in ('post','resume'):
            temporary=AutoHost(stateparent);config=load_config(temporary)
            helperpath=temporary.prepare_packages()
            hs=importlib.util.spec_from_file_location('locks',helperpath);helper=importlib.util.module_from_spec(hs);hs.loader.exec_module(helper)
            roots=helper.managed_roots()+[Path(p) for p in config.get('roots',[])]
            if not any(any(p.glob('*/sets/table')) for p in roots):return
            parent=Path(config.get('backup_parent','/var/backups/schip-package-upgrades'))
            kit.trusted_parents(parent);parent.mkdir(mode=0o700,exist_ok=True)
            kit.require(parent.stat().st_uid==0 and not parent.stat().st_mode & 0o077,'Diretorio de backups deve ser root, modo 0700')
            work=parent/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
            work.mkdir(mode=0o700)
            kit.durable_json(marker,dict(work=str(work)))
        else:
            return
        host=AutoHost(work);engine=Transaction(host)
        legacy,units=legacy_state(host);expected={**legacy,**expected}
        if args.action=='pre' and args.target:expected[args.argument]=args.target
        if args.action in ('pre','apt') and args.argument not in ('post','resume'):
            engine.prepare(expected,units,load_config(host))
            if args.action=='pre' and args.argument=='libschip':engine.core_preinst()
            return
        if args.action=='configured':return
        if engine.data is None:
            # prepare abortou antes do 1o save: nada foi bloqueado nem copiado
            if args.action=='status':
                print(json.dumps(dict(work=str(work),completed=None)));return
            kit.require(not any(work.iterdir()),'Marcador sem state.json mas com arquivos; preserve e investigue: '+str(work))
            work.rmdir();marker.unlink();kit.fsync_dir(marker.parent)
            print('schip-package-upgrade: transacao abandonada antes de bloquear servicos; marcador removido')
            return
        if args.action=='status':
            print(json.dumps(dict(work=str(work),completed=engine.data['completed'])))
            return
        try:engine.finish()
        except PendingTransaction as error:
            if os.environ.get('SCHIP_UPGRADE_TRIGGER')=='1':
                print('schip-package-upgrade: aguardando restante da transacao; servicos continuam bloqueados: '+str(error))
                return
            raise
        for name in ('units','expected'):
            host.path('/var/lib/schip/apt-maintenance/'+name).unlink(missing_ok=True)
        kit.durable_json(work/'result.json',dict(passed=True,version=engine.h.version('schipmyserver')))
        marker.unlink();kit.fsync_dir(marker.parent)
        print('schip-package-upgrade: backup, migracao, configuracao, SHA256 e SQL/Fleet validados; servicos retomados')


if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,ValueError,kit.subprocess.SubprocessError) as error:
        print('schip-package-upgrade: '+str(error)+'; mantenha os bloqueios e preserve o backup.',file=sys.stderr)
        sys.exit(1)
