---
name: retarget-mocap
description: Retarget a UE5-Mannequin-skeleton mocap animation (e.g. mocapcentral) onto a MetaHuman and swap it into a target AnimBlueprint. Use when the user wants to add, retarget, or replace an NPC animation.
---

`scripts/local/retarget_mocap_to_metahuman.py` retargets a UE5-Mannequin-skeleton
anim (e.g. mocapcentral) onto a MetaHuman and swaps the matching SequencePlayer
nodes in a target AnimBlueprint. Idempotent. Edit the constants at the top for a
new animation. Full workflow + Python API gotchas: `docs/animation-retargeting.md`.
