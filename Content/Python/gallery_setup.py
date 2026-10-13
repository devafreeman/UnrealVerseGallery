"""One-time setup for The Verse Gallery.

Runs automatically the first time the project is opened (see init_unreal.py), or manually via
Tools > Execute Python Script... > Content/Python/gallery_setup.py

It imports GalleryData/ (textures, audio, sky) into /Game/Gallery, creates the gallery materials,
and builds + saves the level /Game/Maps/VerseGallery with a GalleryBuilder actor in it.
Safe to run again: existing assets are kept and the level is rebuilt.
"""

import json
import os

import unreal

GALLERY = "/Game/Gallery"
MAP_PATH = "/Game/Maps/VerseGallery"
DATA_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "GalleryData")
IMPORT_EXTENSIONS = (".png", ".jpg", ".jpeg", ".tga", ".wav", ".exr", ".hdr")
WHITE_TEXTURE = "/Engine/EngineResources/WhiteSquareTexture"

asset_lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

SKY_UV_CODE = """float3 d = normalize(Dir);
float c = cos(Rot);
float s = sin(Rot);
float2 xy = float2(d.x * c - d.y * s, d.x * s + d.y * c);
float u = 0.5 - atan2(xy.x, xy.y) / 6.28318530718;
float v = acos(clamp(d.z, -1.0, 1.0)) / 3.14159265359;
return float2(u, v);"""


def log(msg):
    unreal.log("[VerseGallery] " + msg)


# ------------------------------------------------------------------ import

def import_folder(folder, destination):
    source = os.path.join(DATA_DIR, folder)
    if not os.path.isdir(source):
        return
    tasks = []
    for file_name in sorted(os.listdir(source)):
        name, ext = os.path.splitext(file_name)
        if ext.lower() not in IMPORT_EXTENSIONS:
            continue
        if asset_lib.does_asset_exist(destination + "/" + name):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source, file_name))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    if tasks:
        log("Importing %d file(s) from GalleryData/%s" % (len(tasks), folder))
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)


def fix_texture_settings():
    """The importer guesses texture types from the pixels: the blue Skyline screenshots come in as
    normal maps (no sRGB, blue channel dropped) and others as UI icons (no mipmaps). Force plain color."""
    for path in asset_lib.list_assets(GALLERY + "/Textures", recursive=False):
        texture = unreal.load_asset(path)
        if not isinstance(texture, unreal.Texture2D):
            continue
        wanted = {
            "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
            "srgb": True,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_WORLD,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
        }
        changed = False
        for prop, value in wanted.items():
            if texture.get_editor_property(prop) != value:
                texture.set_editor_property(prop, value)
                changed = True
        if changed:
            log("Fixed texture settings on %s" % texture.get_name())
            asset_lib.save_loaded_asset(texture)


def configure_imports(data):
    fix_texture_settings()

    music = data.get("music")
    if music:
        wave = unreal.load_asset("%s/Audio/%s" % (GALLERY, music["name"]))
        if wave:
            wave.set_editor_property("looping", True)
            asset_lib.save_loaded_asset(wave)

    sky_texture = load_sky_texture(data)
    if sky_texture:
        sky_texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        sky_texture.set_editor_property("never_stream", True)
        asset_lib.save_loaded_asset(sky_texture)


def load_sky_texture(data):
    sky = data.get("sky") or {}
    file_name = sky.get("file", "")
    if not file_name:
        return None
    name = os.path.splitext(os.path.basename(file_name))[0]
    return unreal.load_asset("%s/Sky/%s" % (GALLERY, name))


# ------------------------------------------------------------------ materials

def new_material(name):
    path = "%s/Materials/%s" % (GALLERY, name)
    if asset_lib.does_asset_exist(path):
        return None
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(name, GALLERY + "/Materials", unreal.Material, unreal.MaterialFactoryNew())


def expr(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def texture_param(material, name, x, y, texture=None):
    node = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture or unreal.load_asset(WHITE_TEXTURE))
    return node


def vector_param(material, name, x, y, value):
    node = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def scalar_param(material, name, x, y, value):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def multiply(material, a, a_out, b, b_out, x, y):
    node = expr(material, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, a_out, node, "A")
    mel.connect_material_expressions(b, b_out, node, "B")
    return node


def finish(material):
    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)


