# Read-only diagnostic completed; root cause remains open

The planned single plain df test is complete; see df-timing-hardware/RESULT.md. Its 179-second local scan and successful remote completion are recorded separately from the incomplete packet trace. PMBLUE is directly connected to PicoMEM and safely unmounted. No further hardware action or production patch is justified by this result alone.

Preserve the original failed mkdir run and its separately verified continuation. The reviewer packet should report slow FAT scanning and the unresolved connection loss explicitly. Any future packet-timing experiment must first validate end-to-end capture coverage, including completion frames, without replaying USB mutations. Do not treat tcpdump process duration or zero drops as proof of trace completeness.

The sealed source and earlier hardware archives remain unchanged; this diagnostic is a separate evidence addendum.
