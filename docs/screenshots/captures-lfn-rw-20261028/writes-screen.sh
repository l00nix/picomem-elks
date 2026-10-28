#!/bin/sh
echo 'PicoMEM / ELKS: existing long-name USB writes'
echo '# mount'
mount
echo
echo '# cd /mnt/usb/LFN271A'
cd /mnt/usb/LFN271A || exit 1
echo '# ls -l "Name With Spaces.txt"'
ls -l 'Name With Spaces.txt' || exit 1
echo '# ls "N26-xxxxxxxxxxxxxxxxxx.txt"'
ls 'N26-xxxxxxxxxxxxxxxxxx.txt' || exit 1
echo '# wc -c < "N26-xxxxxxxxxxxxxxxxxx.txt"'
wc -c < 'N26-xxxxxxxxxxxxxxxxxx.txt' || exit 1
echo '# cd "Nested Long Folder/Deeper Long Folder"'
cd 'Nested Long Folder/Deeper Long Folder' || exit 1
echo '# ls -l "Nested Read.txt"'
ls -l 'Nested Read.txt' || exit 1
