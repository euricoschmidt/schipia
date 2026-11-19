#!/bin/sh
set -eu
cd "$(dirname "$0")"
curl -fL --retry 3 'https://deb.schip.com.br/kits/schipia/0.14.0%2Bschip1/schipia_0.14.0%2Bschip1_amd64.deb' -o 'debs/schipia_0.14.0+schip1_amd64.deb.tmp'
echo "1d76b7de83a286f20262952d7a7bfcb2b57e1d5a7908823d93837f787e2a5a5f  debs/schipia_0.14.0+schip1_amd64.deb.tmp" | sha256sum -c -
mv 'debs/schipia_0.14.0+schip1_amd64.deb.tmp' 'debs/schipia_0.14.0+schip1_amd64.deb'
