#!/bin/sh

rm lib* -f
cp /home/wanghan/Workspace/heartbleed-nest/build/lib/libssl.so.1.0.0 ./
cp /home/wanghan/Workspace/heartbleed-nest/build/lib/libcrypto.so.1.0.0 ./


cp /home/wanghan/Workspace/heartbleed-nest/build/heartbleed ./

cp /home/wanghan/Workspace/heartbleed-nest/build/attack-heartbleed ./attack

cp /home/wanghan/Workspace/heartbleed-nest/build/cert.crt ./

cp /home/wanghan/Workspace/heartbleed-nest/build/rsa_private.key ./

cp /home/wanghan/Workspace/heartbleed-nest/build/runall ./


cp /home/wanghan/Workspace/heartbleed-nest/run_heartbleed.sh ./
