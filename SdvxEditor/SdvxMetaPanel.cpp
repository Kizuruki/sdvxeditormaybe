#include "SdvxMetaPanel.h"
#include <algorithm>
#include <cstring>
#include "ImGui/imgui.h"

namespace SdvxEditor
{
    void drawMetaPanel(ChartManager& chart)
    {
        kson::MetaInfo& meta = chart.chart().meta;
        kson::BeatInfo& beat = chart.chart().beat;

        ImGui::SeparatorText("Metadata");

        auto strField = [](const char* label, std::string& str)
        {
            char buf[256];
            const size_t copyLen = std::min(str.size(), sizeof(buf) - 1);
            std::memcpy(buf, str.data(), copyLen);
            buf[copyLen] = '\0';
            if (ImGui::InputText(label, buf, sizeof(buf)))
                str = buf;
        };

        strField("Title",         meta.title);
        strField("Title Romaji",  meta.titleTranslit);
        strField("Artist",        meta.artist);
        strField("Chart Author",  meta.chartAuthor);
        strField("Disp BPM",      meta.dispBPM);
        strField("Jacket File",   meta.jacketFilename);
        strField("Jacket Author", meta.jacketAuthor);
        strField("Information",   meta.information);

        ImGui::InputInt("Level", &meta.level);
        meta.level = std::clamp(meta.level, 1, 20);

        static const char* diffNames[] = { "NOV", "ADV", "EXH", "MXM" };
        ImGui::Combo("Difficulty", &meta.difficulty.idx, diffNames, 4);

        ImGui::SeparatorText("BPM");

        kson::Pulse toRemove = -1;
        for (auto& [pulse, bpm] : beat.bpm)
        {
            ImGui::PushID(static_cast<int>(pulse));
            ImGui::Text("Pulse %lld", static_cast<long long>(pulse));
            ImGui::SameLine();
            double b = bpm;
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputDouble("BPM##v", &b, 0.5, 1.0, "%.2f"))
            {
                b = std::clamp(b, 1.0, 9999.0);
                bpm = b;
                chart.rebuildTimingCache();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                toRemove = pulse;
            ImGui::PopID();
        }
        if (toRemove >= 0)
        {
            beat.bpm.erase(toRemove);
            chart.rebuildTimingCache();
        }

        static double newBpm = 120.0;
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputDouble("New BPM", &newBpm, 0.5, 1.0, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Add at cursor"))
        {
            newBpm = std::clamp(newBpm, 1.0, 9999.0);
            beat.bpm.insert_or_assign(chart.currentPulse(), newBpm);
            chart.rebuildTimingCache();
        }

        ImGui::SeparatorText("Time Signature");

        kson::Pulse tsToRemove = -1;
        for (auto& [measureIdx, ts] : beat.timeSig)
        {
            ImGui::PushID(static_cast<int>(measureIdx));
            ImGui::Text("Measure %lld", static_cast<long long>(measureIdx));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50.0f);
            ImGui::InputInt("##n", &ts.n);
            ImGui::SameLine(); ImGui::TextUnformatted("/");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50.0f);
            ImGui::InputInt("##d", &ts.d);
            ts.n = std::max(1, ts.n);
            ts.d = std::max(1, ts.d);
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                tsToRemove = measureIdx;
            ImGui::PopID();
        }
        if (tsToRemove >= 0)
            beat.timeSig.erase(tsToRemove);
    }
}