// 1. C++ Standard Library First
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

// 2. GLAD (Must be before Windows.h)
#include <glad/glad.h> 

// 3. External Library Headers (libkson)
#include <kson/IO/KsonIO.hpp>
#include <kson/Util/TimingUtils.hpp>

// 4. Project Headers
#include "Application.h"
#include "IO.h"
#include "ChartManager.h"

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

