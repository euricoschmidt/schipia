#!/bin/sh
set -eu
cd "$(dirname "$0")"
curl -fL --retry 3 'https://deb.schip.com.br/kits/schipia/0.15.0/schipia_0.15.0_amd64.deb' -o 'debs/schipia_0.15.0_amd64.deb.tmp'
echo "e0d09603605a7c442ef3380d192b71bdb0c7ed0ce0ec6114e3ba2d2c07352550  debs/schipia_0.15.0_amd64.deb.tmp" | sha256sum -c -
mv 'debs/schipia_0.15.0_amd64.deb.tmp' 'debs/schipia_0.15.0_amd64.deb'
