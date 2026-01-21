"""
Batch-optimize VHS cover textures to reduce memory footprint.

Run from UE Editor (Output Log):
  py "Content/Python/optimize_vhs_textures.py"

Settings applied:
- Enable streaming (NeverStream = False)
- Optional virtual texture streaming toggle
- MaxTextureSize capped to 1024
- Keep default compression and sRGB
"""

import unreal


# Tunables
MAX_TEXTURE_SIZE = 1024
ENABLE_VIRTUAL_TEXTURE_STREAMING = False  # Disabled to avoid VT warnings on non–power-of-two sources
TEXTURES_PATH = "/Game/VHSCovers"


def _has_attr(obj, name):
    try:
        getattr(obj, name)
        return True
    except AttributeError:
        return False


def apply_settings(texture: unreal.Texture2D) -> bool:
    """Apply memory-friendly settings; return True if modified."""
    changed = False

    if texture.never_stream:
        texture.set_editor_property("never_stream", False)
        changed = True

    # Leave VT streaming off by default to avoid warnings on non–power-of-two textures.
    if bool(texture.virtual_texture_streaming) != ENABLE_VIRTUAL_TEXTURE_STREAMING:
        texture.set_editor_property("virtual_texture_streaming", ENABLE_VIRTUAL_TEXTURE_STREAMING)
        changed = True

    if texture.max_texture_size != MAX_TEXTURE_SIZE:
        texture.set_editor_property("max_texture_size", MAX_TEXTURE_SIZE)
        changed = True

    if texture.lod_bias != 0:
        texture.set_editor_property("lod_bias", 0)
        changed = True

    if texture.compression_settings != unreal.TextureCompressionSettings.TC_DEFAULT:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        changed = True

    if not texture.srgb:
        texture.set_editor_property("srgb", True)
        changed = True

    # If mip gen is set to no mips, nudge to FromTextureGroup (only if enum exists)
    has_nomips = _has_attr(unreal.TextureMipGenSettings, "TMGS_NoMipmaps")
    has_from_group = _has_attr(unreal.TextureMipGenSettings, "TMGS_FromTextureGroup")
    if has_nomips and has_from_group:
        if texture.mip_gen_settings == unreal.TextureMipGenSettings.TMGS_NoMipmaps:
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FromTextureGroup)
            changed = True

    if changed:
        texture.modify()

    return changed


def optimize_all():
    asset_paths = unreal.EditorAssetLibrary.list_assets(TEXTURES_PATH, recursive=False)
    total = 0
    updated = 0

    for asset_path in asset_paths:
        if "/Materials/" in asset_path:
            continue

        package_path = asset_path.split(".")[0] if "." in asset_path else asset_path
        asset_data = unreal.EditorAssetLibrary.find_asset_data(package_path)
        if not asset_data:
            continue

        asset_class_path = asset_data.asset_class_path
        asset_class = str(asset_class_path.asset_name) if asset_class_path else ""
        if "Texture2D" not in asset_class:
            continue

        texture = unreal.load_asset(package_path)
        if not texture:
            continue

        total += 1
        if apply_settings(texture):
            unreal.EditorAssetLibrary.save_asset(texture.get_path_name(), only_if_is_dirty=True)
            updated += 1

    unreal.log(f"Optimized {updated} of {total} textures (max size {MAX_TEXTURE_SIZE}, VT streaming {ENABLE_VIRTUAL_TEXTURE_STREAMING})")


if __name__ == "__main__":
    optimize_all()
