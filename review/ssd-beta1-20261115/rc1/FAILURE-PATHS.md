# Failure paths and limits

| Condition | Candidate behavior | Evidence / limitation |
|---|---|---|
| Raw offset reaches 64 MiB | Return EFBIG before conversion to the native 16-bit block index; a crossing request may return only its valid prefix | Actual-function host boundary tests and target emulator boundary test; no address-width expansion |
| Unsupported FAT32 active FAT | Refuse both RO and RW mount when mirroring is disabled | Geometry host and emulator tests; reading stale FAT0 is not permitted |
| Unsupported FAT geometry | Refuse sector-size mismatch, nonrepresentable metadata layout, zero/invalid cluster size, zero data clusters, unsupported FAT32 version, or out-of-range root | Before storing narrowed fields or preparing FSInfo; malformed fixture remains byte-exact |
| FAT32 reserved entry bits | Read only low 28 bits and preserve each copy's upper four bits on modification | Actual FAT-access host tests and reserved-nibble emulator write/remount/fsck |
| FSInfo missing or unknown | Preserve permitted unknown hints; invalidate known hints before enabling writes | 31 host scenarios and RO/RW/remount emulator cases |
| FSInfo invalid location/read/write failure | Refuse RW admission; RO remains available where ordinary geometry is valid | Actual-function error injection; target out-of-bounds pointer case |
| USB missing, bad signature, wrong sector size or capacity | Fail initialization; no USB backend usable in that boot | Actual backend and firmware host tests |
| Wrong stage/count, short/error completion, bad CSW, deadline expiry | Fail, latch the backend/transport failure; do not retry/reuse an uncertain exchange | Backend, MSC, cache-policy and PIO fault tests; read caller buffer is copied only after successful completion |
| Completed command error | Clear only the issued command's completed error so BIOS SD may continue; USB backend remains failed | Backend fault tests |
| Pending exchange or missing physical handshake | Do not reset ownership underneath it; retain existing finite/fatal fence policy | Host tests; real electrical timing and fatal hardware behavior remain separate qualification concerns |
| Removal/replacement | Outside supported lifecycle; faults require a cold start with the intended medium attached | Source review; no automatic remount/recovery claim |
| Unknown/header-only cache policy | The specific well-formed header-only reply permits assumed write-through; malformed or transport-failed replies do not | 532 cache cases; an assumption, not proof of durable storage |
| Known enabled write cache | Require successful preparation flush and post-write flush within the shared deadline | Host error cases; a physical explicit-WCE=1 device remains unqualified |
| Native buffered write or sync returns success | May precede physical completion; inherited ELKS sync/error semantics can hide later storage errors | Source review; no application-level durable-success claim |
| Power loss or unplug during writes | No atomicity, automatic repair or filesystem consistency guarantee | Unsupported test-use behavior; preserve backups and independently check a suspect medium |

The raw 64 MiB boundary is specific to the generic raw-file path. It is not a statement that all FAT filesystem access or physical drives are limited to 64 MiB. Beta 1 testing remains confined to the stated 8/48 MiB fixtures; larger layouts are not qualified.

A mount failure is not permission to format or repair. On any storage error, stop writes, retain logs, and avoid retrying or unplugging while ownership/completion is uncertain. A clean `sync` exit alone is insufficient evidence. The qualification workflow uses unmount, remount/readback, then an independent full-image, FAT-table, allocation and guard check after a safe hardware handoff.

Known inherited buffered-error behavior is documented rather than hidden behind a new error-accounting subsystem. This candidate is suitable for controlled tests on backed-up media after its hardware gates, not a claim of general release readiness.
