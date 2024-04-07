#!/bin/sh

ifconfig lo 127.0.0.1
route add -net 127.0.0.0 netmask 255.255.255.0 lo

./heartbleed 9878 ./cert.crt rsa_private.key -dasics &

sleep 1

./attack 127.0.0.1 -p 9878