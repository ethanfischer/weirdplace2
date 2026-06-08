"""
Render a thumbnail PNG for every inventory item × rotation combination using
the SAME setup the game uses for ShowItemNotification:

  * Loaded inside the actual game map (FirstPersonMap) so the project's
    reflection captures / post-process volumes / sky atmosphere apply.
  * Auto-scaled so the mesh's longest axis = 8cm, matching the in-game
    notification mesh sizing in AFirstPersonCharacter::ShowItemNotification.
  * Rendered through SceneCapture2D positioned right in front of the spawn
    so the same lighting hits the mesh.

Run with editor open:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/generate_item_thumbnails.py"
Or headless:
    UnrealEditor-Cmd.exe weirdplace2.uproject -ExecutePythonScript=<path> -unattended -nopause -nosplash

Output:
    <ProjectDir>/Saved/Screenshots/<ItemID>_p<P>_y<Y>_r<R>.png
"""

import itertools
import os
import unreal


# =====================================================================
# Registry — see comments at top for usage.
# =====================================================================
ITEM_REGISTRY = [
    {
        "id": "Money",
        "mesh": "/Game/Import/cash/cash",
        # Pitch=-90 rotates the cash so its bill face presents to the camera.
        "rotation": (-90.0, 0.0, 0.0),
    },
    {
        "id": "Key",
        "mesh": "/Game/Fab/Small_Key__1MB_/small_key_1mb",
        "materials": ["/Game/Fab/Small_Key__1MB_/MetalKey"],
        "rotation": (90.0, 0.0, 0.0),
    },
    {
        "id": "BrokenKey",
        "mesh": "/Game/Fab/Small_Key__1MB_/brokenkey",
        "materials": ["/Game/Fab/Small_Key__1MB_/MetalKey"],
        "rotation": (90.0, 0.0, 0.0),
    },
    {
        "id": "BlankVHS",
        # Use the gameplay Blueprint so the black cassette inside the sleeve
        # renders with the box. The BP's "Cube" SMC has a relative rotation of
        # (roll=90, yaw=-90), so the static-mesh's local-front sleeve ends up
        # facing the camera at actor rotation identity (0, 0, 0).
        "blueprint": "/Game/Blueprints/BP_BlankVHS.BP_BlankVHS",
        "rotation": (0.0, 0.0, 0.0),
    },
]

THUMB_W, THUMB_H = 1024, 1440
# Far from any FirstPerson map geometry — guaranteed empty space.
BOOTH_LOC = unreal.Vector(9999.0, 9999.0, 9999.0)
CAMERA_FOV = 35.0
OUTPUT_SUBDIR = "Screenshots"
ROTATION_ANGLES = [0, 45, 90]
# Same map the game runs in. Brings all the project lighting / post-process /
# reflection-capture setup along for free, matching ShowItemNotification.
TEMPLATE_MAP = "/Game/FirstPerson/Maps/FirstPersonMap"
# Match AFirstPersonCharacter::ShowItemNotification — normalize so longest
# axis half-extent = 4cm (full extent = 8cm).
TARGET_HALF_EXTENT_CM = 4.0


def get_subsystems():
    return (
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem),
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem),
    )


def open_template_map():
    if unreal.EditorAssetLibrary.does_asset_exist(TEMPLATE_MAP):
        unreal.EditorLoadingAndSavingUtils.load_map(TEMPLATE_MAP)
    else:
        unreal.log_error(f"[thumbnails] {TEMPLATE_MAP} missing")


def auto_scale_for_mesh(mesh_asset):
    """Mirror AFirstPersonCharacter::ShowItemNotification's normalization:
    longest axis half-extent → TARGET_HALF_EXTENT_CM."""
    extent = mesh_asset.get_bounds().box_extent
    max_e = max(extent.x, extent.y, extent.z, 1e-3)
    return TARGET_HALF_EXTENT_CM / max_e


