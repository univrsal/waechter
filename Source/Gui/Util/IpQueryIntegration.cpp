//
// Created by usr on 04/11/2025.
//

#include "IpQueryIntegration.hpp"

#include <algorithm>

#include "Windows/GlfwWindow.hpp"

WIpInfoData const* WIpQueryIntegration::GetIpInfo(std::string const& IP)
{
	{
		std::lock_guard Lock(Mutex);
		if (auto It = IpInfoCache.find(IP); It != IpInfoCache.end())
		{
			return &It->second;
		}
	}

	WIpInfoData NewData{};

	std::thread QueryThread([this, QT_IP = IP] {
		std::string Error;
		auto        Json =
			WGlfwWindow::GetInstance().GetMainWindow()->GetLibCurl().GetJson("https://api.ipquery.io/" + QT_IP, Error);

		WIpInfoData ResultData{};
		auto        Isp = Json["isp"];

		if (Isp.is_object())
		{
			ResultData.Asn = Isp["asn"].string_value();
			ResultData.Organization = Isp["org"].string_value();
			ResultData.Isp = Isp["isp"].string_value();
		}

		auto Location = Json["location"];
		if (Location.is_object())
		{
			ResultData.Country = Location["country"].string_value();
			auto CC = ResultData.CountryCode = Location["country_code"].string_value();
			// convert to lowercase
			std::ranges::transform(CC, ResultData.CountryCode.begin(), ::tolower);
			ResultData.City = Location["city"].string_value();
			ResultData.Latitude = Location["latitude"].number_value();
			ResultData.Longitude = Location["longitude"].number_value();
		}

		auto Risk = Json["risk"];
		if (Risk.is_object())
		{
			ResultData.bIsDatacenter = Risk["is_datacenter"].bool_value();
			ResultData.bIsMobile = Risk["is_mobile"].bool_value();
			ResultData.bIsProxy = Risk["is_proxy"].bool_value();
			ResultData.bIsTor = Risk["is_tor"].bool_value();
			ResultData.bIsVPN = Risk["is_vpn"].bool_value();
			ResultData.RiskScore = static_cast<int>(Risk["risk_score"].number_value());
		}

		ResultData.MapsLink = "https://www.google.com/maps/search/?api=1&query=" + std::to_string(ResultData.Latitude)
			+ "," + std::to_string(ResultData.Longitude);
		ResultData.bIsPendingRequest = false;
		std::lock_guard Lock(Mutex);
		IpInfoCache[QT_IP] = ResultData;
	});
	QueryThread.detach();
	IpInfoCache.emplace(IP, NewData);
	return &IpInfoCache[IP];
}

void WIpQueryIntegration::Draw(std::shared_ptr<WSocketItem> Sock)
{
	if (HasIpInfoForIp(Sock->SocketTuple.RemoteEndpoint.Address.ToString()))
	{
		if (CurrentSocketItemId != Sock->ItemId)
		{
			CurrentIpInfo = GetIpInfo(Sock->SocketTuple.RemoteEndpoint.Address.ToString());
			CurrentSocketItemId = Sock->ItemId;
		}

		if (CurrentIpInfo->bIsPendingRequest)
		{
			ImGui::Text("Ip lookup pending...");
		}
		else if (CurrentIpInfo->Asn != "AS0") // That's the value when it fails to lookup
		{
			WGlfwWindow::GetInstance().GetMainWindow()->GetFlagAtlas().DrawFlag(
				CurrentIpInfo->CountryCode, ImVec2(24, 18));
			ImGui::SameLine();
			ImGui::Text("%s", CurrentIpInfo->Country.c_str());
			ImGui::InputText("City", const_cast<char*>(CurrentIpInfo->City.c_str()), 64, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("ISP", const_cast<char*>(CurrentIpInfo->Isp.c_str()), 128, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Organization", const_cast<char*>(CurrentIpInfo->Organization.c_str()), 128,
				ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("ASN", const_cast<char*>(CurrentIpInfo->Asn.c_str()), 64, ImGuiInputTextFlags_ReadOnly);
			ImGui::Text("Latitude: %.6f", CurrentIpInfo->Latitude);
			ImGui::Text("Longitude: %.6f", CurrentIpInfo->Longitude);
			ImGui::Text("Risk Score: %d", CurrentIpInfo->RiskScore);
			ImGui::Text("Is VPN: %s", CurrentIpInfo->bIsVPN ? "Yes" : "No");
			ImGui::Text("Is Proxy: %s", CurrentIpInfo->bIsProxy ? "Yes" : "No");
			ImGui::Text("Is Tor: %s", CurrentIpInfo->bIsTor ? "Yes" : "No");
			ImGui::Text("Is Datacenter: %s", CurrentIpInfo->bIsDatacenter ? "Yes" : "No");
			ImGui::Text("Is Mobile: %s", CurrentIpInfo->bIsMobile ? "Yes" : "No");

			ImGui::TextLinkOpenURL("View on Google Maps", CurrentIpInfo->MapsLink.c_str());
		}
		else
		{
			ImGui::Text("No information found for this IP.");
		}
	}
	else if (ImGui::Button("ipquery.io lookup"))
	{
		CurrentIpInfo = GetIpInfo(Sock->SocketTuple.RemoteEndpoint.Address.ToString());
		CurrentSocketItemId = Sock->ItemId;
	}
}