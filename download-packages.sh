#!/bin/sh
set -eu
cd "$(dirname "$0")"
curl -fL --retry 3 'https://deb.schip.com.br/kits/schipia/0.15.1/schipia_0.15.1_amd64.deb' -o 'debs/schipia_0.15.1_amd64.deb.tmp'
echo "78907006b473a10aaaabe35cbfee931019a06693df84e14d09a8e4a470ce6cb4  debs/schipia_0.15.1_amd64.deb.tmp" | sha256sum -c -
mv 'debs/schipia_0.15.1_amd64.deb.tmp' 'debs/schipia_0.15.1_amd64.deb'