def build_black_chamber(actor_sub):
    """Spawn a black-walled cube around BOOTH_LOC so camera sees clean black
    background (and we don't render the SkyAtmosphere warning text). 4 m on a
    side — wide enough that lights aren't crammed against the walls."""
    HALF = 200.0  # cm
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane.Plane")
    base_mat = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_SolidColor.M_SolidColor")

    # Each tuple: (offset, rotation) where rotation orients the plane's +Z
    # normal to point toward BOOTH_LOC (i.e. inward).
    wall_specs = [
        (unreal.Vector(0, 0, -HALF),  unreal.Rotator(0, 0, 0)),       # floor
        (unreal.Vector(0, 0,  HALF),  unreal.Rotator(180, 0, 0)),     # ceiling
        (unreal.Vector( HALF, 0, 0),  unreal.Rotator(90, 0, 0)),      # wall +X (faces -X)
        (unreal.Vector(-HALF, 0, 0),  unreal.Rotator(-90, 0, 0)),     # wall -X
        (unreal.Vector(0,  HALF, 0),  unreal.Rotator(0, 0, 90)),      # wall +Y
        (unreal.Vector(0, -HALF, 0),  unreal.Rotator(0, 0, -90)),     # wall -Y
    ]
    walls = []
    for offset, rot in wall_specs:
        wall = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, BOOTH_LOC + offset, rot)
        wall.set_actor_label("_ThumbnailWall")
        # Plane is 100x100 cm at scale 1 — scale to 2*HALF so each wall fully covers.
        wall.set_actor_scale3d(unreal.Vector(HALF * 0.02, HALF * 0.02, 1.0))
        sm = wall.static_mesh_component
        sm.set_static_mesh(plane)
        # Walls must not cast shadows — they sit between the directional lights
        # and the item and would shadow it.
        sm.set_editor_property("cast_shadow", False)
        if base_mat:
            sm.set_material(0, base_mat)
            mid = sm.create_dynamic_material_instance(0, base_mat)
            if mid:
                black = unreal.LinearColor(0.0, 0.0, 0.0, 1.0)
                mid.set_vector_parameter_value("Color", black)
                mid.set_vector_parameter_value("BaseColor", black)
                mid.set_vector_parameter_value("EmissiveColor", black)
        walls.append(wall)
    return walls


def spawn_chamber_lights(actor_sub):
    """Three forward-facing directional lights illuminating items inside the
    chamber (camera is on +X side looking -X, so lights yaw toward -X)."""
    specs = [
        ("_ThumbnailKey",  unreal.Rotator(roll=0, pitch=-25, yaw=200), 12.0),
        ("_ThumbnailFill", unreal.Rotator(roll=0, pitch=-15, yaw=160), 5.0),
        ("_ThumbnailRim",  unreal.Rotator(roll=0, pitch=20,  yaw=180), 4.0),
    ]
    lights = []
    for label, rot, intensity in specs:
        light = actor_sub.spawn_actor_from_class(
            unreal.DirectionalLight,
            BOOTH_LOC + unreal.Vector(0.0, 0.0, 100.0),
            rot,
        )
        light.set_actor_label(label)
        light.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        light.light_component.set_editor_property("intensity", intensity)
        lights.append(light)
    return lights


def _pin_texture(tex):
    if not isinstance(tex, unreal.Texture2D):
        return
    # Only force-resident; do NOT mutate asset-level properties (never_stream
    # / lod_bias). On 2048×2048 sRGB sources those mutations have triggered
    # surprising side-effects (e.g. the sleeve texture bleeding into the
    # scene as a pink backdrop) that aren't worth debugging.
    tex.set_force_mip_levels_to_be_resident(9999.0)


# Material property channels we walk to find TextureSample nodes on plain
# unreal.Material assets (which don't expose .Expressions to Python).
_MATERIAL_PROPS = [
    unreal.MaterialProperty.MP_BASE_COLOR,
    unreal.MaterialProperty.MP_METALLIC,
    unreal.MaterialProperty.MP_ROUGHNESS,
    unreal.MaterialProperty.MP_NORMAL,
    unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    unreal.MaterialProperty.MP_SPECULAR,
    unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
]


def force_high_mips(material_interface):
    """Pin every Texture2D referenced by the material at full mips so the
    SceneCapture doesn't sample a blurry / unstreamed placeholder. Handles
    both MaterialInstance (texture parameters) and Material (TextureSample
    nodes walked via MaterialEditingLibrary)."""
    if not material_interface:
        return
    mel = unreal.MaterialEditingLibrary
    if isinstance(material_interface, unreal.MaterialInstance):
        for tp_name in mel.get_texture_parameter_names(material_interface):
            _pin_texture(mel.get_material_instance_texture_parameter_value(material_interface, tp_name))
        return
    if isinstance(material_interface, unreal.Material):
        for prop in _MATERIAL_PROPS:
            node = mel.get_material_property_input_node(material_interface, prop)
            if node is not None and "TextureSample" in node.__class__.__name__:
                _pin_texture(node.get_editor_property("texture"))


