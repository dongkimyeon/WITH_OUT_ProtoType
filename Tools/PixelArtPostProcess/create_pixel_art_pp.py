# Pixel-art post process material builder (Unreal Editor Python)
#
# 실행: 에디터 > Tools > Execute Python Script... > 이 파일 선택
#   또는 Output Log (Python 모드)에서:  py "Tools/PixelArtPostProcess/create_pixel_art_pp.py"
#
# 생성물 (/Game/PostProcess/PixelArt):
#   M_PP_PixelArt              - 부모 머티리얼 (Custom HLSL = PixelArt.hlsl)
#   MI_PixelArt_Natural        - 원본 색감 + 디더
#   MI_PixelArt_Handheld       - 노랑-보라 그라디언트 (GBA 레퍼런스)
#   MI_PixelArt_NavyOutline    - 남색 팔레트 + 외곽선 (고양이 레퍼런스)
#   MI_PixelArt_Night          - 어두운 남색 야간 (가로등 레퍼런스)
#
# 다시 실행하면 머티리얼 그래프를 지우고 다시 만들며, 인스턴스 값도 프리셋으로 덮어씀.

import os
import unreal

ASSET_DIR = "/Game/PostProcess/PixelArt"
MATERIAL_NAME = "M_PP_PixelArt"
HLSL_PATH = os.path.join(unreal.Paths.project_dir(), "Tools", "PixelArtPostProcess", "PixelArt.hlsl")

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def hex_to_linear(hex_str):
    """sRGB hex -> LinearColor (shader converts back with pow(1/2.2))."""
    h = hex_str.lstrip("#")
    r, g, b = (int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
    return unreal.LinearColor(r ** 2.2, g ** 2.2, b ** 2.2, 1.0)


# name -> default value (custom node input order follows this list)
SCALAR_PARAMS = [
    ("PixelSize", 4.0),
    ("ColorSteps", 6.0),
    ("DitherStrength", 1.0),
    ("Saturation", 1.0),
    ("PaletteMix", 0.0),
    ("OutlineStrength", 0.0),
    ("OutlineThreshold", 0.05),
]
VECTOR_PARAMS = [
    ("Pal0", hex_to_linear("#0f0f1b")),
    ("Pal1", hex_to_linear("#565a75")),
    ("Pal2", hex_to_linear("#c6b7be")),
    ("Pal3", hex_to_linear("#fafbf6")),
    ("OutlineColor", hex_to_linear("#0b0b14")),
]

PRESETS = {
    "MI_PixelArt_Natural": {
        "scalars": {"PixelSize": 4, "ColorSteps": 8, "DitherStrength": 0.8, "PaletteMix": 0.0},
        "vectors": {},
    },
    "MI_PixelArt_Handheld": {
        "scalars": {"PixelSize": 5, "ColorSteps": 7, "DitherStrength": 1.0, "PaletteMix": 1.0},
        "vectors": {"Pal0": "#1b1124", "Pal1": "#5b4a8a", "Pal2": "#a99bd6", "Pal3": "#d6dc8a"},
    },
    "MI_PixelArt_NavyOutline": {
        "scalars": {"PixelSize": 6, "ColorSteps": 5, "DitherStrength": 0.6, "PaletteMix": 1.0,
                    "OutlineStrength": 1.0, "OutlineThreshold": 0.04},
        "vectors": {"Pal0": "#14152b", "Pal1": "#2e3560", "Pal2": "#56659a", "Pal3": "#8fa0c8",
                    "OutlineColor": "#0e0f1f"},
    },
    "MI_PixelArt_Night": {
        "scalars": {"PixelSize": 3, "ColorSteps": 6, "DitherStrength": 1.0, "Saturation": 1.2,
                    "PaletteMix": 0.85},
        "vectors": {"Pal0": "#000000", "Pal1": "#1c1f5e", "Pal2": "#5a5fb0", "Pal3": "#f1e4f4"},
    },
}


def load_or_create(name, asset_class, factory):
    path = f"{ASSET_DIR}/{name}"
    if EAL.does_asset_exist(path):
        return EAL.load_asset(path)
    return asset_tools.create_asset(name, ASSET_DIR, asset_class, factory)


def build_material():
    with open(HLSL_PATH, "r", encoding="utf-8") as f:
        code = f.read()

    mat = load_or_create(MATERIAL_NAME, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)

    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "PixelArt")

    input_names = (["UV"] + [n for n, _ in SCALAR_PARAMS] + [n for n, _ in VECTOR_PARAMS]
                   + ["DummyColor", "DummyDepth"])
    inputs = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    y = -600
    screen_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -800, y)
    MEL.connect_material_expressions(screen_pos, "ViewportUV", custom, "UV")

    for name, value in SCALAR_PARAMS:
        y += 80
        node = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        MEL.connect_material_expressions(node, "", custom, name)

    for name, value in VECTOR_PARAMS:
        y += 160
        node = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        MEL.connect_material_expressions(node, "", custom, name)

    # SceneTexture nodes make the material bind PostProcessInput0 / SceneDepth for SceneTextureLookup()
    for input_name, tex_id, offset in (("DummyColor", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, 0),
                                       ("DummyDepth", unreal.SceneTextureId.PPI_SCENE_DEPTH, 120)):
        node = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -800, y + 200 + offset)
        node.set_editor_property("scene_texture_id", tex_id)
        MEL.connect_material_expressions(node, "Color", custom, input_name)

    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_presets(parent):
    for name, preset in PRESETS.items():
        mi = load_or_create(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, parent)
        MEL.clear_all_material_instance_parameters(mi)
        for p, v in preset["scalars"].items():
            MEL.set_material_instance_scalar_parameter_value(mi, p, float(v))
        for p, v in preset["vectors"].items():
            MEL.set_material_instance_vector_parameter_value(mi, p, hex_to_linear(v))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)


material = build_material()
build_presets(material)
unreal.log(f"[PixelArt] Created {ASSET_DIR}/{MATERIAL_NAME} and {len(PRESETS)} presets")
