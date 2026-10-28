PPC512 controlled USB fixture, 27 October 2026.

boundary/nested: successful native listings and reads after isolating the
non-ASCII-alias file in UNIBAD. failure: earlier native I/O error and clean
unmount, before that isolation. No kernel or firmware change was made.

*-video.bin: 4,000 actual bytes at B800:0000, rendered with the captured PPC
BIOS font by tools/render-text.py. *-screen.sh contains the native commands.
*-capture-manifest.json hashes the video, font and PNG. PNGs live one directory
above this evidence folder. *-screen.txt is decoded from the video cells.

v2-read-results.json: 15 SHA-256 checks of 14 distinct files, including a
case-variant read. v2-alias-results.json: six additional short-alias reads.
All match the Mac-created fixture bytes. The isolated known-failing Unicode
file is not included in those 21 successful reads. Two pmfstest invocations
also verify read chunk sizes, seek/EOF and refusal of write-only opens.

legacy-reply.bin: only the 84-byte failed legacy directory reply, captured
read-only from the running kernel's packet buffer using its exact system.map
and verified data segment. Its 11-byte FCB name includes UTF-8 c3 a9; the
77-byte size matches the failing Cafe-with-acute test file. The entire
containing directory scan is rejected by the ASCII-only client.

Mac AppleDouble metadata sidecars explain the underscore-prefixed files in
screenshots. They are not duplicate test payloads or truncated display names.

Only generated fixture data and diagnostic evidence are published here.
No firmware flash backup, credentials or existing USB file contents included.
