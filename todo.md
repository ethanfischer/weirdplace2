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
[x] add lock mesh to employee door
[ ] claude added the movie poster to phone pole but it looks like shit
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
[ ] color scheme/tonemapping
[ ] put another keypad locked door on the outside of the building
[ ] make gas station pumps interactable
[ ] make employee hallway have collectibles
[ ] add more collectibles in general
[ ] add lowpassfilter (lpf) to door lock sound
[ ] seneca unlock employee bathroom animation
[ ] more intricate soundscape espcially in oasis
    [ ] have random noises so the world feels more alive
[ ] door close sounds

# Needs design
[ ] explore camera roll / dutch-angle as an aesthetic (a debugging accident tilted the horizon ~4° and it looked cool — maybe for dread moments, the other-world, or the drive)
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
    
-------------------------------------    
# Fable Audit
---------------------------------
#	What	Why	Size
[x] 1	De-tick the MovieBox fleet	~3,200 no-op tick dispatches/frame, est. 1.5–3 ms	S
[x] 2	Fix the key-break wedge guard	Can permanently dead-end the story	S
[x] 3	Fix TeleportTriggerBox “UltraDynamic” needle	Feature destroys the wrong actor	S
[ ] 4	Delete the dead InventoryRoomComponent cluster	900-line agent trap w/ hitch grenade inside	S
[ ] 5	Fix Rick’s missing OnDialogueEnded dispatch	Real bug born from NPC copy-paste	S
[ ] 6	Decide the combined-tape beat: cut or rewire	~120 lines of unreachable story code	S–M
[ ] 7	E2E loop economics (§5): step-delay, timeout, fail-fast	Directly buys back overnight agent hours	S–M
[ ] 8	Decide VR for real (§6)	Everything downstream depends on it	strategic
[ ] 9	Fix the “this PC dies” exposure (§7)	8.9 GB of level content exists only on this machine	S
[ ] 10	Merge AMyCharacter → AFirstPersonCharacter	Two names for one object; already confused the agent’s own memory once	M
