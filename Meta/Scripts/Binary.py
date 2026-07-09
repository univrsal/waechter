import json


def bytes_to_cpp_array(data: bytes) -> str:

    return ", ".join(f"0x{b:02X}" for b in data)

def generate_embed(input_path: str, array_name: str):
    with open(input_path, "rb") as f:
        data = f.read()

    array_initializer = bytes_to_cpp_array(data)
    size = len(data)

    return f"""unsigned char const G{array_name}Data[] = {{
    {array_initializer}
}};
unsigned int const G{array_name}Size = {size};
""", f"""extern unsigned char const G{array_name}Data[];
extern unsigned int const G{array_name}Size;
"""

def output_to_cpp(arrays: list[str], defs: list[str], output_path: str):
    cpp_code = f"""#include "{output_path}.hpp"\n\n""" + "\n".join(arrays)
    with open("../../Source/Gui/" + output_path + ".cpp", "w") as f:
        f.write(cpp_code)

    header_code = f"""#pragma once
""" + "\n".join(defs)
    with open("../../Source/Gui/" + output_path + ".hpp", "w") as f:
        f.write(header_code)


with open("../I18n/en_US.json") as f:
    en = json.load(f)


def compare_structure(a, b, path=""):
    # Types must match
    if type(a) != type(b):
        return [f"{path}: type mismatch ({type(a).__name__} != {type(b).__name__})"]

    # Dictionaries
    if isinstance(a, dict):
        errors = []

        missing = a.keys() - b.keys()
        extra = b.keys() - a.keys()

        for key in sorted(missing):
            errors.append(f"{path}/{key}: missing")

        for key in sorted(extra):
            errors.append(f"{path}/{key}: extra")

        for key in sorted(a.keys() & b.keys()):
            errors.extend(compare_structure(a[key], b[key], f"{path}/{key}"))

        return errors

    # Lists
    if isinstance(a, list):
        errors = []

        if len(a) != len(b):
            errors.append(f"{path}: list length differs ({len(a)} != {len(b)})")
            return errors

        for i, (x, y) in enumerate(zip(a, b)):
            errors.extend(compare_structure(x, y, f"{path}[{i}]"))

        return errors

    return []


def check_lang_file(f):
    with open(f) as lang_file:
        lang_data = json.load(lang_file)
        errors = compare_structure(en, lang_data)
        if errors:
            print(f"Errors in {f}:")
            for error in errors:
                print(f"  {error}")

if __name__ == "__main__":

    input_files = [
        ("../../Thirdparty/jetbrains-mono/JetBrainsMono-Regular.ttf", "JetBrainsMono"),
        ("../../Thirdparty/adwaita-sans/AdwaitaSans-Regular.ttf", "AdwaitaSans"),
        ("../../Thirdparty/cereal/LICENSE.txt", "CerealLicense"),
        ("../../Thirdparty/flags/LICENSE.txt", "FlagsLicense"),
        ("../../Thirdparty/glad/LICENSE.txt", "GladLicense"),
        ("../../Thirdparty/sdl2/LICENSE.txt", "Sdl2License"),
        ("../../Thirdparty/imgui/LICENSE.txt", "ImguiLicense"),
        ("../../Thirdparty/implot/LICENSE.txt", "ImPlotLicense"),
        ("../../Thirdparty/mini/LICENSE.txt", "MiniLicense"),
        ("../../Thirdparty/json11/LICENSE.txt", "Json11License"),
        ("../../Thirdparty/sigslot/LICENSE.txt", "SigSlotLicense"),
        ("../../Thirdparty/spdlog/LICENSE.txt", "SpdlogLicense"),
        ("../../Thirdparty/stb/LICENSE.txt", "StbLicense"),
        ("../../Thirdparty/tracy/LICENSE.txt", "TracyLicense"),
        ("../../Thirdparty/traycon/LICENSE.txt", "TrayconLicense"),
        ("../../Thirdparty/adwaita-sans/LICENSE.txt", "AdwaitaSansLicense"),
        ("../../Thirdparty/jetbrains-mono/LICENSE.txt", "JetbrainsMonoLicense"),
        ("../../Thirdparty/sqlpp11/LICENSE.txt", "Sqlpp11License"),
        ("../../Thirdparty/date/LICENSE.txt", "DateLicense"),
        ("../../Thirdparty/flags/atlas.png", "FlagAtlas"),
        ("../../LICENSE", "WaechterLicense"),
        ("../I18n/de_DE.json", "de_DE"),
        ("../I18n/en_US.json", "en_US"),
        ("../Assets/AUTHORS", "Authors"),
        ("../Assets/Branding/Icon.png", "Icon"),
        ("../Assets/Watermark.png", "WatermarkImage"),
        ("../Assets/IconAtlas.png", "IconAtlas"),
        ("../BuiltinProtocolDB.json", "BuiltinProtocolDB")
    ]
    output_cpp = "Assets"
    co = []
    em = []
    for i in input_files:
        f, n = i

        if f.endswith(".json") and "I18n" in f and not f.endswith("en_US.json"):
            check_lang_file(f)
        code, embed = generate_embed(f, n)
        co.append(code)
        em.append(embed)

    output_to_cpp(co, em, output_cpp)
    print(f"Generated {output_cpp} with embedded data.")
