#!/bin/sh
echo 'PicoMEM / ELKS - nested reads and Unicode fallback'
echo '# mount'
mount
echo "# cd '/mnt/usb/LFN271A/Nested Long Folder/Deeper Long Folder'"
cd '/mnt/usb/LFN271A/Nested Long Folder/Deeper Long Folder' || exit 1
echo '# cat "Nested Read.txt"'
cat 'Nested Read.txt' || exit 1
cd / || exit 1
echo '# ls -l /mnt/usb/LFN271A/UNISAFE'
ls -l /mnt/usb/LFN271A/UNISAFE || exit 1
echo '# cat /mnt/usb/LFN271A/UNISAFE/UNICOD~2.TXT'
cat /mnt/usb/LFN271A/UNISAFE/UNICOD~2.TXT || exit 1
