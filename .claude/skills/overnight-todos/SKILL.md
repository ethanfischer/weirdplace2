---
name: overnight-todos
description: >-
  Autonomously work through the "# Claude Friendly" section of todo.md overnight
  using red-green-refactor TDD against the project's E2E harness, verifying each
  change with headed screenshots, then leave a NIGHTLY_REPORT.md handoff. Use
  this whenever the user wants to run unattended/overnight, "knock out todos
  while I sleep", "TDD the todo list", do an "overnight run", or hand off a batch
  of Claude-Friendly todos for autonomous work. The kickoff is interactive (scope
  is negotiated with the user up front); everything after that runs without
  intervention. Do NOT use this for working a single todo interactively — that's
  the plain `/todo` command.
---

# Overnight Todos

Run unattended through the Claude-Friendly backlog while the user sleeps. The
contract: **one short interactive kickoff to agree on scope, then full autonomy.**
Every item is driven test-first (red → green → refactor) against the project's
E2E harness, and every "green" is confirmed by reading an actual rendered
screenshot — not just a passing log line. Work happens on a dedicated overnight
branch with one commit per completed item, so each green is a restore point you
can reset to when a later item goes sideways. The morning handoff is that branch
plus `NIGHTLY_REPORT.md`, and the run ends by offering to squash-merge it back
into the branch the user started from.

The mechanical recipe for building, running E2E, screenshots, and the known UE
5.7 gotchas lives in **`references/e2e-harness.md`** — read it before the first
build, and re-consult it whenever a build or test behaves unexpectedly.

---

## Phase 0 — Kickoff (the only time you talk to the user)

This is the single human touchpoint. Get scope nailed down, then go dark.

1. **Read `todo.md`** and extract the `# Claude Friendly` section — every line
   from the `# Claude Friendly` heading down to the next `# ` heading. Top-level
   items are `[ ]` lines; their indented `[ ]` lines are sub-tasks of that item.
   Ignore items already marked `[x]`.

2. **If the section has no open items**, tell the user it's empty and ask whether
   they want help with `# Needs human`, `# Needs design`, or `# Stretch goals`.
   Be honest that those are categorized as *not* autonomously completable — they
   need their decisions, assets, or animation work — so an overnight run can at
   best scaffold or investigate them, not finish them. Let the user redirect.

3. **If there are open items**, list them back to the user as numbered candidates
   (top-level item + its sub-bullets), and for each give a one-line read on how
   you'd verify it (E2E-assertable? screenshot-only? not autonomously
   verifiable?). Then **agree on scope**: which items, in what order. Set
   expectations plainly — each item is a full build + E2E + screenshot cycle, so
   it's roughly 15–40 min of wall-clock each, and a UE rebuild is the slow part.
   Recommend ordering cleanly-E2E-testable items first so the night front-loads
   verifiable wins.

4. **Lock it in.** Record the branch the user is currently on
   (`git rev-parse --abbrev-ref HEAD`) — that's the *home branch* you'll offer to
   merge back into. Cut a dedicated overnight branch off it:
   `git checkout -b overnight/<YYYY-MM-DD>`. Create a task list (TaskCreate) with
   one task per agreed item so progress is trackable, restate the agreed scope and
   ordering in one message, name the home and overnight branches, and tell the
   user you're going autonomous now and they'll find `NIGHTLY_REPORT.md` in the
   morning. This message is the last thing they see until then.

After this point, **do not ask the user anything.** If you'd normally stop to
ask, instead make the most reasonable call, leave the work as WIP, write down the
decision and its uncertainty in the report, and move on.

---

## Phase 1 — Go autonomous

Before the first item: **close the editor** so E2E builds can link
(`taskkill //F //IM UnrealEditor.exe` — ignore "not found"). E2E builds need the
on-disk DLL and `run_e2e.ps1` spawns its own headless instance, so a live editor
only gets in the way. See `references/e2e-harness.md` for why Live Coding is not
enough here.

Then work the agreed items **one at a time**, each through the loop below. Mark
its task `in_progress` when you start, `completed` when its E2E is green and the
screenshot checks out (or `completed` with a blocked note if you had to leave it
as WIP).

---

## The red-green-refactor loop (per item)

