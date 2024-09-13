#!/bin/sh

set -e

[ -e Pico_flashCPR ] \
	|| git clone https://gitee.com/rtthread/rt-thread.git Pico_flashCPR

cd Pico_flashCPR

git reset --hard f5156774b2b7bc73278d602dd027ac4d61349e3f

cp ../main.c ./bsp/raspberry-pico/applications/
cp ../config ./bsp/raspberry-pico/.config
cp ../rtconfig.h ./bsp/raspberry-pico/
