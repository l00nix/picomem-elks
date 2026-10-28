#!/bin/sh
echo 'PicoMEM / ELKS: long names and 8.3 aliases'
echo '# mount'
mount
echo '# cd /mnt/usb/LFNCRE~1'
cd /mnt/usb/LFNCRE~1 || exit 1
echo '# ls'
ls || exit 1
echo '# cmp "Created Long Filename.txt" CREATE~1.TXT'
cmp 'Created Long Filename.txt' CREATE~1.TXT || exit 1
echo '# cmp "N26-xxxxxxxxxxxxxxxxxx.txt" N26-XX~1.TXT'
cmp 'N26-xxxxxxxxxxxxxxxxxx.txt' N26-XX~1.TXT || exit 1
echo '# cd "Nested Long Directory"'
cd 'Nested Long Directory' || exit 1
echo '# cmp "Child With Spaces.txt" CHILDW~1.TXT'
cmp 'Child With Spaces.txt' CHILDW~1.TXT || exit 1
