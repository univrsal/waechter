/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MemoryUsageWindow.hpp"

#include "Format.hpp"
#include "Util/I18n.hpp"

#include <imgui.h>

void WMemoryUsageWindow::Draw()
{
	if (!bVisible)
	{
		return;
	}
	if (ImGui::Begin(TR("window.memory_usage"), &bVisible, ImGuiWindowFlags_NoDocking))
	{
		if (Stats.Stats.empty())
		{
			ImGui::TextDisabled("No memory statistics available");
		}
		else
		{
			std::scoped_lock Lock(Mutex);
			ImGui::Text("Total Memory Usage: %.2f MB", static_cast<double>(TotalUsage) / (1024.0 * 1024.0));
			ImGui::Separator();

			// Draw each category as a tree node
			for (auto const& Stat : Stats.Stats)
			{
				WBytes CategoryTotal = 0;
				for (auto const& Entry : Stat.ChildEntries)
				{
					CategoryTotal += Entry.Usage;
				}

				// Format the tree node header with category name and total
				ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen;
				if (Stat.ChildEntries.empty())
					Flags |= ImGuiTreeNodeFlags_Leaf;
				bool bOpen = ImGui::TreeNodeEx(Stat.Name.c_str(), Flags, "%s: %s", Stat.Name.c_str(),
					WStorageFormat::AutoFormat(CategoryTotal).c_str());

				if (bOpen)
				{
					// Draw child entries
					for (auto const& Entry : Stat.ChildEntries)
					{
						ImGui::TreeNodeEx(Entry.Name.c_str(),
							ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s: %s", Entry.Name.c_str(),
							WStorageFormat::AutoFormat(Entry.Usage).c_str());
					}
					ImGui::TreePop();
				}
			}
		}
	}
	ImGui::End();
}