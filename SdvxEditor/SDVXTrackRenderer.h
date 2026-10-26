#pragma once

#include "ChartManager.h"
#include "ImGui/imgui.h"

namespace SdvxEditor
{
	class SDVXTrackRenderer
	{
	  public:
		struct Style
		{
			// How many screen pixels correspond to one libkson pulse.
			// Higher => faster scroll / more zoomed-in.
			float pixelsPerPulse = 0.75f;

			// Cursor position inside the highway (0 = top, 1 = bottom).
			float cursorYNormalized = 0.55f;

			// Horizontal grid spacing in pulses.
			kson::Pulse gridStepMajor = kson::kResolution;      // typically 1 beat
			kson::Pulse gridStepMinor = kson::kResolution / 2;  // subdivisions

			// Vertical line thickness.
			float btDividerThickness = 1.25f;
			float fxDividerThickness = 2.0f;
			float fxGlowThickness = 6.0f;

			// FX overlay fill alpha (drawn on top of BT).
			float fxOverlayAlpha = 0.12f;

			// Background color under the lanes.
			ImU32 highwayBg = IM_COL32(26, 26, 26, 255);  // same 0.10,0.10,0.10,1.0
		};

		explicit SDVXTrackRenderer(Style style = {});

		// Draw a vertical 4 BT + 2 FX highway using the current ImGui window's content region.
		// The vertical scroll is centered around ChartManager::currentPulse().
		void render(ChartManager& chartManager);

		void setStyle(const Style& style) { style_ = style; }
		[[nodiscard]] const Style& getStyle() const noexcept { return style_; }

	  private:
		Style style_{};

		static ImU32 withAlpha(ImU32 col, float alpha);
		static void addGlowLine(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 col,
		                        float glowThickness, float mainThickness);

		void drawNotes(const ChartManager& chartManager,
		               ImDrawList* drawList,
		               const ImVec2& min,
		               const ImVec2& max,
		               float laneUnitWidth,
		               kson::Pulse centerPulse,
		               float centerY) const;
	};
} // namespace SdvxEditor

