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
    with open("../Source/Gui/" + output_path + ".cpp", "w") as f:
        f.write(cpp_code)

    header_code = f"""#pragma once
""" + "\n".join(defs)
    with open("../Source/Gui/" + output_path + ".hpp", "w") as f:
        f.write(header_code)

if __name__ == "__main__":

    input_files = [
        ("../Thirdparty/jetbrains-mono/JetBrainsMono-Regular.ttf", "Font"),
        ("../Thirdparty/cereal/LICENSE.txt", "CerealLicense"),
        ("../Thirdparty/flags/LICENSE.txt", "FlagsLicense"),
        ("../Thirdparty/glad/LICENSE.txt", "GladLicense"),
        ("../Thirdparty/glfw/LICENSE.txt", "GlfwLicense"),
        ("../Thirdparty/imgui/LICENSE.txt", "ImguiLicense"),
        ("../Thirdparty/implot/LICENSE.txt", "ImPlotLicense"),
        ("../Thirdparty/inih/LICENSE.txt", "InihLicense"),
        ("../Thirdparty/json11/LICENSE.txt", "Json11License"),
        ("../Thirdparty/sigslot/LICENSE.txt", "SigSlotLicense"),
        ("../Thirdparty/spdlog/LICENSE.txt", "SpdlogLicense"),
        ("../Thirdparty/stb/LICENSE.txt", "StbLicense"),
        ("../Thirdparty/tracy/LICENSE.txt", "TracyLicense"),
        ("../Thirdparty/jetbrains-mono/LICENSE.txt", "JetbrainsMonoLicense"),
        ("./I18n/de_DE.json", "de_DE"),
        ("./I18n/en_US.json", "en_US"),
        ("./Icon.png", "Icon"),
        ("./watermark.png", "WatermarkImage"),
        ("./atlas.png", "IconAtlas"),
        ("../Thirdparty/flags/atlas.png", "FlagAtlas"),
        ("./BuiltinProtocolDB.json", "BuiltinProtocolDB")
    ]
    output_cpp = "Assets"
    co = []
    em = []
    for i in input_files:
        f, n = i
        code, embed = generate_embed(f, n)
        co.append(code)
        em.append(embed)

    output_to_cpp(co, em, output_cpp)
    print(f"Generated {output_cpp} with embedded data.")