def create_surface_material(name, unlit=False, translucent=False):
    material = new_material(name)
    if material is None:
        return
    material.set_editor_property("two_sided", True)
    if unlit:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    if translucent:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    base_map = texture_param(material, "BaseMap", -900, 0)
    base_color = vector_param(material, "BaseColor", -900, 260, unreal.LinearColor(1, 1, 1, 1))
    albedo = multiply(material, base_map, "RGB", base_color, "", -500, 100)

    if unlit:
        mel.connect_material_property(albedo, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
        metallic = scalar_param(material, "Metallic", -500, 300, 0.0)
        roughness = scalar_param(material, "Roughness", -500, 400, 0.5)
        mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        emissive = vector_param(material, "Emissive", -900, 520, unreal.LinearColor(0, 0, 0, 1))
        emissive_map = texture_param(material, "EmissiveMap", -900, 760)
        glow = multiply(material, emissive, "", emissive_map, "RGB", -500, 600)
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if translucent:
        opacity = multiply(material, base_map, "A", base_color, "A", -500, 800)
        mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    finish(material)


def create_sky_material(sky_texture):
    if sky_texture is None:
        unreal.log_warning("[VerseGallery] Sky texture missing; the gallery will use an Unreal sky atmosphere instead.")
        return
    material = new_material("M_GallerySky")
    if material is None:
        return
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("is_sky", True)

    camera = expr(material, unreal.MaterialExpressionCameraVectorWS, -1400, 0)
    view_dir = expr(material, unreal.MaterialExpressionMultiply, -1200, 0)
    view_dir.set_editor_property("const_b", -1.0)
    mel.connect_material_expressions(camera, "", view_dir, "A")

    rotation = scalar_param(material, "Rotation", -1200, 200, 0.0)

    dir_input = unreal.CustomInput()
    dir_input.set_editor_property("input_name", "Dir")
    rot_input = unreal.CustomInput()
    rot_input.set_editor_property("input_name", "Rot")

    custom = expr(material, unreal.MaterialExpressionCustom, -950, 0)
    custom.set_editor_property("code", SKY_UV_CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property("description", "EquirectUV")
    custom.set_editor_property("inputs", [dir_input, rot_input])
    mel.connect_material_expressions(view_dir, "", custom, "Dir")
    mel.connect_material_expressions(rotation, "", custom, "Rot")

    sky = texture_param(material, "SkyTexture", -650, 0, sky_texture)
    sky.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    sky.set_editor_property("mip_value_mode", unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
    sky.set_editor_property("const_mip_value", 0)
    mel.connect_material_expressions(custom, "", sky, "UVs")

    tint = vector_param(material, "Tint", -650, 300, unreal.LinearColor(1, 1, 1, 1))
    color = multiply(material, sky, "RGB", tint, "", -300, 100)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    finish(material)


# ------------------------------------------------------------------ level

def build_level():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if asset_lib.does_asset_exist(MAP_PATH):
        level_editor.load_level(MAP_PATH)
    else:
        level_editor.new_level(MAP_PATH)

    builder_class = unreal.load_class(None, "/Script/VerseGallery.GalleryBuilder")
    if builder_class is None:
        unreal.log_error("[VerseGallery] GalleryBuilder class not found. Make sure the C++ code compiled.")
        return

    builders = [a for a in actors.get_all_level_actors() if a.get_class() == builder_class]
    if builders:
        builders[0].rebuild()
    else:
        actors.spawn_actor_from_class(builder_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

    level_editor.save_current_level()
    log("Level saved to " + MAP_PATH)


# ------------------------------------------------------------------ entry points

def load_data():
    data_file = os.path.join(DATA_DIR, "gallery.json")
    if not os.path.isfile(data_file):
        unreal.log_error("[VerseGallery] Missing " + data_file)
        return None
    with open(data_file, "r", encoding="utf-8") as f:
        return json.load(f)


def import_data(data):
    import_folder("Textures", GALLERY + "/Textures")
    import_folder("Audio", GALLERY + "/Audio")
    import_folder("Sky", GALLERY + "/Sky")
    configure_imports(data)


def run():
    data = load_data()
    if data is None:
        return

    with unreal.ScopedSlowTask(3, "Setting up The Verse Gallery...") as task:
        task.make_dialog(True)

        task.enter_progress_frame(1, "Importing textures, audio and sky")
        import_data(data)

        task.enter_progress_frame(1, "Creating materials")
        create_surface_material("M_GalleryLit")
        create_surface_material("M_GalleryUnlit", unlit=True)
        create_surface_material("M_GalleryTranslucent", translucent=True)
        create_sky_material(load_sky_texture(data))

        task.enter_progress_frame(1, "Building the gallery level")
        build_level()

    log("Setup complete. Press Play to walk the gallery.")


def frame_viewport_on_gallery(data):
    """Puts the editor camera at the intro tour's opening shot (the doorway) instead of the
    last saved view, which is often far outside the room."""
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    builders = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GalleryBuilder) if world else []
    if not builders:
        return

    shots = (data.get("introTour") or {}).get("shots") or []
    if shots:
        pos, pitch, yaw = shots[0]["pos"], shots[0].get("pitch", 0.0), shots[0].get("yaw", 0.0)
    else:
        player = data.get("player") or {}
        pos = list(player.get("pos", [0.0, 0.0, 0.0]))
        pos[2] += 170.0
        pitch, yaw = 0.0, player.get("yaw", 0.0)

    xf = builders[0].get_actor_transform()
    location = unreal.MathLibrary.transform_location(xf, unreal.Vector(*pos))
    rotation = unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw + builders[0].get_actor_rotation().yaw)
    editor.set_level_viewport_camera_info(location, rotation)


_state = {"handle": None, "elapsed": 0.0}


def _first_launch_tick(delta_seconds):
    _state["elapsed"] += delta_seconds
    if _state["elapsed"] < 3.0:
        return
    if unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets():
        return

    unreal.unregister_slate_post_tick_callback(_state["handle"])
    _state["handle"] = None

    try:
        if asset_lib.does_asset_exist(MAP_PATH):
            # Already set up: only pick up files added to GalleryData since (e.g. a new voice-over).
            data = load_data()
            if data is not None:
                import_data(data)
                frame_viewport_on_gallery(data)
            return
        run()
        data = load_data()
        if data is not None:
            frame_viewport_on_gallery(data)
    except Exception as exc:  # keep the editor usable if anything goes wrong
        unreal.log_error("[VerseGallery] Setup failed: %s" % exc)


def run_on_first_launch():
    """Called from init_unreal.py: waits until the editor is ready, then runs setup once.
    On later launches it only imports files that were added to GalleryData."""
    if _state["handle"] is None:
        _state["handle"] = unreal.register_slate_post_tick_callback(_first_launch_tick)


if __name__ == "__main__":
    run()
