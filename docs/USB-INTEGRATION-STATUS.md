# Public USB integration discontinued

As of 16 November 2026, public development and upstream integration of PicoMEM USB filesystem access for ELKS have been discontinued. The author is continuing experimental USB work privately for personal use.

This public repository now contains the configuration and diagnostic tools: `pmconfig` (also `pmcfg`), `pmdump`, `pmstat` and `pmmem`, with their source, tests, utility binaries and supporting documentation. `pmstat --usb` only queries firmware inventory and remains part of the diagnostic tools.

The filesystem experiments, custom kernels, firmware/TinyUSB storage patches, USB/mouse driver experiments and related test packages have been removed from the current branch after preserving them in a private repository. The configuration-tool implementation and binaries are unchanged. No PPC kernel, firmware or connected media were changed as part of this split.

Existing Git history has not been rewritten. Older commits, previously published review links, upstream PRs/comments and external clones may still contain the earlier work. Those historical snapshots do not constitute a supported release. This notice records the repository split; it does not claim that upstream maintainers have closed their discussions or issues.

Thank you to the ELKS, PicoMEM and TinyUSB communities for their projects and feedback. The decision to narrow this repository's scope is the author's.
