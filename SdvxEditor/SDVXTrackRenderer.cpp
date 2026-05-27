#include "SDVXTrackRenderer.h"

#include "MikuMikuWorld/Colors.h"

#include <algorithm>
#include <cmath>

namespace SdvxEditor
{
	SDVXTrackRenderer::SDVXTrackRenderer(Style style)
	    : style_{ style }
	{
	}

	ImU32 SDVXTrackRenderer::withAlpha(ImU32 col, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		const auto a = static_cast<unsigned int>(alpha * 255.0f);
		return (col & 0x00FFFFFFu) | (a << 24);
	}

	void SDVXTrackRenderer::addGlowLine(ImDrawList* drawList,
	                                    const ImVec2& p1,
	                                    const ImVec2& p2,
	                                    ImU32 col,
	                                    float glowThickness,
	                                    float mainThickness)
	{
		if (!drawList)
			return;

		// Outer soft line
		drawList->AddLine(p1, p2, withAlpha(col, 0.25f), glowThickness);
		// Inner crisp line
		drawList->AddLine(p1, p2, col, mainThickness);
	}

	void SDVXTrackRenderer::render(const ChartManager& chartManager)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (!drawList)
			return;

		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const ImVec2 size = ImGui::GetContentRegionAvail();
		const ImVec2 min = pos;
		const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

		drawList->PushClipRect(min, max, true);

		// Background
		drawList->AddRectFilled(min, max, style_.highwayBg);

		// Lane layout: 4 BT lanes, 2 FX lanes (each 2x BT width, overlapping).
		const float totalBtUnits = 4.0f;
		const float totalFxUnits = 4.0f; // 2 lanes * 2 BT width each
		const float laneUnitWidth = size.x / totalBtUnits;

		// BT lane x positions (0..4 units across width).
		auto btX = [&](int lane) {
			return min.x + laneUnitWidth * static_cast<float>(lane);
		};

		// FX spans: left covers BT 0 & 1, right covers BT 2 & 3.
		const float fxLeftMinX = btX(0);
		const float fxLeftMaxX = btX(2);
		const float fxRightMinX = btX(2);
		const float fxRightMaxX = btX(4);

