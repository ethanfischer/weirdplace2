------------------------------------------
# Required for MVP
------------------------------------------

# Claude Friendly
[ ] looking at one of the gastation lights for 30 seconds adds something to your inventory
    [x] add component to whatever actor I choose. And editor supplied reward
        [ ] camera fov and black vignette
    [x] for example, watching the movie playing on the tv rewards you too (deferred 6/9 — Ethan's call: light-only for now)
[x] "Rick: I'll meet you inside" It shouldn't say Rick: in the dialogue shown to the user but it does for some reason.
[x] improve movie pickup
    [x] tell player what button to put back
    [x] have text always face player
[x] make blank vhs held pose same as other movies
[x] playing videos on the tv
    
# Needs human
[ ] more intricate soundscape espcially in oasis
    [ ] have random noises so the world feels more alive
[ ] gas station door animation
[ ] drop key
    [ ] need better sfx
    [ ] broken key mesh
[ ] meet seneca outside smoking cig
    [ ] Seneca smoking animation — animate directly in UE5 via Control Rig + Sequencer on MetaHuman body, export as Animation Sequence, drive via `bIsSmoking` bool in `ABP_Seneca`.
        Cigarette prop already attached to finger bone in `BP_Seneca`.
        
# Needs design
[ ] Can't see items collected in dark. Illuminate them somehow
[ ] handle homeless scenario if you give money to Hudson. how to progress? 
    [ ] strange washer found later, but seneca accepts it as if it covers the 2.95 after given money to hudson
    
-------------------------------------------
# Post MVP
-------------------------------------------

# Claude Friendly

# Needs human
[ ] seneca unlock employee bathroom animation

# Needs design
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
    
