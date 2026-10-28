# Console component conventions

- Keep implementation APIs, paths, macros and labels semantic; donor product
  identities belong in source provenance, legal notices and migration records.
- Edit shared byte components in the canonical PortableShell repository, then
  use the sync tool and exact-source lock. Do not maintain divergent copies.
- Keep SDK and host calls in adapters. The console remains fixed-storage C99;
  the optional snapshot/action view may use freestanding C++11.
- Preserve command behavior and explicit compatibility guards when renaming.
- Retain original Git history, licenses and contributor attribution.
- Build and simulator proof is separate from physical and human acceptance.