		// Draw BT dividers using existing editor colors.
		for (int l = 0; l <= 4; ++l)
		{
			const float x = btX(l);
			const bool bold = (l == 0 || l == 2 || l == 4);
			const ImU32 col = bold ? MikuMikuWorld::divColor1 : MikuMikuWorld::divColor2;
			const float thickness = bold ? style_.btDividerThickness : style_.btDividerThickness * 0.6f;

			drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), col, thickness);
		}

		// FX overlays: filled rectangles with a vertical glow edge.
		const ImU32 fxBase = ImGui::GetColorU32(ImVec4(1.00f, 0.55f, 0.15f, 1.00f));
		const ImU32 fxFill = withAlpha(fxBase, style_.fxOverlayAlpha);

		// Left FX region.
		drawList->AddRectFilled(ImVec2(fxLeftMinX, min.y), ImVec2(fxLeftMaxX, max.y), fxFill);
		addGlowLine(drawList, ImVec2(fxLeftMinX, min.y), ImVec2(fxLeftMinX, max.y), fxBase,
		            style_.fxGlowThickness, style_.fxDividerThickness);
		addGlowLine(drawList, ImVec2(fxLeftMaxX, min.y), ImVec2(fxLeftMaxX, max.y), fxBase,
		            style_.fxGlowThickness, style_.fxDividerThickness);

		// Right FX region.
		drawList->AddRectFilled(ImVec2(fxRightMinX, min.y), ImVec2(fxRightMaxX, max.y), fxFill);
		addGlowLine(drawList, ImVec2(fxRightMinX, min.y), ImVec2(fxRightMinX, max.y), fxBase,
		            style_.fxGlowThickness, style_.fxDividerThickness);
		addGlowLine(drawList, ImVec2(fxRightMaxX, min.y), ImVec2(fxRightMaxX, max.y), fxBase,
		            style_.fxGlowThickness, style_.fxDividerThickness);

		// Horizontal grid + cursor based on currentPulse.
		const kson::Pulse centerPulse = chartManager.currentPulse();
		const float centerY = min.y + style_.cursorYNormalized * size.y;

		auto pulseToScreenY = [&](kson::Pulse pulse) {
			const double delta = static_cast<double>(pulse - centerPulse);
			return centerY - static_cast<float>(delta * style_.pixelsPerPulse);
		};

		// Major/minor grid lines in the visible pulse range.
		const float maxSpanPixels = size.y * 1.5f;
		const kson::Pulse spanPulses =
		    static_cast<kson::Pulse>(maxSpanPixels / std::max(style_.pixelsPerPulse, 1e-3f));

		const kson::Pulse startPulse = centerPulse - spanPulses;
		const kson::Pulse endPulse = centerPulse + spanPulses;

		auto drawGrid = [&](kson::Pulse step, ImU32 col, float thickness) {
			if (step <= 0)
				return;

			// Align first line <= startPulse.
			kson::Pulse base = (startPulse / step) * step;
			if (base > startPulse)
				base -= step;

			for (kson::Pulse p = base; p <= endPulse; p += step)
			{
				const float y = pulseToScreenY(p);
				if (y < min.y - 4.0f || y > max.y + 4.0f)
					continue;

				drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), col, thickness);
			}
		};

		// Use measureColor / divColor2 to stay visually consistent with MMW.
		drawGrid(style_.gridStepMajor, MikuMikuWorld::measureColor, 1.5f);
		drawGrid(style_.gridStepMinor, MikuMikuWorld::divColor2, 1.0f);

		// Notes (chips + longs)
		drawNotes(chartManager, drawList, min, max, laneUnitWidth, centerPulse, centerY);

		// Current cursor line using the same red as the timeline cursor.
		const float cursorY = pulseToScreenY(centerPulse);
		const int triPtOffset = 6;
		const float triX = min.x - (triPtOffset * 2.0f);

		drawList->AddTriangleFilled(ImVec2(triX, cursorY - triPtOffset),
		                            ImVec2(triX, cursorY + triPtOffset),
		                            ImVec2(triX + (triPtOffset * 2.0f), cursorY),
		                            MikuMikuWorld::cursorColor);
		drawList->AddLine(ImVec2(min.x, cursorY), ImVec2(max.x, cursorY),
		                  MikuMikuWorld::cursorColor, 2.0f);

		drawList->PopClipRect();

		// Consume the space so later widgets appear below the highway.
		ImGui::Dummy(size);
	}

	void SDVXTrackRenderer::drawNotes(const ChartManager& chartManager,
	                                 ImDrawList* drawList,
	                                 const ImVec2& min,
	                                 const ImVec2& max,
	                                 float laneUnitWidth,
	                                 kson::Pulse centerPulse,
	                                 float centerY) const
	{
		if (!drawList)
			return;

		auto pulseToScreenY = [&](kson::Pulse pulse) {
			const double delta = static_cast<double>(pulse - centerPulse);
			return centerY - static_cast<float>(delta * style_.pixelsPerPulse);
		};

		// Visible pulse range with a small guard band.
		const float guardPixels = 48.0f;
		const kson::Pulse guardPulses =
		    static_cast<kson::Pulse>(guardPixels / std::max(style_.pixelsPerPulse, 1e-3f));

		const kson::Pulse spanPulses =
		    static_cast<kson::Pulse>((max.y - min.y + guardPixels * 2.0f) /
		                             std::max(style_.pixelsPerPulse, 1e-3f));

		const kson::Pulse startPulse = centerPulse - spanPulses - guardPulses;
		const kson::Pulse endPulse = centerPulse + spanPulses + guardPulses;

		// Helpers for note rects.
		const float btPaddingX = std::max(1.0f, laneUnitWidth * 0.10f);
		const float fxPaddingX = std::max(1.0f, laneUnitWidth * 0.12f);

		const float chipHalfHeight = 7.0f;
		const float minLongHeight = 6.0f;

		auto drawChip = [&](float x1, float x2, float y, ImU32 baseCol) {
			ImVec2 p1{ x1, y - chipHalfHeight };
			ImVec2 p2{ x2, y + chipHalfHeight };
			drawList->AddRectFilled(p1, p2, withAlpha(baseCol, 0.85f), 2.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddRect(p1, p2, withAlpha(baseCol, 1.0f), 2.0f, ImDrawFlags_RoundCornersAll, 1.5f);
			// small glow
			drawList->AddRect(p1 - ImVec2(1, 1), p2 + ImVec2(1, 1), withAlpha(baseCol, 0.25f), 2.5f,
			                  ImDrawFlags_RoundCornersAll, 3.0f);
		};

		auto drawLong = [&](float x1, float x2, float y1, float y2, ImU32 baseCol) {
			const float top = std::min(y1, y2);
			const float bot = std::max(y1, y2);
			if (bot - top < minLongHeight)
				return;

			ImVec2 p1{ x1, top };
			ImVec2 p2{ x2, bot };
			drawList->AddRectFilled(p1, p2, withAlpha(baseCol, 0.35f), 2.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddRect(p1, p2, withAlpha(baseCol, 0.90f), 2.0f, ImDrawFlags_RoundCornersAll, 1.25f);
			drawList->AddRect(p1 - ImVec2(1, 1), p2 + ImVec2(1, 1), withAlpha(baseCol, 0.18f), 2.5f,
			                  ImDrawFlags_RoundCornersAll, 3.0f);
		};

		// Colors
		const ImU32 btCol = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		const ImU32 fxCol = ImGui::GetColorU32(ImVec4(1.00f, 0.55f, 0.15f, 1.00f));

		// BT lanes: 4 lanes, each 1 unit wide.
		for (int lane = 0; lane < 4; ++lane)
		{
			const float laneMinX = min.x + laneUnitWidth * static_cast<float>(lane) + btPaddingX;
			const float laneMaxX = min.x + laneUnitWidth * static_cast<float>(lane + 1) - btPaddingX;

			const auto& map = chartManager.btLanes()[static_cast<std::size_t>(lane)];
			for (auto it = map.lower_bound(startPulse); it != map.end() && it->first <= endPulse; ++it)
			{
				const kson::Pulse p = it->first;
				const kson::RelPulse len = it->second.length;
				const float y = pulseToScreenY(p);
				if (y < min.y - guardPixels || y > max.y + guardPixels)
					continue;

				if (len <= 0)
				{
					drawChip(laneMinX, laneMaxX, y, btCol);
				}
				else
				{
					const float y2 = pulseToScreenY(p + len);
					drawLong(laneMinX, laneMaxX, y, y2, btCol);
				}
			}
		}

		// FX lanes: 2 lanes, each spans 2 BT lanes (overlapping).
		for (int lane = 0; lane < 2; ++lane)
		{
			const float fxMinX = min.x + laneUnitWidth * static_cast<float>(lane * 2) + fxPaddingX;
			const float fxMaxX = min.x + laneUnitWidth * static_cast<float>(lane * 2 + 2) - fxPaddingX;

			const auto& map = chartManager.fxLanes()[static_cast<std::size_t>(lane)];
			for (auto it = map.lower_bound(startPulse); it != map.end() && it->first <= endPulse; ++it)
			{
				const kson::Pulse p = it->first;
				const kson::RelPulse len = it->second.length;
				const float y = pulseToScreenY(p);
				if (y < min.y - guardPixels || y > max.y + guardPixels)
					continue;

				if (len <= 0)
				{
					drawChip(fxMinX, fxMaxX, y, fxCol);
				}
				else
				{
					const float y2 = pulseToScreenY(p + len);
					drawLong(fxMinX, fxMaxX, y, y2, fxCol);
				}
			}
		}
	}
} // namespace SdvxEditor

