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
        ("_ThumbnailKey",  unreal.Rotator(roll=0, pitch=-25, yaw=200), 20.0),
        ("_ThumbnailFill", unreal.Rotator(roll=0, pitch=-15, yaw=160), 8.0),
        ("_ThumbnailRim",  unreal.Rotator(roll=0, pitch=20,  yaw=180), 6.0),
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


def force_high_mips(material_interface):
    """Force every texture parameter on the material to its highest mip level
    so the SceneCapture doesn't sample a blurry low-res mip. Also clears any
    LOD bias on the texture asset itself."""
    if not material_interface:
        return
    mel = unreal.MaterialEditingLibrary
    if not isinstance(material_interface, unreal.MaterialInstance):
        return
    for tp_name in mel.get_texture_parameter_names(material_interface):
        tex = mel.get_material_instance_texture_parameter_value(material_interface, tp_name)
        if isinstance(tex, unreal.Texture2D):
            # 9999s = effectively forever for this commandlet run.
            tex.set_force_mip_levels_to_be_resident(9999.0)
            try:
                tex.set_editor_property("never_stream", True)
                tex.set_editor_property("lod_bias", -3)
            except Exception:
                pass


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


def spawn_capture(actor_sub, render_target, pad=1.4):
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
    return capture


def render_one(item, actor_sub, world, render_target, out_dir):
    item_id = item["id"]
    mesh_asset = unreal.EditorAssetLibrary.load_asset(item["mesh"])
    if not mesh_asset or not isinstance(mesh_asset, unreal.StaticMesh):
        unreal.log_error(f"[thumbnails] skipping {item_id}: bad mesh asset")
        return 0

    scale = auto_scale_for_mesh(mesh_asset)
    base_pitch, base_yaw, base_roll = item.get("rotation", (0.0, 0.0, 0.0))

    written = 0
    for p_off, y_off, r_off in itertools.product(ROTATION_ANGLES, repeat=3):
        rotation = (base_pitch + p_off, base_yaw + y_off, base_roll + r_off)

        item_actor = spawn_item_actor(
            actor_sub, mesh_asset, item.get("materials"), rotation, scale
        )
        capture_actor = spawn_capture(actor_sub, render_target)

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

    written = 0
    try:
        for item in ITEM_REGISTRY:
            written += render_one(item, actor_sub, world, render_target, out_dir)
    finally:
        for wall in chamber_walls:
            actor_sub.destroy_actor(wall)
        for light in chamber_lights:
            actor_sub.destroy_actor(light)

    expected = len(ITEM_REGISTRY) * (len(ROTATION_ANGLES) ** 3)
    unreal.log(f"[thumbnails] Done: {written}/{expected} PNGs in {out_dir}")


if __name__ == "__main__":
    main()