def _combined_smc_bounds(actor):
    """Union the world-space AABBs of every visible StaticMeshComponent.
    Each SMC's local-space (min, max) box is transformed via its world
    transform; we walk all 8 corners to get an axis-aligned world box.
    Returns (center, half_extent) as unreal.Vector."""
    smcs = actor.get_components_by_class(unreal.StaticMeshComponent)
    bmin = [float("inf")] * 3
    bmax = [float("-inf")] * 3
    for smc in smcs:
        if not smc.is_visible():
            continue
        lo, hi = smc.get_local_bounds()  # (min, max) in local space — yes, misnamed API
        xform = smc.get_world_transform()
        for x in (lo.x, hi.x):
            for y in (lo.y, hi.y):
                for z in (lo.z, hi.z):
                    p = unreal.MathLibrary.transform_location(xform, unreal.Vector(x, y, z))
                    bmin[0] = min(bmin[0], p.x); bmax[0] = max(bmax[0], p.x)
                    bmin[1] = min(bmin[1], p.y); bmax[1] = max(bmax[1], p.y)
                    bmin[2] = min(bmin[2], p.z); bmax[2] = max(bmax[2], p.z)
    if bmin[0] == float("inf"):
        return unreal.Vector(0, 0, 0), unreal.Vector(0, 0, 0)
    center = unreal.Vector((bmin[0]+bmax[0])*0.5, (bmin[1]+bmax[1])*0.5, (bmin[2]+bmax[2])*0.5)
    extent = unreal.Vector((bmax[0]-bmin[0])*0.5, (bmax[1]-bmin[1])*0.5, (bmax[2]-bmin[2])*0.5)
    return center, extent


def _hide_non_mesh_visuals(comp):
    """Hide WidgetComponent/BillboardComponent/TextRenderComponent in a BP so
    they don't render UI/icons into the thumbnail."""
    for c in comp.get_children_components(False):
        if isinstance(c, (unreal.WidgetComponent, unreal.BillboardComponent, unreal.TextRenderComponent)):
            c.set_visibility(False, False)
        _hide_non_mesh_visuals(c)


def spawn_bp_actor(actor_sub, bp_path, rotation):
    """Spawn a Blueprint actor, hide UI components, pin its mesh materials'
    textures, auto-scale to fit, and re-center on BOOTH_LOC. Returns the actor."""
    pitch, yaw, roll = rotation
    rot = unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)
    bp_class = unreal.load_object(None, f"{bp_path}_C")
    if not bp_class:
        unreal.log_error(f"[thumbnails] couldn't load BP class {bp_path}_C")
        return None
    actor = actor_sub.spawn_actor_from_class(bp_class, BOOTH_LOC, rot)
    actor.set_actor_label("_ThumbnailItem")
    if actor.root_component:
        _hide_non_mesh_visuals(actor.root_component)

    for smc in actor.get_components_by_class(unreal.StaticMeshComponent):
        for slot in range(smc.get_num_materials()):
            force_high_mips(smc.get_material(slot))

    # Measure at scale 1, then scale + re-center so combined bounds center sits on BOOTH_LOC.
    center, extent = _combined_smc_bounds(actor)
    max_e = max(extent.x, extent.y, extent.z, 1e-3)
    scale = TARGET_HALF_EXTENT_CM / max_e
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    offset = (center - BOOTH_LOC) * scale
    actor.set_actor_location(BOOTH_LOC - offset, False, False)
    return actor


def spawn_item_actor(actor_sub, mesh_asset, material_paths, rotation, scale):
    pitch, yaw, roll = rotation
    rot = unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, BOOTH_LOC, rot)
    actor.set_actor_label("_ThumbnailItem")
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    sm_comp = actor.static_mesh_component
    sm_comp.set_static_mesh(mesh_asset)
    # Force the mesh's authored materials to high mips too.
    for slot in range(sm_comp.get_num_materials()):
        force_high_mips(sm_comp.get_material(slot))
    for slot, path in enumerate(material_paths or []):
        mat = unreal.EditorAssetLibrary.load_asset(path)
        if mat:
            sm_comp.set_material(slot, mat)
            force_high_mips(mat)

    # Re-center: rotate+scale local bounds origin into world space.
    mb = mesh_asset.get_bounds()
    xform = unreal.Transform(location=unreal.Vector(0, 0, 0), rotation=rot,
                             scale=unreal.Vector(scale, scale, scale))
    world_origin = unreal.MathLibrary.transform_location(xform, mb.origin)
    actor.set_actor_location(BOOTH_LOC - world_origin, False, False)
    return actor


