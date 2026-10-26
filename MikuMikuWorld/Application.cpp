#include <Windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include "Application.h"
#include "ApplicationConfiguration.h"
#include "Colors.h"
#include "SdvxEditor/EditHistory.h"
#include "IO.h"
#include "Localization.h"
#include "ResourceManager.h"
#include "Utilities.h"
#include "SdvxEditor/KshExport.h" 
#include "SdvxEditor/ChartManager.h"
#include "SdvxEditor/SDVXTrackRenderer.h"	
#include <filesystem>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
#include "SdvxEditor/SdvxMetaPanel.h"

namespace MikuMikuWorld
{
	std::string Application::version;
	std::string Application::appDir;
	std::string Application::pendingLoadScoreFile;
	WindowState Application::windowState;

	NoteTextures noteTextures{ -1, -1, -1, -1, -1 };

	Application::Application() : initialized{ false }
	{
		appDir = "";
		version = "";
		language = "";
	}

	Result Application::initialize(const std::string& root)
	{
		if (initialized)
			return Result(ResultStatus::Success, "App is already initialized");

		appDir = root;
		version = getVersion();
		language = "";

		config.read(appDir + APP_CONFIG_FILENAME);
		readSettings();

		Result result = initOpenGL();
		if (!result.isOk())
			return result;

		imgui = std::make_unique<ImGuiManager>();
		result = imgui->initialize(window);
		if (!result.isOk())
			return result;

		imgui->setBaseTheme(config.baseTheme);
		imgui->applyAccentColor(config.accentColor);

		loadResources();

		editor = std::make_unique<ScoreEditor>();
		editor->loadPresets(appDir + "library");

		initialized = true;
		return Result::Ok();
	}

	const std::string& Application::getAppDir() { return appDir; }

