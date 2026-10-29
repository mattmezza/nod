# SheenBidi source receipt

Upstream remains commit `cfe430e7375a7845b679adae9d51dac6deaa8858` from
https://github.com/Tehreer/SheenBidi/tree/cfe430e7375a7845b679adae9d51dac6deaa8858.
The Apache-2.0 license in LICENSE remains unchanged.

Local patch: Source/API/SBParagraph.c explicitly consumes each assertion-only
`stringLength` with `(void)stringLength`. Both original debug range assertions
remain intact. NDEBUG removes assertions without leaving an unused local under
GCC 16 Release warnings. No upstream version bump or runtime algorithm change.

Patched SHA-256: `4e94023c95941609db1abe93de8ed6f75f095ded4a56a745f11df1f3a3b54cbe`.
The integrator updates third_party/dependencies.json alongside this receipt.
