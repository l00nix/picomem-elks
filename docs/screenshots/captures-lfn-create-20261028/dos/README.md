# DOS compatibility evidence

This is a PPC512 test of the long-name creation firmware under MS-DOS 6.22
with the 4DOS 8.00 shell. The patched COMrade agent provides DOS file operations,
serial key injection and a native screen capture. The ROM-marker read confirms
that the metadata and creation descriptors were both present during this test.

The read report includes full-file SHA-256 comparisons through the three short
aliases, directory entries and a successful HDD root directory enumeration.
The write report covers a fresh `G:\LFNCRE~1\DOSRW28.TMP`: 1,025 pattern bytes
`(index * 17 + 3) & 255`, followed by 513 appended bytes
`(index * 31 + 9) & 255`, then truncation/rewrite to the 31-byte ASCII string
`DOS short-name truncate check` followed by CR/LF. Each phase was downloaded and
compared completely, then checked with a DOS-side CRC-32 and length. Only that
temporary file was deleted. The three retained ELKS fixtures were downloaded
again and their hashes remained unchanged.

The screenshot shows native DOS version and alias directory listings, captured
before the temporary write test. It does not depict synthetic test results.
The retained read-only fixture payloads and hashes are the same as in the ELKS
and direct Mac checks. This is a bounded compatibility check, not comprehensive
DOS/device certification or a test of native DOS long-filename support.