	std::string Application::getVersion()
	{
		wchar_t filename[1024];
		lstrcpyW(filename, IO::mbToWideStr(std::string(appDir + "MikuMikuWorld.exe")).c_str());

		DWORD verHandle = 0;
		UINT size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD verSize = GetFileVersionInfoSizeW(filename, &verHandle);

		int major = 0, minor = 0, build = 0, rev = 0;
		if (verSize != NULL)
		{
			LPSTR verData = new char[verSize];

			if (GetFileVersionInfoW(filename, verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							major = (verInfo->dwFileVersionMS >> 16) & 0xffff;
							minor = (verInfo->dwFileVersionMS >> 0) & 0xffff;
							rev = (verInfo->dwFileVersionLS >> 16) & 0xffff;
							build = (verInfo->dwFileVersionLS >> 0) & 0xffff;
						}
					}
				}
			}
			delete[] verData;
		}

		return IO::formatString("%d.%d.%d.%d", major, minor, rev, build);
	}

	const std::string& Application::getAppVersion() { return version; }

	void Application::dispose()
	{
		if (initialized)
		{
			editor->uninitialize();
			imgui->shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		initialized = false;
	}

	void Application::readSettings()
	{
		windowState.position = config.windowPos;
		windowState.size = config.windowSize;
		windowState.maximized = config.maximized;
		windowState.vsync = config.vsync;
		UI::accentColors[0] = config.userColor.toImVec4();
	}

	void Application::writeSettings()
	{
		config.maximized = windowState.maximized;
		config.vsync = windowState.vsync;
		config.windowPos = windowState.position;
		config.windowSize = windowState.size;
		config.userColor = Color::fromImVec4(UI::accentColors[0]);

		editor->writeSettings();
		config.write(appDir + APP_CONFIG_FILENAME);
	}

	void Application::appendOpenFile(const std::string& filename)
	{
		pendingOpenFiles.push_back(filename);
		windowState.dragDropHandled = false;
	}

	void Application::handlePendingOpenFiles()
	{
		std::string scoreFile{};
		std::string musicFile{};

		for (auto it = pendingOpenFiles.rbegin(); it != pendingOpenFiles.rend(); ++it)
		{
			std::string extension = IO::File::getFileExtension(*it);
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

			if (extension == SUS_EXTENSION || extension == USC_EXTENSION ||
			    extension == MMWS_EXTENSION || extension == CC_MMWS_EXTENSION || extension == JSON_EXTENSION)
				scoreFile = *it;
			else if (Audio::isSupportedFileFormat(extension))
				musicFile = *it;

			if (!scoreFile.empty() && !musicFile.empty())
				break;
		}

		if (!scoreFile.empty())
		{
			windowState.resetting = true;
			pendingLoadScoreFile = scoreFile;
		}

		if (!musicFile.empty())
			editor->loadMusic(musicFile);

		pendingOpenFiles.clear();
		windowState.dragDropHandled = true;
	}

	void Application::update()
	{
		if (config.language != language)
		{
			std::string locale =
			    config.language == "auto" ? Utilities::getSystemLocale() : config.language;

			// Try to set the selected language and fallback to default (en) on failure
			if (!Localization::setLanguage(locale))
				Localization::setLanguage("en");

			language = config.language;
		}

		float dpiX = 1.0f, dpiY = 1.0f;
		GLFWmonitor* mainMonitor = glfwGetPrimaryMonitor();
		if (mainMonitor)
		{
			glfwGetMonitorContentScale(mainMonitor, &dpiX, &dpiY);
		}

		float dpiScale = (dpiX + dpiY) * 0.5f;
		if (dpiScale != windowState.lastDpiScale)
		{
			imgui->buildFonts(dpiScale);
			windowState.lastDpiScale = dpiScale;
		}

		imgui->begin();

		// Inform ImGui of dpi changes
		ImGui::GetMainViewport()->DpiScale = dpiX;
		UI::updateBtnSizesDpiScaling(dpiScale);

		if (!windowState.dragDropHandled)
			handlePendingOpenFiles();

		imgui->initializeLayout();

		if (config.accentColor != imgui->getAccentColor())
			imgui->applyAccentColor(config.accentColor);

		if (config.userColor != Color::fromImVec4(UI::accentColors[0]) && config.accentColor == 0)
			imgui->applyAccentColor(config.accentColor);

		if (config.baseTheme != imgui->getBaseTheme())
			imgui->setBaseTheme(config.baseTheme);

		if ((windowState.closing || windowState.resetting) && !editor->isUpToDate() &&
		    !unsavedChangesDialog.open)
		{
			unsavedChangesDialog.open = true;
			ImGui::OpenPopup(MODAL_TITLE("unsaved_changes"));
		}

		auto unsavedChangesResult = unsavedChangesDialog.update();

		if (windowState.closing)
		{
			if (!editor->isUpToDate())
			{
				switch (unsavedChangesResult)
				{
				case DialogResult::Yes:
					editor->trySave(editor->getWorkingFilename().data());
					glfwSetWindowShouldClose(window, 1);
					break;

				case DialogResult::No:
					glfwSetWindowShouldClose(window, 1);
					break;

				case DialogResult::Cancel:
					windowState.closing = false;
					break;

				default:
					break;
				}
			}
			else
			{
				glfwSetWindowShouldClose(window, 1);
			}
		}

		if (windowState.resetting)
		{
			if (!editor->isUpToDate())
			{
				switch (unsavedChangesResult)
				{
				case DialogResult::Yes:
					editor->trySave(editor->getWorkingFilename().data());
					break;

				case DialogResult::Cancel:
					windowState.resetting = shouldPickScore = false;
					pendingLoadScoreFile.clear();
					break;

				default:
					break;
				}
			}

			// Already saved or clicked save changes or discard changes
			if (editor->isUpToDate() || (unsavedChangesResult != DialogResult::Cancel &&
			                             unsavedChangesResult != DialogResult::None))
			{
				if (windowState.shouldPickScore)
				{
					editor->open();
					windowState.shouldPickScore = false;
				}
				else if (pendingLoadScoreFile.size())
				{
					editor->loadScore(pendingLoadScoreFile);
					pendingLoadScoreFile.clear();
				}
				else
				{
					editor->create();
				}

				windowState.resetting = false;
			}
		}

		// --- SDVX Editor ---
		enum class SdvxTool { Select, BT, FX, LaserL, LaserR };
		static SdvxEditor::ChartManager sdvxChart{};
		static SdvxEditor::EditHistory sdvxHistory{};
		static SdvxEditor::SDVXTrackRenderer trackRenderer{};
		static SdvxTool sdvxTool = SdvxTool::BT;
		static bool sdvxDragging = false;
		static kson::Pulse sdvxDragStartPulse = 0;
		static int sdvxDragLane = 0;

		ImGui::Begin("SDVX Track", nullptr, ImGuiWindowFlags_MenuBar);
		{
			// ---- Menu bar ----
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New"))
						sdvxChart = SdvxEditor::ChartManager{};

					if (ImGui::MenuItem("Open .kson..."))
					{
						char buf[MAX_PATH] = {};
						OPENFILENAMEA ofn{};
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner   = glfwGetWin32Window(window);
						ofn.lpstrFilter = "KSON Files\0*.kson\0All Files\0*.*\0";
						ofn.lpstrFile   = buf;
						ofn.nMaxFile    = MAX_PATH;
						ofn.Flags       = OFN_FILEMUSTEXIST;
						if (GetOpenFileNameA(&ofn))
							sdvxChart.loadKson(buf);
					}
					if (ImGui::MenuItem("Save .kson..."))
					{
						char buf[MAX_PATH] = {};
						OPENFILENAMEA ofn{};
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner   = glfwGetWin32Window(window);
						ofn.lpstrFilter = "KSON Files\0*.kson\0All Files\0*.*\0";
						ofn.lpstrFile   = buf;
						ofn.nMaxFile    = MAX_PATH;
						ofn.lpstrDefExt = "kson";
						ofn.Flags       = OFN_OVERWRITEPROMPT;
						if (GetSaveFileNameA(&ofn))
							sdvxChart.saveKson(buf);
					}
					if (ImGui::MenuItem("Export .ksh..."))
					{
						char buf[MAX_PATH] = {};
						OPENFILENAMEA ofn{};
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner   = glfwGetWin32Window(window);
						ofn.lpstrFilter = "KSH Files\0*.ksh\0All Files\0*.*\0";
						ofn.lpstrFile   = buf;
						ofn.nMaxFile    = MAX_PATH;
						ofn.lpstrDefExt = "ksh";
						ofn.Flags       = OFN_OVERWRITEPROMPT;
						if (GetSaveFileNameA(&ofn))
							SdvxEditor::exportKsh(sdvxChart, buf);
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			// ---- Toolbar ----
			auto toolBtn = [&](const char* label, SdvxTool t) {
				bool active = (sdvxTool == t);
				if (active) ImGui::PushStyleColor(ImGuiCol_Button,
								ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
				if (ImGui::Button(label)) sdvxTool = t;
				if (active) ImGui::PopStyleColor();
				ImGui::SameLine();
			};

			toolBtn("Select",  SdvxTool::Select);
			toolBtn("BT",      SdvxTool::BT);
			toolBtn("FX",      SdvxTool::FX);
			toolBtn("Laser L", SdvxTool::LaserL);
			toolBtn("Laser R", SdvxTool::LaserR);

			ImGui::NewLine();

			// Tool hint
			switch (sdvxTool)
			{
			case SdvxTool::BT:
				ImGui::TextUnformatted("LMB: place/drag BT  |  RMB: delete  |  Wheel: scroll");
				break;
			case SdvxTool::FX:
				ImGui::TextUnformatted("LMB: place/drag FX (left or right half)  |  RMB: delete");
				break;
			case SdvxTool::LaserL:
				ImGui::TextUnformatted("LMB: add laser L point  |  RMB: delete");
				break;
			case SdvxTool::LaserR:
				ImGui::TextUnformatted("LMB: add laser R point  |  RMB: delete");
				break;
			default:
				ImGui::TextUnformatted("Wheel: scroll");
				break;
			}
			ImGui::Separator();

			// ---- Scroll (outer window hover covers child too) ----
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
			{
				const float wheel = ImGui::GetIO().MouseWheel;
				if (wheel != 0.0f)
				{
					kson::Pulse delta = static_cast<kson::Pulse>(wheel * 120.0f);
					sdvxChart.setCurrentPulse(sdvxChart.currentPulse() - delta);
					// setCurrentPulse already clamps to 0
				}
			}
			// Undo/redo
			const bool ctrl = ImGui::GetIO().KeyCtrl;
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) sdvxHistory.undo(sdvxChart);
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) sdvxHistory.redo(sdvxChart);
			// ---- Highway child window ----
			const float highwayHeight = std::max(200.0f, ImGui::GetContentRegionAvail().y);
			ImGui::BeginChild("##sdvx_highway", ImVec2(0.0f, highwayHeight), true,
							ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			// Capture layout BEFORE render() calls Dummy and moves cursor
			const ImVec2 hwPos  = ImGui::GetCursorScreenPos();
			const ImVec2 hwSize = ImGui::GetContentRegionAvail();

			trackRenderer.render(sdvxChart);   // only draws, no input

			// ---- Input (inside child, after render) ----
			if (ImGui::IsWindowHovered() && hwSize.x > 0 && hwSize.y > 0)
			{
				const auto& style  = trackRenderer.getStyle();
				const ImVec2 mouse = ImGui::GetMousePos();
				const float relX   = mouse.x - hwPos.x;
				const float relY   = mouse.y - hwPos.y;

				// Only act if mouse is inside the highway rect
				if (relX >= 0 && relX < hwSize.x && relY >= 0 && relY < hwSize.y)
				{
					// Convert Y → snapped pulse
					const float cursorScreenY = hwPos.y + style.cursorYNormalized * hwSize.y;
					const float pixelDelta    = cursorScreenY - mouse.y;
					const kson::Pulse rawPulse = sdvxChart.currentPulse()
						+ static_cast<kson::Pulse>(pixelDelta / style.pixelsPerPulse);

					const kson::Pulse snap = std::max<kson::Pulse>(1, style.gridStepMinor);
					const kson::Pulse snapped = ((rawPulse + snap / 2) / snap) * snap;
					const kson::Pulse placePulse = std::max<kson::Pulse>(0, snapped);

					// Convert X → BT column (0-3) and FX side (0=left, 1=right)
					const float unitW  = hwSize.x / 4.0f;
					const int btCol    = std::clamp(static_cast<int>(relX / unitW), 0, 3);
					const int fxSide   = (relX < hwSize.x * 0.5f) ? 0 : 1; // 0=FX-L, 1=FX-R

					// ---- Place / start drag ----
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						sdvxDragging       = true;
						sdvxDragStartPulse = placePulse;
						sdvxDragLane       = (sdvxTool == SdvxTool::FX) ? fxSide : btCol;

						if (sdvxTool == SdvxTool::BT)
						{
							// Place chip immediately on click; drag will extend to hold below
							auto& lane = sdvxChart.chart().note.bt[static_cast<size_t>(btCol)];
							SdvxEditor::EditCommand btCmd{};
							btCmd.type      = SdvxEditor::CommandType::AddBT;
							btCmd.lane      = btCol;
							btCmd.pulse     = placePulse;
							btCmd.noteAfter = kson::Interval{ 0 };
							sdvxHistory.execute(btCmd, sdvxChart);
						}
						else if (sdvxTool == SdvxTool::FX)
						{
							auto& lane = sdvxChart.chart().note.fx[static_cast<size_t>(fxSide)];
							SdvxEditor::EditCommand fxCmd{};
							fxCmd.type      = SdvxEditor::CommandType::AddFX;
							fxCmd.lane      = fxSide;
							fxCmd.pulse     = placePulse;
							fxCmd.noteAfter = kson::Interval{ 0 };
							sdvxHistory.execute(fxCmd, sdvxChart);
						}
						else if (sdvxTool == SdvxTool::LaserL || sdvxTool == SdvxTool::LaserR)
						{
							const int laserLane = (sdvxTool == SdvxTool::LaserL) ? 0 : 1;
							const float normX = std::clamp(relX / hwSize.x, 0.0f, 1.0f);
							auto& laneMap = sdvxChart.chart().note.laser[static_cast<size_t>(laserLane)];

							const kson::Pulse searchTol = kson::kResolution;
							bool addedToExisting = false;

							for (auto& [sectionPulse, section] : laneMap)
							{
								if (std::abs(static_cast<long long>(sectionPulse - placePulse)) < searchTol)
								{
									// Save before state, modify, record command
									SdvxEditor::EditCommand cmd{};
									cmd.type        = SdvxEditor::CommandType::ModifyLaser;
									cmd.lane        = laserLane;
									cmd.pulse       = sectionPulse;
									cmd.laserBefore = std::make_shared<kson::LaserSection>(section);

									const kson::RelPulse relP = placePulse - sectionPulse;
									section.v.insert_or_assign(relP,
										kson::GraphPoint{ kson::GraphValue{ normX, normX } });

									cmd.laserAfter = std::make_shared<kson::LaserSection>(section);
									sdvxHistory.execute(cmd, sdvxChart);
									addedToExisting = true;
									break;
								}
							}

							if (!addedToExisting)
							{
								kson::LaserSection newSection{};
								newSection.v.emplace(kson::RelPulse{ 0 },
									kson::GraphPoint{ kson::GraphValue{ normX, normX } });

								SdvxEditor::EditCommand cmd{};
								cmd.type       = SdvxEditor::CommandType::AddLaser;
								cmd.lane       = laserLane;
								cmd.pulse      = placePulse;
								cmd.laserAfter = std::make_shared<kson::LaserSection>(newSection);
								sdvxHistory.execute(cmd, sdvxChart);
							}
						}
					}

					// ---- Extend hold while dragging ----
					if (sdvxDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						const kson::Pulse dragEnd   = placePulse;
						const kson::Pulse dragStart = sdvxDragStartPulse;

						if (dragEnd > dragStart)
						{
							const kson::RelPulse length = dragEnd - dragStart;

							if (sdvxTool == SdvxTool::BT)
							{
								auto& lane = sdvxChart.chart().note.bt[static_cast<size_t>(sdvxDragLane)];
								auto it = lane.find(dragStart);
								if (it != lane.end())
									it->second.length = length;
							}
							else if (sdvxTool == SdvxTool::FX)
							{
								auto& lane = sdvxChart.chart().note.fx[static_cast<size_t>(sdvxDragLane)];
								auto it = lane.find(dragStart);
								if (it != lane.end())
									it->second.length = length;
							}
						}
					}

					// ---- Delete on right-click ----
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
					{
						const kson::Pulse tol = snap * 2;

						auto eraseNear = [&](auto& laneMap) {
							for (auto it = laneMap.begin(); it != laneMap.end(); ++it)
							{
								const long long diff = static_cast<long long>(it->first)
													- static_cast<long long>(placePulse);
								if (std::abs(diff) < static_cast<long long>(tol))
								{
									laneMap.erase(it);
									return;
								}
							}
						};

						if (sdvxTool == SdvxTool::BT)
							eraseNear(sdvxChart.chart().note.bt[static_cast<size_t>(btCol)]);
						else if (sdvxTool == SdvxTool::FX)
							eraseNear(sdvxChart.chart().note.fx[static_cast<size_t>(fxSide)]);
						else if (sdvxTool == SdvxTool::LaserL)
							eraseNear(sdvxChart.chart().note.laser[0]);
						else if (sdvxTool == SdvxTool::LaserR)
							eraseNear(sdvxChart.chart().note.laser[1]);
					}
				}
			}

			// Stop drag when mouse released
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
				sdvxDragging = false;

			ImGui::EndChild();
			if (ImGui::CollapsingHeader("Chart Properties"))
				SdvxEditor::drawMetaPanel(sdvxChart);
		}
		ImGui::End();

		editor->update();

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		imgui->draw(window);
		glfwSwapBuffers(window);
	}

	void Application::loadResources()
	{
		ResourceManager::loadShader(appDir + "res\\shaders\\basic2d");
		const std::string texturesDir = appDir + "res\\textures\\";
		ResourceManager::loadTexture(texturesDir + "notes1.png",
		                             TextureFilterMode::LinearMipMapLinear,
		                             TextureFilterMode::Linear);
		ResourceManager::loadTexture(texturesDir + "notes2.png");
		ResourceManager::loadTexture(texturesDir + "longNoteLine.png");
		ResourceManager::loadTexture(texturesDir + "touchLine_eff.png");
		ResourceManager::loadTexture(texturesDir + "guideColors.png");

		ResourceManager::loadTexture(texturesDir + "timeline_select.png");
		ResourceManager::loadTexture(texturesDir + "timeline_tap.png");
		ResourceManager::loadTexture(texturesDir + "timeline_hold.png");
		ResourceManager::loadTexture(texturesDir + "timeline_hold_step_normal.png");
		ResourceManager::loadTexture(texturesDir + "timeline_hold_step_hidden.png");
		ResourceManager::loadTexture(texturesDir + "timeline_hold_step_skip.png");
		ResourceManager::loadTexture(texturesDir + "timeline_flick_default.png");
		ResourceManager::loadTexture(texturesDir + "timeline_flick_left.png");
		ResourceManager::loadTexture(texturesDir + "timeline_flick_right.png");
		ResourceManager::loadTexture(texturesDir + "timeline_critical.png");
		ResourceManager::loadTexture(texturesDir + "timeline_trace.png");
		for (auto color : guideColors)
			for (auto fade : fadeTypes)
				ResourceManager::loadTexture(
				    appDir + IO::formatString("res\\textures\\timeline_guide_%s_%s.png", color,
				                              std::string(fade).substr(5).c_str()));
		ResourceManager::loadTexture(texturesDir + "timeline_damage.png");
		ResourceManager::loadTexture(texturesDir + "timeline_bpm.png");
		ResourceManager::loadTexture(texturesDir + "timeline_time_signature.png");
		ResourceManager::loadTexture(texturesDir + "timeline_hi_speed.png");
		// Cache note textures indices
		noteTextures.notes = ResourceManager::getTexture(NOTES_TEX);
		noteTextures.holdPath = ResourceManager::getTexture(HOLD_PATH_TEX);
		noteTextures.touchLine = ResourceManager::getTexture(TOUCH_LINE_TEX);
		noteTextures.ccNotes = ResourceManager::getTexture(CC_NOTES_TEX);
		noteTextures.guideColors = ResourceManager::getTexture(GUIDE_COLORS_TEX);

		Localization::loadLanguages(appDir + "res\\i18n");
	}

	void Application::run()
	{
		HWND hwnd = glfwGetWin32Window(window);

		/*
		    Override the current GLFW/Imgui window procedure and store it in the GLFW window user
		   pointer

		    NOTE: For this to be safe, it should be only called AFTER ImGui is initialized
		    so that the WndProc ImGui is expecting matches with our own WndProc
		*/
		glfwSetWindowUserPointer(window, (void*)::GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
		::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);

		windowState.windowHandle = hwnd;
		windowState.windowTimerId =
		    ::SetTimer(hwnd, reinterpret_cast<UINT_PTR>(&windowState.windowTimerId),
		               USER_TIMER_MINIMUM, nullptr);

		::DragAcceptFiles(hwnd, TRUE);

		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			update();
		}

		editor->savePresets(appDir + "library");
		writeSettings();
	}
}
