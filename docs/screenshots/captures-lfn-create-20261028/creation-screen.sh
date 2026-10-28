#!/bin/sh
echo 'PicoMEM / ELKS: long-name USB creation and writes'
echo '# mount'
mount
echo
echo '# cd "/mnt/usb/LFN Create 28"'
cd '/mnt/usb/LFN Create 28' || exit 1
echo '# ls'
ls || exit 1
echo '# wc -c < "Created Long Filename.txt"'
wc -c < 'Created Long Filename.txt' || exit 1
echo '# wc -c < "N26-xxxxxxxxxxxxxxxxxx.txt"'
wc -c < 'N26-xxxxxxxxxxxxxxxxxx.txt' || exit 1
echo '# cd "Nested Long Directory"'
cd 'Nested Long Directory' || exit 1
echo '# ls'
ls || exit 1
echo '# wc -c < "Child With Spaces.txt"'
wc -c < 'Child With Spaces.txt' || exit 1
