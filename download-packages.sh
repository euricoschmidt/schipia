#!/bin/sh
set -eu
cd "$(dirname "$0")"
curl -fL --retry 3 'https://deb.schip.com.br/kits/schipia/0.14.1/schipia_0.14.1_amd64.deb' -o 'debs/schipia_0.14.1_amd64.deb.tmp'
echo "0b383b49e6fd3e3f9ec8378bb9e3f249bd943e84944f3d33247f9827634f6450  debs/schipia_0.14.1_amd64.deb.tmp" | sha256sum -c -
mv 'debs/schipia_0.14.1_amd64.deb.tmp' 'debs/schipia_0.14.1_amd64.deb'