def spawn_capture(actor_sub, render_target, pad=1.4, exposure_bias=-1.0):
    # After auto-scale all items have ~TARGET_HALF_EXTENT_CM half-extent. So
    # camera distance is constant per render.
    half_fov_rad = unreal.MathLibrary.degrees_to_radians(CAMERA_FOV * 0.5)
    distance = (TARGET_HALF_EXTENT_CM * pad) / unreal.MathLibrary.tan(half_fov_rad)

    cam_loc = BOOTH_LOC + unreal.Vector(distance, 0.0, 0.0)
    cam_rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)

    capture = actor_sub.spawn_actor_from_class(unreal.SceneCapture2D, cam_loc, cam_rot)
    capture.set_actor_label("_ThumbnailCapture")
    cap = capture.capture_component2d
    cap.texture_target = render_target
    cap.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    cap.fov_angle = CAMERA_FOV
    # Lock exposure so dimmer lights actually render dimmer (otherwise auto-
    # exposure normalizes everything to mid-gray). Clamping min==max pins
    # auto-exposure at a fixed value; bias is in EV stops (+1 = 2x brighter).
    pp = cap.post_process_settings
    pp.set_editor_property("override_auto_exposure_min_brightness", True)
    pp.set_editor_property("auto_exposure_min_brightness", 1.0)
    pp.set_editor_property("override_auto_exposure_max_brightness", True)
    pp.set_editor_property("auto_exposure_max_brightness", 1.0)
    pp.set_editor_property("override_auto_exposure_bias", True)
    pp.set_editor_property("auto_exposure_bias", exposure_bias)
    cap.post_process_settings = pp
    return capture


def render_one(item, actor_sub, world, render_target, out_dir, chamber_lights):
    item_id = item["id"]
    bp_path = item.get("blueprint")
    mesh_asset = None
    if not bp_path:
        mesh_asset = unreal.EditorAssetLibrary.load_asset(item["mesh"])
        if not mesh_asset or not isinstance(mesh_asset, unreal.StaticMesh):
            unreal.log_error(f"[thumbnails] skipping {item_id}: bad mesh asset")
            return 0

    scale = auto_scale_for_mesh(mesh_asset) if mesh_asset else None
    base_pitch, base_yaw, base_roll = item.get("rotation", (0.0, 0.0, 0.0))
    exposure_bias = item.get("exposure_bias", -1.0)
    light_mult = item.get("light_intensity_mult", 1.0)

    # Scale chamber lights for this item; restore after.
    base_intensities = [
        light.light_component.get_editor_property("intensity") for light in chamber_lights
    ]
    if light_mult != 1.0:
        for light, base in zip(chamber_lights, base_intensities):
            light.light_component.set_editor_property("intensity", base * light_mult)

    written = 0
    try:
        for p_off, y_off, r_off in itertools.product(ROTATION_ANGLES, repeat=3):
            rotation = (base_pitch + p_off, base_yaw + y_off, base_roll + r_off)

            if bp_path:
                item_actor = spawn_bp_actor(actor_sub, bp_path, rotation)
                if not item_actor:
                    continue
            else:
                item_actor = spawn_item_actor(
                    actor_sub, mesh_asset, item.get("materials"), rotation, scale
                )
            capture_actor = spawn_capture(actor_sub, render_target, exposure_bias=exposure_bias)

            try:
                unreal.RenderingLibrary.clear_render_target2d(world, render_target)
                capture_actor.capture_component2d.capture_scene()
                filename = f"{item_id}_p{p_off}_y{y_off}_r{r_off}.png"
                unreal.RenderingLibrary.export_render_target(world, render_target, out_dir, filename)
                unreal.log(f"[thumbnails] Wrote {filename}")
                written += 1
            finally:
                actor_sub.destroy_actor(capture_actor)
                actor_sub.destroy_actor(item_actor)
    finally:
        if light_mult != 1.0:
            for light, base in zip(chamber_lights, base_intensities):
                light.light_component.set_editor_property("intensity", base)

    return written


