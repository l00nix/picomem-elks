#!/bin/sh
cd /
echo 'PicoMEM / ELKS - controlled LFN test, 27 October 2026'
echo '# mount'
mount
echo '# ls -ld /mnt/usb/LFN271A'
ls -ld /mnt/usb/LFN271A
echo '# cat /mnt/usb/LFN271A/CONTROL.TXT'
cat /mnt/usb/LFN271A/CONTROL.TXT
echo '# cat /mnt/usb/patchstick.sh > /dev/null'
cat /mnt/usb/patchstick.sh > /dev/null
echo "Existing file read status: $?"
echo '# pmmount -u /mnt/usb'
pmmount -u /mnt/usb || exit 1
echo '# mount'
mount
