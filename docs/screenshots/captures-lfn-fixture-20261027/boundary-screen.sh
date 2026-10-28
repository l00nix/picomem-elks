#!/bin/sh
echo 'PicoMEM / ELKS - filename boundaries, 27 October 2026'
echo '# mount'
mount
echo '# cd /mnt/usb/LFN271A'
cd /mnt/usb/LFN271A || exit 1
echo '# ls'
ls || exit 1
