#pragma once
#include <DOGEngine.h>
#include "../Core/GameSettings.h"
class SettingsMenu
{
public:
	static void Initialize(
		std::function<void(const DOG::GraphicsSettings&)> setGraphicsSettings,
		std::function<DOG::GraphicsSettings(void)> getGraphicsSettings,
		std::function<Vector2u(void)> getAspectRatio,
		std::function<void(const DOG::AudioSettings&)> setAudioSettings,
		std::function<DOG::AudioSettings(void)> getAudioSettings,
		std::function<void(const GameSettings&)> setGameSettings,
		std::function<GameSettings(void)> getGameSettings
);
	static void SetGraphicsSettings(const DOG::GraphicsSettings& settings);
	static void SetAudioSettings(const DOG::AudioSettings& settings);
	static void SetGameSettings(const GameSettings& settings);
	static bool IsOpen();
	static void Close();
private:

	static void UpdateResolutions();

	static std::function<void(const DOG::GraphicsSettings&)> s_setGraphicsSettings;
	static std::function<DOG::GraphicsSettings(void)> s_getGraphicsSettings;
	static std::function<Vector2u(void)> s_getAspectRatio;

	static std::function<void(const DOG::AudioSettings&)> s_setAudioSettings;
	static std::function<DOG::AudioSettings(void)> s_getAudioSettings;

	static std::function<void(const GameSettings&)> s_setGameSettings;
	static std::function<GameSettings(void)> s_getGameSettings;

	static DOG::GraphicsSettings s_graphicsSettings;
	static DOG::AudioSettings s_audioSettings;
	static GameSettings s_gameSettings;
	static std::vector<Vector2u> s_renderResolution;
	static std::vector<u32> s_renderResolutionHeightPreset;
	static inline u32 s_backgroundID;
	static inline u32 s_backButtonID;
	static inline u32 s_exitToMainMenuButtonID;
	static inline u32 s_exitToDesktopButtonID;

	static inline u32 s_displaySettingsLabelID;
	static inline u32 s_fullscreenLabelID;
	static inline u32 s_fullscreenCheckBoxID;

	static inline u32 s_vsyncLabelID;
	static inline u32 s_vsyncCheckBoxID;
	
	static inline u32 s_graphicsSettingsLabelID;
	static inline u32 s_shadowMappingLabelID;
	static inline u32 s_shadowMappingCheckBoxID;

	static inline u32 s_ssaoLabelID;
	static inline u32 s_ssaoCheckBoxID;

	static inline u32 s_bloomLabelID;
	static inline u32 s_bloomCheckBoxID;

	static inline u32 s_bloomSliderLabelID;
	static inline u32 s_bloomSliderID;

	static inline u32 s_renderResCarouselLabelID;
	static inline u32 s_renderResCarouselID;


	static inline u32 s_audioSettingsLabelID;
	static inline u32 s_audioVolumeSliderLabelID;
	static inline u32 s_audioVolumeSliderID;

	static inline u32 s_inputSettingLabelID;
	static inline u32 s_mouseSensitivitySliderLabelID;
	static inline u32 s_mouseSensitivitySliderID;
	
};