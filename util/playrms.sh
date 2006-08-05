#!/bin/bash

rm audiodump.wav
mkfifo audiodump.wav
./wavrms audiodump.wav &
mplayer $1 -vc null -vo null -ao pcm:fast
rm audiodump.wav
