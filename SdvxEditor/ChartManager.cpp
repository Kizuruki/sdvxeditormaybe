#include "ChartManager.h"

#include <kson/IO/KsonIO.hpp>
#include <kson/Util/TimingUtils.hpp>

namespace SdvxEditor
{
	kson::ErrorType ChartManager::loadKson(const std::filesystem::path& path)
	{
		kson::KsonLoadingDiag diag{};
		chart_ = kson::LoadKsonChartData(path.string(), &diag);

		if (chart_.error != kson::ErrorType::None)
		{
			loaded_ = false;
			return chart_.error;
		}

		path_ = path;
		loaded_ = true;
		dirty_ = false;

		rebuildTimingCache();
		return kson::ErrorType::None;
	}

	kson::ErrorType ChartManager::saveKson(const std::filesystem::path& path) const
	{
		return kson::SaveKsonChartData(path.string(), chart_);
	}

	void ChartManager::rebuildTimingCache()
	{
		timing_ = kson::CreateTimingCache(chart_.beat);
	}
} // namespace SdvxEditor

