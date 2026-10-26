#pragma once
#include <kson/kson.hpp>
#include <filesystem>
#include <optional>
#include <string>

namespace SdvxEditor {

class ChartManager {
public:
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }
    [[nodiscard]] const kson::ChartData& chart() const noexcept { return chart_; }
    [[nodiscard]] kson::ChartData& chart() noexcept { return chart_; }
    [[nodiscard]] const kson::TimingCache& timing() const noexcept { return timing_; }

    kson::ErrorType loadKson(const std::filesystem::path& path);
    kson::ErrorType saveKson(const std::filesystem::path& path) const;

    void rebuildTimingCache();

    // Current playhead position in kson::Pulse units (used by renderers).
    [[nodiscard]] kson::Pulse currentPulse() const noexcept { return currentPulse_; }
    void setCurrentPulse(kson::Pulse pulse) noexcept {
        currentPulse_ = std::max<kson::Pulse>(0, pulse);
    }

    // --- Note views (read-mostly helpers) ---
    [[nodiscard]] const kson::BTLane<kson::Interval>& btLanes() const { return chart_.note.bt; }
    [[nodiscard]] const kson::FXLane<kson::Interval>& fxLanes() const { return chart_.note.fx; }
    [[nodiscard]] const kson::LaserLane<kson::LaserSection>& laserLanes() const { return chart_.note.laser; }

    // Iteration helpers for timeline rendering
    template<typename Fn>
    void forEachBtNote(Fn&& fn) const;  // (lane, pulse, const Interval&)
    template<typename Fn>
    void forEachFxNote(Fn&& fn) const;
    template<typename Fn>
    void forEachLaserSection(Fn&& fn) const;  // (lane, startPulse, const LaserSection&)

private:
    kson::ChartData chart_{};
    kson::TimingCache timing_{};
    std::filesystem::path path_{};
    kson::Pulse currentPulse_{ 0 };
    bool loaded_{false};
    bool dirty_{false};
};

} // namespace SdvxEditor

// ---- Inline template implementations ----
namespace SdvxEditor
{
	template<typename Fn>
	inline void ChartManager::forEachBtNote(Fn&& fn) const
	{
		for (std::size_t lane = 0; lane < kson::kNumBTLanesSZ; ++lane)
			for (const auto& [pulse, interval] : chart_.note.bt[lane])
				fn(static_cast<int>(lane), pulse, interval);
	}

	template<typename Fn>
	inline void ChartManager::forEachFxNote(Fn&& fn) const
	{
		for (std::size_t lane = 0; lane < kson::kNumFXLanesSZ; ++lane)
			for (const auto& [pulse, interval] : chart_.note.fx[lane])
				fn(static_cast<int>(lane), pulse, interval);
	}

	template<typename Fn>
	inline void ChartManager::forEachLaserSection(Fn&& fn) const
	{
		for (std::size_t lane = 0; lane < kson::kNumLaserLanesSZ; ++lane)
			for (const auto& [pulse, section] : chart_.note.laser[lane])
				fn(static_cast<int>(lane), pulse, section);
	}
}
