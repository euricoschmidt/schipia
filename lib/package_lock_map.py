#!/usr/bin/env python3
"""Offline retirement of v1 coordination maps; database records are never changed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import struct
import sys

def active_users(proc=Path('/proc'),roots=()):
    users=[]
    for p in proc.glob('[0-9]*'):
        try:
            maps=(p/'maps').read_text()
        except (FileNotFoundError,ProcessLookupError):
            continue
        except PermissionError:
            if p.exists():raise RuntimeError('Cannot inspect PID '+p.name+'; run with sufficient privileges')
            continue
        using=bool(re.search(r'/(?:libschip(?:fleet_auto)?\.so[^/\s]*|schipmyserverd)(?: \(deleted\))?$',maps,re.M) or re.search(r'\.locks(?: \(deleted\))?$',maps,re.M))
        using=using or any(str(r).replace(' ','\\040')+'/' in maps for r in roots)
        if roots and not using:
            try:
                for fd in (p/'fd').iterdir():
                    try:target=os.readlink(fd).removesuffix(' (deleted)')
                    except (FileNotFoundError,ProcessLookupError):continue
                    if any(target.startswith(str(r)+'/') for r in roots):using=True;break
            except (FileNotFoundError,ProcessLookupError):pass
        if using:
            users.append(p.name)
    return users

def offline(proc=Path('/proc'),roots=()):
    users=active_users(proc,roots)
    if users:raise RuntimeError('Stop all SQL, Fleet and C consumers first; active PIDs: '+','.join(users))

def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def managed_roots():
    roots={p.resolve() for p in Path('/var/lib').glob('schipmyserver*') if p.is_dir()}
    for conf in Path('/etc').glob('schipmyserver*.conf'):
        for line in conf.read_text().splitlines():
            if line.startswith('SCHIP_DBROOT='):
                words=shlex.split(line.split('=',1)[1],comments=True)
                if len(words)!=1 or not Path(words[0]).is_absolute():raise RuntimeError('Unrecognized SCHIP_DBROOT in '+str(conf))
                if Path(words[0]).is_dir():roots.add(Path(words[0]).resolve())
    extra=Path('/etc/schip/lock-map-roots')
    if extra.exists():
        for line in extra.read_text().splitlines():
            if not line.strip() or line.lstrip().startswith('#'):continue
            path=Path(line.strip())
            if not path.is_absolute() or not path.is_dir():raise RuntimeError('Invalid registered database root: '+line)
            roots.add(path.resolve())
    return sorted(roots)

def ready(roots=None,proc=Path('/proc')):
    roots=managed_roots() if roots is None else roots
    report=retire(roots,proc=proc)
    if report['retired']:raise RuntimeError('Legacy lock maps remain; run lock-map-upgrade.py offline with explicit roots and a verified backup before installing 0.5.22')

def retire(roots,backup=None,apply=False,proc=Path('/proc')):
    roots=[Path(r).resolve(strict=True) for r in roots]
    offline(proc,roots)
    if len(set(roots))!=len(roots):raise RuntimeError('Duplicate roots')
    plan=[]
    for index,root in enumerate(roots):
        for path in sorted(root.glob('*/sets/table.locks')):
            if path.is_symlink() or not path.is_file() or path.resolve()!=path:raise RuntimeError('Unexpected lock-map path: '+str(path))
            with path.open('rb') as stream:head=stream.read(8)
            if len(head)!=8:raise RuntimeError('Invalid lock-map header: '+str(path))
            magic,version=struct.unpack('<II',head)
            if magic!=0x544c4b31 or version not in (1,2):raise RuntimeError('Unknown lock-map format: '+str(path))
            if version==1:
                st=path.stat()
                plan.append(dict(path=str(path),relative=str(Path(str(index))/path.relative_to(root)),sha256=digest(path),uid=st.st_uid,gid=st.st_gid,mode=st.st_mode & 0o777))
    report=dict(roots=list(map(str,roots)),apply=apply,retired=plan,data_files_changed=False)
    if not apply:return report
    if not backup:raise RuntimeError('--backup is required with --apply')
    backup=Path(backup).resolve()
    if any(backup==r or r in backup.parents for r in roots):raise RuntimeError('Backup must be outside database roots')
    backup.mkdir(mode=0o700,parents=False,exist_ok=False)
    for item in plan:
        destination=backup/item['relative'];destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(item['path'],destination)
        with destination.open('rb') as stream:os.fsync(stream.fileno())
        if digest(destination)!=item['sha256']:raise RuntimeError('Backup verification failed')
    receipt=backup/'manifest.json'
    receipt.write_text(json.dumps(report,indent=2)+'\n')
    with receipt.open('rb') as stream:os.fsync(stream.fileno())
    for folder in sorted({p.parent for p in backup.rglob('*')} | {backup,backup.parent},key=lambda p:len(p.parts),reverse=True):
        fd=os.open(folder,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)
    offline(proc,roots)
    for item in plan:
        if digest(Path(item['path']))!=item['sha256']:raise RuntimeError('Map changed after planning')
    for item in plan:
        path=Path(item['path']);path.unlink()
        fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check-active',action='store_true')
    p.add_argument('--check-ready',action='store_true')
    p.add_argument('--root',action='append',type=Path,default=[])
    p.add_argument('--backup',type=Path);p.add_argument('--apply',action='store_true')
    a=p.parse_args()
    try:
        if a.check_active:offline();return
        if a.check_ready:ready(a.root or None);return
        if not a.root:p.error('at least one --root is required')
        print(json.dumps(retire(a.root,a.backup,a.apply),indent=2))
    except (OSError,RuntimeError) as error:
        print('schip lock-map upgrade refused: '+str(error),file=sys.stderr);raise SystemExit(1)

if __name__=='__main__':main()
