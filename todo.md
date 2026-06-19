------------------------------------------
# Required for MVP
------------------------------------------

# Claude Friendly
[ ] TV shows tornado warning the next time you enter the store after bathroom key breaks  (storm beat code + E2E done — needs editor wiring, see NIGHTLY_REPORT addendum 2026-06-18)
    [x] original tv sounds need to cease (Ambient_TV and Ambient_TV2)
    [x] tornado warning needs to play the creepy tornado warning sound and show a new texture on the tv screen that says tornado warning  (siren loops from TVs; ScreenTex slot added — assign your "TORNADO WARNING" texture on BP_TV)
    [x] gas station spotlights, gastationbarlights, and outsidegastationlights need to have their intensity reduced by a number I can set in editor at this story beat  (AStormBeatController: drag light actors into LightsToDim + set DimMultiplier; emissive-mesh "lights" w/o a light component can't be dimmed this way)
     
[x] BP_TelephoneScene should only appear after seeing tornado warning on tv
[x] add "missing person" poster that looks like seneca to telephone pole
[ ] seneca tells you about tornado shelter in bathroom stall later
[ ] hear stuff through static on pay phone
[ ] door lock sound shouldn't play while the user is inserting the key. Also the key is really hard to see in this game we gotta do something about that

    
# Needs human
[x] replace gas pumps grey blocks
[ ] add lowpassfilter (lpf) to door lock sound
[ ] clock that ticks with only one hand in both directions
    [ ] or at least needs to be blurred. cant see clocks in dreams
[ ] rick gas pump animation (or at least improve pose when he looks at you)
[ ] gas station door animation
[x] drop key
    [x] need better sfx
    [x] show broken key collection spinning like any other collected item
    [x] broken key mesh
    
# Needs design
[ ] Can't see items collected in dark. Illuminate them somehow
    [x] add light
    [ ] further test light with other items
[ ] handle homeless scenario if you give money to Hudson. how to progress? 
    [ ] strange washer found later, but seneca accepts it as if it covers the 2.95 after given money to hudson
    
-------------------------------------------
# Post MVP
-------------------------------------------

# Claude Friendly

# Needs human
[ ] add blur in item inspection state
[ ] seneca unlock employee bathroom animation
[ ] more intricate soundscape espcially in oasis
    [ ] have random noises so the world feels more alive

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
    
