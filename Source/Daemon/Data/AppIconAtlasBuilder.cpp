//
// Created by usr on 02/11/2025.
//

#include "AppIconAtlasBuilder.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <memory>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <stb_rect_pack.h>
#include <stb_image_resize2.h>

struct WPackedIcon
{
	std::string                      Path{};
	int                              W{}, H{};
	std::unique_ptr<unsigned char[]> Pixels{}; // RGBA
};

bool WAppIconAtlasBuilder::GetAtlasData(WAppIconAtlasData& outData, std::vector<std::string> const& BinaryNames, std::size_t AtlasSize, std::size_t IconSize)
{
	std::vector<WPackedIcon> PackedIcons{};
	PackedIcons.reserve(BinaryNames.size());
	spdlog::debug("Building atlas for {} apps", BinaryNames.size());

	for (auto const& BinaryName : BinaryNames)
	{
		std::string IconPath = Resolver.ResolveIcon(BinaryName);
		if (IconPath.empty())
		{
			continue;
		}

		int            IconW, IconH, N;
		unsigned char* Data = stbi_load(IconPath.c_str(), &IconW, &IconH, &N, 4); // Force RGBA
		if (!Data)
		{
			spdlog::warn("Failed to load app icon from path '{}'", IconPath);
			continue;
		}

		if (IconW > static_cast<int>(IconSize) || IconH > static_cast<int>(IconSize))
		{
			// Resize icon to fit
			auto ResizedData = std::make_unique<unsigned char[]>(IconSize * IconSize * 4);
			stbir_resize_uint8_srgb(Data, IconW, IconH, 0,
				ResizedData.get(), static_cast<int>(IconSize), static_cast<int>(IconSize), 0, STBIR_RGBA);
			stbi_image_free(Data);
			Data = ResizedData.release();
			IconW = static_cast<int>(IconSize);
			IconH = static_cast<int>(IconSize);
		}
		WPackedIcon Icon;
		Icon.Path = IconPath;
		Icon.W = IconW;
		Icon.H = IconH;
		Icon.Pixels = std::make_unique<unsigned char[]>(static_cast<std::size_t>(IconW * IconH * 4));
		std::memcpy(Icon.Pixels.get(), Data, static_cast<std::size_t>(IconW * IconH * 4));
		stbi_image_free(Data);
		PackedIcons.emplace_back(std::move(Icon));
	}

	auto                    NumIcons = PackedIcons.size();
	std::vector<stbrp_rect> Rects(NumIcons);
	for (std::size_t i = 0; i < NumIcons; ++i)
	{
		Rects[i].id = i;
		Rects[i].w = PackedIcons[i].W;
		Rects[i].h = PackedIcons[i].H;
		Rects[i].x = Rects[i].y = 0;
	}

	std::vector<stbrp_node> Nodes(AtlasSize);
	stbrp_context           Context;
	stbrp_init_target(&Context, AtlasSize, AtlasSize, Nodes.data(), Nodes.size());
	stbrp_pack_rects(&Context, Rects.data(), NumIcons);

	for (std::size_t i = 0; i < NumIcons; ++i)
	{
		if (Rects[i].was_packed == 0)
		{
			spdlog::error("Icon '{}' ({}/{})) does not fit in the atlas.", PackedIcons[i].Path, i, NumIcons);
			return false;
		}
	}

	outData.AtlasImageData.resize(AtlasSize * AtlasSize * 4, 0);
	for (std::size_t i = 0; i < NumIcons; ++i)
	{
		auto const X = static_cast<std::size_t>(Rects[i].x);
		auto const Y = static_cast<std::size_t>(Rects[i].y);
		auto const W = static_cast<std::size_t>(PackedIcons[i].W);
		auto const H = static_cast<std::size_t>(PackedIcons[i].H);

		// Copy icon pixels into atlas
		for (std::size_t Row = 0; Row < H; ++Row)
		{
			auto Idx = ((Y + Row) * AtlasSize + X) * 4;
			auto Idx2 = Row * W * 4;
			std::memcpy(
				&outData.AtlasImageData[Idx],
				&PackedIcons[i].Pixels[Idx2],
				W * 4);
		}

		// Calculate UV coordinates
		WAtlasUv Uv{};
		Uv.U1 = static_cast<float>(X) / static_cast<float>(AtlasSize);
		Uv.V1 = static_cast<float>(Y) / static_cast<float>(AtlasSize);
		Uv.U2 = static_cast<float>(X + W) / static_cast<float>(AtlasSize);
		Uv.V2 = static_cast<float>(Y + H) / static_cast<float>(AtlasSize);

		outData.UvData[BinaryNames[i]] = Uv;
	}
	outData.AtlasSize = static_cast<int>(AtlasSize);

	return !outData.UvData.empty();
}