#pragma once

#include <emuframework/Option.hh>
#include <emuframework/EmuSystem.hh>
#include <vbam/gba/GBA.h>

namespace IG
{
class ApplicationContext;
}

struct GBASys;

namespace EmuEx
{

enum class RtcMode : uint8_t {AUTO, OFF, ON};

enum
{
	CFGKEY_RTC_EMULATION = 256, CFGKEY_SAVE_TYPE_OVERRIDE = 257,
	CFGKEY_PCM_VOLUME = 258, CFGKEY_GB_APU_VOLUME = 259,
	CFGKEY_SOUND_FILTERING = 260, CFGKEY_SOUND_INTERPOLATION = 261,
};

void readCheatFile(EmuSystem &);
void setSaveType(int type, int size);
const char *saveTypeStr(int type, int size);
bool saveMemoryHasContent();
int soundVolumeAsInt(GBASys &, bool gbVol);
int soundFilteringAsInt(GBASys &);

constexpr uint32_t packSaveTypeOverride(int type, int size = 0) { return (type << 24) | (size & 0xFFFFFF); }
constexpr std::pair<int, int> unpackSaveTypeOverride(uint32_t val) { return {val >> 24, val & 0xFFFFFF}; }

constexpr bool optionSaveTypeOverrideIsValid(uint32_t val)
{
	auto [type, size] = unpackSaveTypeOverride(val);
	return type >= GBA_SAVE_AUTO && type <= GBA_SAVE_NONE;
}

class GbaSystem final: public EmuSystem
{
public:
	Byte1Option optionRtcEmulation{CFGKEY_RTC_EMULATION, to_underlying(RtcMode::AUTO), 0, optionIsValidWithMax<2>};
	Byte4Option optionSaveTypeOverride{CFGKEY_SAVE_TYPE_OVERRIDE, GBA_SAVE_AUTO, 0, optionSaveTypeOverrideIsValid};
	int detectedSaveSize{};
	uint8_t detectedSaveType{};
	bool detectedRtcGame{};

	GbaSystem(ApplicationContext ctx):
		EmuSystem{ctx} {}
	void setGameSpecificSettings(GBASys &gba, int romSize);
	void setRTC(RtcMode mode);
	std::pair<int, int> saveTypeOverride() { return unpackSaveTypeOverride(optionSaveTypeOverride.val); }
	void setSaveTypeOverride(int type, int size) { optionSaveTypeOverride = packSaveTypeOverride(type, size); };

	// required API functions
	void loadContent(IO &, EmuSystemCreateParams, OnLoadProgressDelegate);
	[[gnu::hot]] void runFrame(EmuSystemTaskContext task, EmuVideo *video, EmuAudio *audio);
	FS::FileString stateFilename(int slot, std::string_view name) const;
	void loadState(EmuApp &, CStringView uri);
	void saveState(CStringView path);
	bool readConfig(ConfigType, IO &io, unsigned key, size_t readSize);
	void writeConfig(ConfigType, IO &);
	void reset(EmuApp &, ResetMode mode);
	void clearInputBuffers(EmuInputView &view);
	void handleInputAction(EmuApp *, InputAction);
	unsigned translateInputAction(unsigned input, bool &turbo);
	VController::Map vControllerMap(int player);
	void configAudioRate(FloatSeconds frameTime, int rate);

	// optional API functions
	bool resetSessionOptions(EmuApp &);
	void onFlushBackupMemory(BackupMemoryDirtyFlags);
	void closeSystem();
	bool onVideoRenderFormatChange(EmuVideo &, IG::PixelFormat);
	void renderFramebuffer(EmuVideo &);

private:
	void applyGamePatches(uint8_t *rom, int &romSize);
};

using MainSystem = GbaSystem;

}

void CPULoop(GBASys &, EmuEx::EmuSystemTaskContext, EmuEx::EmuVideo *, EmuEx::EmuAudio *);
void CPUCleanUp();
bool CPUReadBatteryFile(IG::ApplicationContext, GBASys &gba, const char *);
bool CPUWriteBatteryFile(IG::ApplicationContext, GBASys &gba, const char *);
bool CPUReadState(IG::ApplicationContext, GBASys &gba, const char *);
bool CPUWriteState(IG::ApplicationContext, GBASys &gba, const char *);
