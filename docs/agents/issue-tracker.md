# Issue Tracker

Issues for this repo live in **`todo.md`** at the repo root. There is no external tracker; do not use GitHub Issues.

## Structure

`todo.md` is divided by horizontal-rule headers into priority tiers (`# Required for MVP`, `# Post MVP`), each containing routing sections:

- **`# Claude Friendly`** — tasks an agent can complete autonomously. This is the agent work queue (the `overnight-todos` skill consumes it).
- **`# Needs human`** — tasks requiring the user's hands/eyes (audio, art judgment, in-editor authoring).
- **`# Needs design`** — tasks blocked on a design decision; not actionable until the user resolves them.

Items are checkbox lines: `[ ]` open, `[x]` done. Sub-details and outcomes are indented lines beneath the parent item. Completed items stay in place (marked `[x]`) as a record rather than being deleted.

## How skills should use it

- **Creating an issue** (`to-tickets` etc.): append a `[ ]` line to the appropriate section — `# Claude Friendly` if an agent can do it end-to-end, `# Needs human` or `# Needs design` otherwise. Default to the `# Required for MVP` tier unless it's clearly post-MVP polish.
- **Reading/selecting work**: pick from `# Claude Friendly` under `# Required for MVP` first.
- **Closing an issue**: flip `[ ]` to `[x]` and, when the resolution isn't obvious, add a short indented note recording what was done (see existing entries for the style).
- **Specs**: for anything bigger than a one-liner, keep the `todo.md` line short and put the spec elsewhere (e.g. a `docs/` file), linked or named from the item.