The whole point of TDD here is that the test, not the implementation, is the
spec. Write the failing test first and **watch it fail for the right reason** —
that's what proves the test actually exercises the behavior. Skipping red is how
you end up with a green test that asserts nothing.

### 1. Frame the behavior
Restate the todo as one observable, in-game behavior an E2E test could witness.
"Looking at a gas-station light for 30s adds an item" → *after looking at the
light actor for 30s, the player's inventory count increases by one.* If you
genuinely can't phrase it as something the harness can observe (a pure pose/art
tweak with no logic), say so in the report and handle it as **screenshot-only**
verification, or flag it **not autonomously verifiable** and skip to the next
item rather than fake a test.

### 2. RED — write the test, make it fail at runtime
Add a new `IMPLEMENT_SIMPLE_AUTOMATION_TEST` in `E2E_Level1Test.cpp`
(`Weirdplace2.E2E.Level1.<Name>`), composing existing `FTD_*` latent commands and
step helpers. Take a screenshot at the moment of truth.

Crucial distinction: the test must **compile and fail on an assertion**, not fail
to compile. If the behavior needs a new *test* primitive (a new `FTD_` assert or
a new look-at helper), add that infrastructure now — but leave the *feature*
unimplemented, so red is a clean runtime assertion failure. If red instead comes
back as a build error, your test is referencing feature code that doesn't exist
yet; pull it back to asserting on observable state (inventory count, activity
state, on-screen text, screenshot) using primitives that already exist.

Build, run the test headed, and confirm it **fails** — and read the failure to
confirm it's failing because the behavior is absent, not because of a typo or a
broken teleport. A red test that fails for the wrong reason is worse than none.

### 3. GREEN — implement until it passes
Now write the feature: C++ in `Source/weirdplace2/`, or Blueprint/asset edits via
headless Python (`UnrealEditor-Cmd.exe -ExecutePythonScript`, since the editor is
closed). Build, run the test headed, confirm **PASS**.

Then **open the screenshot with the Read tool and actually look at it.** Green
logs plus a blank or wrong-looking frame means it's not really done — a passing
assertion can still sit on top of a visually broken result, and headless runs
produce blank screenshots, which is why every verification run is `-Headed`. The
screenshot is the part a sleeping user is trusting you to check.

### 4. REFACTOR — clean up, stay green
Tidy the implementation to match the codebase's conventions (forward-declare in
headers, null-check + early-return, `UPROPERTY()` on UObject pointers). Honor the
project rule against **fallback code**: if the happy path can't succeed, log and
error out — don't add a second branch of "just in case" logic that makes the code
harder to reason about. Re-run the test to confirm it's still green.

### 5. RECORD — commit the green
Check the item off in `todo.md` (flip its `[ ]` to `[x]`, sub-bullets too), append
it to `NIGHTLY_REPORT.md` (template below), then **commit everything for this item
to the overnight branch** in one commit:
```
git add -A && git commit -m "todo: <item summary> — E2E Weirdplace2.E2E.Level1.<Name> green"
```
This commit is a restore point: if a later item corrupts the tree you can
`git reset --hard` back to it. The invariant that makes that safe — **the overnight
branch tip must build and have green tests before you start the next item** — is
satisfied by a completed item. Within an item you may checkpoint-commit freely
(e.g. the red test) since it's your branch; just don't *leave* the tip red when you
move on. Never merge, squash, or push — that waits for the user (see the final
step).

---

## WIP & blocker handling

When an item can't be finished autonomously — it needs a human decision, a missing
asset, or the implementation defeats you after a real effort — **don't poison the
overnight branch for every later item.** Their E2E tests can't even run against a
broken-build tip, so a single stuck item would strand the rest of the night.

Preserve the attempt, then get the overnight branch tip back to its last green
state:
```
git add -A && git commit -m "WIP(blocked): <item-slug> — <one-line reason>"
git branch wip/<item-slug>          # park the attempt on its own branch
git reset --hard <last-green-commit> # overnight tip is buildable + green again
```
Record `wip/<item-slug>` and the reason in the report so the morning review knows
exactly where the partial work lives and how to resume it (`git checkout
wip/<item-slug>`). If the blocked attempt actually still *builds and passes*, you
don't need the side-branch dance — just leave its commit on the overnight branch
and note in the report that it's incomplete.

This is the payoff of committing per item: "leave WIP, move on" without letting one
bad item break the build under everything that follows.

