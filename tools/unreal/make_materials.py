"""Build the project's materials in code (run by BuildEditor.bat after a successful build):

  UnrealEditor-Cmd.exe HigataZukan.uproject -run=pythonscript -script=tools/unreal/make_materials.py

Writes /Game/Generated (Content/Generated, not in git) from scratch every run, so the materials live as reviewable
text here instead of binary assets. The C++ falls back to plain colours when they are missing.

M_HigataGround  vertex colour (dry substrate albedo), darker and glossy below the scalar WetLevelM (metres, set by
                AHigataFlat from the tide every frame).
M_HigataWater   Single Layer Water: clear-ish, slightly green bay water that shows the flat under shallow water.

Prints HIGATA_MATERIALS_OK at the end when everything connected; BuildEditor.bat looks for it.
"""
import unreal

ROOT = '/Game/Generated'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def norm(name):
    return str(name).replace(' ', '').lower()


def new_material(name):
    path = f'{ROOT}/{name}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    expr = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def connect(src, dst, dst_input='', src_output=''):
    """Connect src's output to dst's input; input names are matched loosely (spaces and case ignored)."""
    if mel.connect_material_expressions(src, src_output, dst, dst_input):
        return
    try:
        names = [str(n) for n in mel.get_material_expression_input_names(dst)]
    except Exception:  # older engines have no input-name query
        names = []
    for name in names:
        if norm(name) == norm(dst_input) and mel.connect_material_expressions(src, src_output, dst, name):
            return
    problems.append(f'cannot connect {src.get_name()}.{src_output!r} -> {dst.get_name()}.{dst_input!r} (inputs: {names})')


def to_property(src, prop, src_output=''):
    if not mel.connect_material_property(src, src_output, prop):
        problems.append(f'cannot connect {src.get_name()}.{src_output!r} -> {prop}')


def finish(mat):
    mat.set_editor_property('two_sided', True)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    if not eal.save_loaded_asset(mat):
        problems.append(f'cannot save {mat.get_path_name()}')


def make_ground():
    mat = new_material('M_HigataGround')
    E = unreal
    albedo = node(mat, E.MaterialExpressionVertexColor, -1200, -300)
    wet_level = node(mat, E.MaterialExpressionScalarParameter, -1200, 0, parameter_name='WetLevelM', default_value=0.0)
    world = node(mat, E.MaterialExpressionWorldPosition, -1200, 200)
    z_cm = node(mat, E.MaterialExpressionComponentMask, -1000, 200, r=False, g=False, b=True, a=False)
    connect(world, z_cm)
    z_m = node(mat, E.MaterialExpressionDivide, -850, 200, const_b=100.0)
    connect(z_cm, z_m, 'A')
    # wet = saturate((WetLevelM - z) / 0.1 + 0.5): a 10 cm soft edge at the recent high-water mark
    below = node(mat, E.MaterialExpressionSubtract, -700, 100)
    connect(wet_level, below, 'A')
    connect(z_m, below, 'B')
    band = node(mat, E.MaterialExpressionDivide, -550, 100, const_b=0.1)
    connect(below, band, 'A')
    centred = node(mat, E.MaterialExpressionAdd, -400, 100, const_b=0.5)
    connect(band, centred, 'A')
    wet = node(mat, E.MaterialExpressionSaturate, -250, 100)
    connect(centred, wet)
    # wet sand and mud are about half as bright as dry, and much smoother (the sheen that mirrors the sky)
    darken = node(mat, E.MaterialExpressionLinearInterpolate, -100, -100, const_a=1.0, const_b=0.55)
    connect(wet, darken, 'Alpha')
    base = node(mat, E.MaterialExpressionMultiply, 100, -250)
    connect(albedo, base, 'A')
    connect(darken, base, 'B')
    rough = node(mat, E.MaterialExpressionLinearInterpolate, 100, 50, const_a=0.85, const_b=0.12)
    connect(wet, rough, 'Alpha')
    to_property(base, E.MaterialProperty.MP_BASE_COLOR)
    to_property(rough, E.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def make_water():
    mat = new_material('M_HigataWater')
    E = unreal
    mat.set_editor_property('shading_model', E.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property('blend_mode', E.BlendMode.BLEND_OPAQUE)
    base = node(mat, E.MaterialExpressionConstant3Vector, -600, -200, constant=E.LinearColor(0.0, 0.0, 0.0, 1.0))
    rough = node(mat, E.MaterialExpressionConstant, -600, 0, r=0.03)
    to_property(base, E.MaterialProperty.MP_BASE_COLOR)
    to_property(rough, E.MaterialProperty.MP_ROUGHNESS)
    out = node(mat, E.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 300)
    # per metre: red goes first, a little green-brown scattering like the bay's suspended sediment
    scattering = node(mat, E.MaterialExpressionConstant3Vector, -600, 200, constant=E.LinearColor(0.02, 0.035, 0.028, 1.0))
    absorption = node(mat, E.MaterialExpressionConstant3Vector, -600, 400, constant=E.LinearColor(0.45, 0.08, 0.07, 1.0))
    phase = node(mat, E.MaterialExpressionConstant, -600, 600, r=0.1)
    connect(scattering, out, 'Scattering Coefficients')
    connect(absorption, out, 'Absorption Coefficients')
    connect(phase, out, 'Phase G')
    finish(mat)


for make in (make_ground, make_water):
    try:
        make()
    except Exception as error:  # keep going so one broken material does not hide the other's problems
        problems.append(f'{make.__name__}: {error!r}')

if problems:
    for problem in problems:
        unreal.log_error(f'[Higata materials] {problem}')
    unreal.log_error('HIGATA_MATERIALS_FAILED')
else:
    unreal.log('HIGATA_MATERIALS_OK')
