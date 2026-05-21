## Documentation Structure

`docs/v2/` has three directories with distinct roles:

- **`设计方案/`** — Long-term architecture and system design. These documents represent considered decisions with lasting value. Read relevant ones before implementing any non-trivial feature.
- **`阶段记录/`** — Phase-scoped plans, acceptance criteria, and historical context. Read the relevant phase plan before starting a sub-phase.
- **`参考资料/`** — Supporting research, asset inventories, migration notes.

**When implementing, treat `设计方案/` as authoritative.** The designs there describe module boundaries, protocol contracts, manifest schemas, and runtime behavior that other components depend on. Don't silently deviate.

**If a design document appears wrong or outdated during implementation:**
- Stop and point it out explicitly.
- State what the document says, what the code reveals, and what you think should change.
- Don't implement a workaround and move on — the document is the source of truth until it's updated.