---

## The morning handoff

Two artifacts plus the diff. Write `NIGHTLY_REPORT.md` at the repo root. Keep it
skimmable — the user reads it half-awake.

```markdown
# Nightly Report — <YYYY-MM-DD>

**Home branch:** <branch the user started on>
**Overnight branch:** `overnight/<YYYY-MM-DD>` (<N> commits)
**Scope agreed:** <the items, in order>
**Result:** N done · M blocked · K skipped

## Done — needs your eyes before merge
- ✅ <item> — `Weirdplace2.E2E.Level1.<Name>` green · commit `<short-sha>`.
  Screenshots: `E2E_<slug>_*.png` — <what you saw that confirms it>.
  **Verify in-game:** <the concrete repro — e.g. "look at the gas-station light
  for 30s, check inventory gained an item">.
  Notes / decisions made solo: <...>

## Blocked / WIP
- ⛔ <item> — <why>. Attempt parked on `wip/<slug>`. Touches: <files>.
  To resume: `git checkout wip/<slug>` — <the one concrete next step>.

## Skipped
- ⏭️ <item> — <reason it wasn't autonomously verifiable>.

## How to review this run
- `git log --oneline <home-branch>..overnight/<YYYY-MM-DD>` — the per-item commits
- `git diff <home-branch>..overnight/<YYYY-MM-DD>` — the full change
- Screenshots: `Saved/Screenshots/Windows/E2E_*.png`
- Parked attempts: `git branch --list 'wip/*'`

→ **Next:** <the single most valuable thing to pick up next>

---
**Not merged — waiting on your verification.** Green tests and my screenshots got
each item this far, but nothing lands on `<home-branch>` until *you* play it and
confirm. Walk the **Verify in-game** steps above, then tell me to squash-merge
`overnight/<YYYY-MM-DD>` into `<home-branch>`.
```

Finish by marking the task list done. That's the autonomous run.

## Final step — the merge handoff (when the user returns)

A passing E2E plus a screenshot I looked at is necessary but **not** sufficient —
the user is the final gate, and they verify by actually playing the build before
anything merges. So the run completes while they sleep, leaves the overnight branch
and report ready, and **stops without merging.** Don't fire a question into the
void at 4am.

When the user re-engages in the morning, lead with verification, not the merge:

1. **Help them verify first.** Point them at the **Verify in-game** repro steps in
   the report. If they want, reopen the editor and drive the repro (or hand them
   the steps for a PIE session) so they can see each item working for real. The
   editor was closed for the build loop, so a live check means launching it again.
2. **Only after they confirm it works**, offer the merge decision
   (AskUserQuestion). Don't pre-select "merge" or imply it's the obvious next
   click — an unverified squash is exactly what this gate exists to prevent.
   - **Squash-merge into `<home-branch>`** (the default the user asked for):
     `git checkout <home-branch> && git merge --squash overnight/<YYYY-MM-DD> && git commit`
     — collapses the night's per-item commits into one clean commit.
   - **Merge only the items they verified** — if some passed their check and others
     didn't, cherry-pick the good commits and leave the rest on the branch.
   - **Plain merge** — keep the per-item commits as history.
   - **Leave the branch as-is** — they'll review/merge themselves.
   - **Discard** — `git branch -D overnight/<YYYY-MM-DD>` (and any `wip/*`).

Never merge, squash, or delete branches without that explicit, post-verification
go-ahead.

---

## Autonomy rules (what never to do after kickoff)

- **Never ask the user to build, run a test, read a log, or check a screenshot.**
  You do all of it. Build yourself, run `run_e2e.ps1` yourself (in the background —
  runs take minutes), grep the log yourself, Read the screenshot yourself.
- **Never trust a green log alone** — a verification isn't done until you've looked
  at the rendered frame.
- **Never skip red.** If you're tempted to implement first, stop: the failing test
  is the only proof the test means anything.
- **Commit freely on the overnight branch** (one per green item) — that's the
  restore-point mechanism. But **never merge, squash, push, or delete a branch**
  without the user's morning go-ahead.
- **Keep the overnight branch tip buildable.** Park anything that would leave it
  red on a `wip/*` branch and reset back to the last green commit.
- **Never fake verifiability.** A todo with no observable behavior gets flagged,
  not wrapped in a hollow test.