def main():
    open_template_map()
    actor_sub, ue_sub = get_subsystems()
    world = ue_sub.get_editor_world()
    if not world:
        unreal.log_error("[thumbnails] no editor world")
        return

    sl = unreal.SystemLibrary
    sl.execute_console_command(world, "r.AntiAliasingMethod 1")
    # Texture streaming knobs — force every mip of every used texture to load
    # and stay resident, so SceneCapture doesn't sample a 64×64 mip and bake
    # a blurry blob where the dollar bill print should be.
    sl.execute_console_command(world, "r.Streaming.FullyLoadUsedTextures 1")
    sl.execute_console_command(world, "r.Streaming.UnlimitedPoolSize 1")
    sl.execute_console_command(world, "r.Streaming.PoolSize 8192")
    sl.execute_console_command(world, "r.Streaming.MipBias -3")
    sl.execute_console_command(world, "r.MipMapLODBias -3")
    sl.execute_console_command(world, "r.MaxAnisotropy 16")
    sl.execute_console_command(world, "r.MaterialQualityLevel 3")

    out_dir = os.path.join(unreal.SystemLibrary.get_project_saved_directory(), OUTPUT_SUBDIR)
    os.makedirs(out_dir, exist_ok=True)

    render_target = unreal.new_object(unreal.TextureRenderTarget2D, world)
    render_target.set_editor_property("size_x", THUMB_W)
    render_target.set_editor_property("size_y", THUMB_H)
    render_target.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)

    # Hide just the SkyAtmosphere components in the level — they draw a
    # "your scene contains a skydome..." warning in our captures because our
    # chamber sits far outside the skydome mesh. Hiding the entire UDS actor
    # would also kill the SkyLight (IBL), so we recurse into components and
    # only disable atmosphere/skydome visuals.
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in eas.get_all_level_actors():
        for comp in actor.get_components_by_class(unreal.SkyAtmosphereComponent):
            comp.set_visibility(False, False)
        # UDS stores the skydome as a StaticMeshComponent named "SkySphere"
        # or "Sky_Sphere" — hide those by component name.
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            cname = comp.get_name().lower()
            if "sky" in cname:
                comp.set_visibility(False, False)

    chamber_walls = build_black_chamber(actor_sub)
    chamber_lights = spawn_chamber_lights(actor_sub)

    # Pre-pin every item's textures so the streaming system has time to load
    # high-detail mips before the first capture. Pinning inside the per-render
    # path leaves zero ticks for streaming and the first frames sample the
    # engine "missing texture" placeholder.
    for item in ITEM_REGISTRY:
        bp_path = item.get("blueprint")
        if bp_path:
            bp_class = unreal.load_object(None, f"{bp_path}_C")
            if not bp_class:
                continue
            # Spawn temporarily to enumerate SMCs + their materials, then destroy.
            tmp = actor_sub.spawn_actor_from_class(bp_class, BOOTH_LOC + unreal.Vector(0, 0, 1000), unreal.Rotator(0, 0, 0))
            for smc in tmp.get_components_by_class(unreal.StaticMeshComponent):
                for slot in range(smc.get_num_materials()):
                    force_high_mips(smc.get_material(slot))
            actor_sub.destroy_actor(tmp)
            continue
        mesh_asset = unreal.EditorAssetLibrary.load_asset(item["mesh"])
        if not mesh_asset or not isinstance(mesh_asset, unreal.StaticMesh):
            continue
        for slot in mesh_asset.static_materials:
            if slot.material_interface:
                force_high_mips(slot.material_interface)
        for path in item.get("materials") or []:
            mat = unreal.EditorAssetLibrary.load_asset(path)
            if mat:
                force_high_mips(mat)

    written = 0
    try:
        for item in ITEM_REGISTRY:
            written += render_one(item, actor_sub, world, render_target, out_dir, chamber_lights)
    finally:
        for wall in chamber_walls:
            actor_sub.destroy_actor(wall)
        for light in chamber_lights:
            actor_sub.destroy_actor(light)

    expected = len(ITEM_REGISTRY) * (len(ROTATION_ANGLES) ** 3)
    unreal.log(f"[thumbnails] Done: {written}/{expected} PNGs in {out_dir}")


if __name__ == "__main__":
    main()
