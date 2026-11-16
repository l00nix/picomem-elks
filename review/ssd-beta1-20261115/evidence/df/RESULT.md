# Read-only df timing result

The single no-argument `df` completed on PMBLUE mounted read-only through the unchanged Beta 1 hotplug pair. PPC-local start/end records show 179 seconds (one-second resolution; the PPC clock date is unset). The remote command returned exit 0 after 183.681 seconds, including shell and connection overhead. The FAT row was exactly 49,152 total, 13,475 free and 35,677 used 1 KiB blocks (73%); stderr was empty. Exit status alone was not used as the success criterion.

Before the scan, three reference files, including the 40,997-byte binary and current hp1115 proof, matched their independently verified hashes. The command ran once. Its script and all output records were stored on the internal root filesystem; no USB writes, kernel changes, firmware changes or reboots occurred. Storage diagnostics remained clear and final mounts show only the root filesystem. PMBLUE remains directly attached and safely unmounted.

The telnet completion was received without a reported connection failure. This establishes that this session survived this particular long read-only scan; it does not establish continuous availability of other network services.

The passive capture is incomplete for timing interpretation: its 40 packets cover only the first 0.725920 seconds and omit completion traffic, even though the capture process ran for 183.840 seconds and reported zero kernel drops. Early command/queued-marker acknowledgments were captured. Capture buffering at termination is a possibility, not a demonstrated cause; no conclusion about TCP retransmissions or network starvation during the remainder of the scan is justified. Original evidence is preserved and the scan was not repeated.

This confirms a slow native FAT free-space scan, not the root cause of the earlier mkdir errno60 failure. A read-only statfs scan differs from allocation, locking and metadata writes. The original write run remains failed/incomplete with unknown mkdir exit; its separately verified continuation and independent media-integrity checks remain valid. No speculative production change follows from this diagnostic.
