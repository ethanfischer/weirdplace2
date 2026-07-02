------------------------------------------
# Required for MVP
------------------------------------------

# Claude Friendly
[ ] have objects pass you by in driving scene so it actually looks like you're moving. Right now we just have the car staionary in the middle of nowhere. Feel free to use any meshes/blueprints in content that would make sense for this.
[x] seneca text illegible when viewing with light in background
[x] clock should be blurred out
[x] the fog wall and bladder indicator never look right on my first play after opening the editor. on the second play, they fix themselves. Is there some kinda shader/material compilation that only happens the first time I hit play after launching the editor fresh or something?
[x] Chosen movies show up as poster on telephone pole, in bathroom
[x] blur the background when in item inspection state
    
# Needs human
[ ] add lock mesh to employee door
[ ] review dialogue
[ ] seneca dialogue cigarrete tweaks
[ ] rick gas pump animation (or at least improve pose when he looks at you)
[ ] more weather sounds
[ ] more weather effects
[ ] sound mixing (tornado alert too loud)
    
    
# Needs design
[ ] message on payphone is code to employee bathroom
    [ ] you write it down (wrongly)
    [ ] you enter it wrongly, but it works
[ ] handle homeless scenario if you give money to Hudson. how to progress? 
    [ ] strange washer found later, but seneca accepts it as if it covers the 2.95 after given money to hudson
    
-------------------------------------------
# Post MVP
-------------------------------------------

# Claude Friendly

# Needs human
[ ] make gas station pumps interactable
[ ] make employee hallway have collectibles
[ ] add more collectibles in general
[ ] add lowpassfilter (lpf) to door lock sound
[ ] seneca unlock employee bathroom animation
[ ] more intricate soundscape espcially in oasis
    [ ] have random noises so the world feels more alive
[ ] door close sounds

# Needs design
[ ] make tv's gazereward component turn the tv off (with CRT static sound and visual)
[ ] decide what gaze rewards should be
[ ] put in eye contact given by seneca
    [ ] maybe after friend talks to him
[ ] replace llm responses to movies
[ ] allow walking away mid dialogue
[ ] century massage
    [ ] make seneca mention it or add some reference to it
[ ] replacing copywrighted content
[ ] "follow the money" (? high idea no idea)
[ ] There is no dying. only waking up in the other world
    [ ] 2 worlds that you progress in back and forth. One where the drive continues, one where you stop at the gas station
        [ ] eventually they converge. it's up to the player to interpret how these two wolds relate to each other
[ ] death state at beginning of game so player feels like there's stakes
    [ ] maybe rick asks if he should turn off his headlights. If you say yes, he crashes
    
    
# Review Claude's work
 [ ] 1. Movie posters — collecting movies now live-updates posters: first pick appears on the telephone pole's flyer sheet (it reuses the existing PosterSheet, gated with the phone reveal), second on a new plane in the employee bathroom. One decision to review: the pole sheet previously showed a designed "missing poster" — per our locked criteria it's now hidden until a movie is collected, so that art never shows. If you'd rather the flyer be the pre-collection state (pasted over by your first pick), it's a two-line change.
[ ] 2. Inspection blur — cinematic DoF ramps in while inspecting (item sharp, store bokeh'd), fully clears on exit, flatscreen-only.
[ ] 3. Seneca text — a translucent dark plate now rides behind the dialogue text; the before/after screenshots show it visibly dimming the ceiling lights behind the words. Rick and Hudson have the same illegibility risk if you want the fix propagated.
[ ] 4. Clock — found it (SM_Wall_Decor_Set_NN_02c, store south wall) and blurred its face at the texture level: numerals and hands gone, still obviously a clock.
[ ] Item 5 (first-play fog wall/bladder) — your shader-compilation hunch is almost certainly right: cold-session PSO compilation with the proxy-delay setting explains the invisible-then-fine pattern, and I found + fixed a concrete case (the bladder vignette only compiled its pipeline at the first pulse — it now warms invisibly at load). The exact symptom didn't reproduce in the harness (my DDC is warm), so your one verification: the editor I just launched is fresh — hit play once and watch the first bladder pulse and the fog wall. Also, nothing in the level is named anything fog-wall-like; my best guess is the oasis waterfall steam. If it's something else, tell me which actor and I'll give it the same warm-at-load treatment.
