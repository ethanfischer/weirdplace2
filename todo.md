# Todo / Bugs

## Bugs

- **Q doesn't exit inspection after Seneca convo + inventory opens**
  After collecting 3 movies and talking to Seneca, inspecting another movie fails to exit on Q press — the box just resets its rotation to default instead. Suspected cause: the inventory room teleport or Seneca dialogue leaves stale input bindings or ignores `SetIgnoreMoveInput`/`SetIgnoreLookInput` state, so `StopInspection` is being called but something is re-triggering the inspect or the binding is doubled up.

- **Bladder urgency vignette fires during Seneca dialogue**
  The bladder pulse triggers while the Seneca dialogue is playing. It shouldn't — dialogue should suppress or pause the bladder urgency timer.

- **Seneca conversation loops — gives shopping basket repeatedly**
  Talking to Seneca multiple times restarts the conversation and gives the player the shopping basket again. The handoff should only happen once; subsequent interactions should either do nothing or play a different response.
