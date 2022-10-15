#include "MiniProfiler.h"
#include "ImGUI\imgui.h"

namespace DOG
{
	std::unordered_map<std::string, u64> MiniProfiler::s_times;
	std::unordered_map<std::string, MiniProfiler::RollingAvg> MiniProfiler::s_avg;
	bool MiniProfiler::s_isActive = true;

	MiniProfiler::MiniProfiler(const std::string& name) : m_name(name)
	{
		m_start = std::chrono::high_resolution_clock::now();
	}

	MiniProfiler::~MiniProfiler()
	{
		u64 time = static_cast<u64>(duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_start).count());
		s_times[m_name] = s_avg[m_name](time);
	}


	void MiniProfiler::DrawResultWithImGui(bool& open)
	{
		static bool allowMove = false;
		if (ImGui::BeginMenu("MiniProfiler"))
		{
			ImGui::MenuItem("Active", nullptr, &open);
			ImGui::MenuItem("Unlock", nullptr, &allowMove);
			ImGui::EndMenu(); // "MiniProfiler"
		}


		if (open)
		{
			//allowMove = true;
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (ImGui::Begin("MiniProfiler", &open, (allowMove ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground)))
			{
				static std::vector<std::pair<std::string, u64>> results;
				results.clear();
				results.reserve(MiniProfiler::s_times.size());
				for (auto& [n, t] : MiniProfiler::s_times)
					results.emplace_back(n, t);
				std::sort(results.begin(), results.end(), [](auto& a, auto& b) {return a.second > b.second; });

				if (ImGui::BeginTable("data", 2))
				{
					for (auto& [n, t] : results)
					{
						double milisec = 1E-6 * t;
						int styleStackToPop = 0;
						if (milisec > 16)
						{
							styleStackToPop++;
							ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
						}
						else if (milisec > 4)
						{
							styleStackToPop++;
							ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
						}
						else if (milisec < 1)
						{
							styleStackToPop++;
							ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
						}
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", n.c_str());
						
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%-6.2fms", milisec);

						ImGui::PopStyleColor(styleStackToPop);
					}
					ImGui::EndTable();
				}

				MiniProfiler::s_times.clear();
			}
			ImGui::End(); // "MiniProfiler"
			ImGui::PopStyleColor(1);
		}
	}


	u64 MiniProfiler::RollingAvg::operator()(u64 input)
	{
		m_sum -= m_previousInputs[m_index];
		m_sum += input;
		m_previousInputs[m_index] = input;
		if (++m_index == n)
			m_index = 0;
		return (m_sum + (n / 2)) / n;
	}
}
