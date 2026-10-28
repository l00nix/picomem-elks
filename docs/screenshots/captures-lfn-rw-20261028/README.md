# PPC512 existing long-name write evidence, 28 October 2026

The final candidate is identified by `build-manifest.json`; `kernel.config`
includes the PPC display extension. `first-boot-final.log` contains boot,
mount and native unsupported-alias safeguards. `writes-final.log` and
`remount-final.log` contain the existing-file write/read checks.

`after-write-final-readbacks.json` and `after-remount-final-readbacks.json`
record independent host verification through long paths and aliases.
`restored-final-readbacks.json` matches the original fixture hashes after
restoration and a further unmount/remount. `restore-final.log` records cleanup.
`create-delete-final.log` and `keep-final-readback.json` cover short-name
creation in a long-named parent on the final kernel. The earlier `keep-readback`
records that the first candidate's test file also survived the kernel reboot.

`writes-video.bin` is the actual 4,000-byte B800:0000 text screen captured by
ELKS `dd`; `writes-screen.sh` contains its native display commands. It was
rendered with `tools/render-text.py` and the captured BIOS font identified in
`writes-capture-manifest.json`. No command output was synthesized.
`writes-screen.txt` is its decoded text; the PNG is one directory above.

`failed-packet.bin` is the bounded legacy DFS reply for the non-ASCII Mac
sidecar that exposed the first candidate's bug. Its exact FCB is now a host
regression fixture. It is failure evidence, not a final-candidate failure.
