/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "imgui.h"

#include "Format.hpp"

inline bool DrawUnitCombo(ETrafficUnit& Unit, char const* Label = "Unit", bool bWithAuto = true)
{
	auto UnitText = [Unit] {
		switch (Unit)
		{
			case TU_Bps:
				return "B/s";
			case TU_KiBps:
				return "KiB/s";
			case TU_MiBps:
				return "MiB/s";
			case TU_GiBps:
				return "GiB/s";
			case TU_Auto:
				return "Auto";
			default:
				return "B/s";
		}
	};

	bool bChanged = false;

	if (ImGui::BeginCombo(Label, UnitText(), ImGuiComboFlags_WidthFitPreview))
	{
		if (ImGui::Selectable("B/s", Unit == TU_Bps))
		{
			Unit = TU_Bps;
			bChanged = true;
		}
		if (ImGui::Selectable("KiB/s", Unit == TU_KiBps))
		{
			Unit = TU_KiBps;
			bChanged = true;
		}
		if (ImGui::Selectable("MiB/s", Unit == TU_MiBps))
		{
			Unit = TU_MiBps;
			bChanged = true;
		}
		if (ImGui::Selectable("GiB/s", Unit == TU_GiBps))
		{
			Unit = TU_GiBps;
			bChanged = true;
		}
		if (bWithAuto && ImGui::Selectable("Auto", Unit == TU_Auto))
		{
			Unit = TU_Auto;
			bChanged = true;
		}
		ImGui::EndCombo();
	}
	return bChanged;
}