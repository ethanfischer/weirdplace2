# Issue Tracker

Issues live on the **Trello board "weirdplace"**: https://trello.com/b/apYW69HZ/weirdplace
(`todo.md` is deprecated — do not add to it. There is no GitHub Issues usage either.)

The user designs and ideates cards in Claude chat / the Trello app; agents pull work from the board via the Trello MCP tools (`trelloReadCard`, `trelloWriteCard`, ...).

## IDs

- Board ARI: `ari:cloud:trello::board/workspace/5ed4124b989f0577a9368e25/666cb98434912b561d2bd45f`
- Board URL (accepted by most read tools): `https://trello.com/b/apYW69HZ/weirdplace`

## Structure

Kanban lists: **Todo** → **Doing** → **Done**.

Routing (which used to be `# Claude Friendly` / `# Needs human` / `# Needs design` sections in todo.md) is by **label color**:

| Color  | Meaning | Label ARI |
|--------|---------|-----------|
| green  | Claude Friendly — an agent can do it end-to-end (the agent work queue) | `ari:cloud:trello::label/workspace/5ed4124b989f0577a9368e25/666cb9841dc51400eb0d064b` |
| red    | Needs human — user's hands/eyes (audio, art judgment, in-editor authoring) | `ari:cloud:trello::label/workspace/5ed4124b989f0577a9368e25/666cb9841dc51400eb0d0652` |
| yellow | Needs design — blocked on a design decision; not actionable yet | `ari:cloud:trello::label/workspace/5ed4124b989f0577a9368e25/666cb9841dc51400eb0d0651` |

An unlabeled card is unrouted — triage it (attach the right label) when touching it.

## How agents should use it

- **Selecting work**: pick green-labeled cards from **Todo** (top of list = higher priority). Move the card to **Doing** when starting.
- **Creating an issue**: create a card in **Todo** with the appropriate color label. Card name = short imperative summary; details/spec go in the card `desc` (or a `docs/` file linked from the desc for anything big).
- **Closing an issue**: move the card to **Done** and, when the resolution isn't obvious from the diff, add a short note to the desc recording what was done.
- **Due-date queries**: call `trelloReadMember` (`get_me`) first for the user's timezone.

## Known gaps

- MCP cannot create/rename labels or comment on cards — desc edits stand in for comments.
- The `overnight-todos` skill still reads todo.md's `# Claude Friendly` section; until it's updated, treat green **Todo** cards as its queue.
