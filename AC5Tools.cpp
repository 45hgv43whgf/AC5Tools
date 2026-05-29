#include <windows.h>
#include <d3d11.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <dxgi.h>
#include <wincrypt.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cfloat>
#include <cstring>
#include <vector>

#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "advapi32.lib")

extern "C" IMAGE_DOS_HEADER __ImageBase;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace {

constexpr const char* kToolName = "AC5Tools";
constexpr const char* kToolVersion = "v0.1.0";
constexpr const char* kToolTitle = "AC5Tools v0.1.0";
constexpr const char* kSupportedGameExe = "ACC.exe";
constexpr unsigned long long kSupportedGameExeSize = 67873496ull;
constexpr const char* kSupportedGameExeSha256 =
    "E870D89433297F978DFEB9903F37C0BB1553F35D2E7D4C7847E8C40AB6B77A84";
constexpr int kMenuHotkeyCapture = -2;

constexpr std::uint8_t kBhvAssassinPattern[] = {
    0x48, 0x8D, 0x8F, 0xA0, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x52,
};
constexpr std::uint8_t kOriginalBhvAssassinBytes[] = {
    0x48, 0x8D, 0x8F, 0xA0, 0x02, 0x00, 0x00,
};
constexpr std::uint8_t kFallDamagePattern[] = {
    0xF3, 0x0F, 0x11, 0x44, 0x24, 0x28, 0x49, 0x8B, 0xD9,
    0x4C, 0x8B, 0xD1, 0x44, 0x88, 0x44, 0x24, 0x30,
};
constexpr std::uint8_t kOriginalFallDamageBytes[] = {
    0xF3, 0x0F, 0x11, 0x44, 0x24, 0x28, 0x49, 0x8B, 0xD9,
    0x4C, 0x8B, 0xD1, 0x44, 0x88, 0x44, 0x24, 0x30,
};
constexpr std::uint8_t kNoReloadPattern[] = {
    0x8B, 0x50, 0x1C, 0x8B, 0x48, 0x10,
};
constexpr std::uint8_t kOriginalNoReloadBytes[] = {
    0x8B, 0x50, 0x1C, 0x8B, 0x48, 0x10,
};
constexpr std::uint8_t kLockConsumables1Pattern[] = {
    0x49, 0x8B, 0x08, 0x8B, 0xFA, 0x48, 0x63, 0x41, 0x0C,
    0x48, 0xC1, 0xE0, 0x20, 0x48, 0xC1, 0xF8, 0x3F,
};
constexpr std::uint8_t kOriginalLockConsumables1Bytes[] = {
    0x49, 0x8B, 0x08, 0x8B, 0xFA, 0x48, 0x63, 0x41, 0x0C,
    0x48, 0xC1, 0xE0, 0x20, 0x48, 0xC1, 0xF8, 0x3F,
};
constexpr std::uint8_t kLockConsumables2Pattern[] = {
    0x8B, 0xFA, 0x48, 0x8D, 0x54, 0x24, 0x38, 0x48,
    0x8B, 0xCB, 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00,
};
constexpr std::uint8_t kOriginalLockConsumables2Bytes[] = {
    0x8B, 0xFA, 0x48, 0x8D, 0x54, 0x24, 0x38, 0x48,
    0x8B, 0xCB, 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00,
};
constexpr std::uint8_t kInventorySuppliesPattern[] = {
    0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x4C, 0x8B, 0x49, 0x08,
    0x0F, 0xB7, 0x41, 0x12, 0x8B, 0xFA, 0xC1, 0xE0, 0x03, 0x44, 0x8B, 0xD0,
};
constexpr std::size_t kInventorySuppliesPatchOffset = 6;
constexpr std::uint8_t kOriginalInventorySuppliesBytes[] = {
    0x4C, 0x8B, 0x49, 0x08, 0x0F, 0xB7, 0x41, 0x12,
    0x8B, 0xFA, 0xC1, 0xE0, 0x03, 0x44, 0x8B, 0xD0,
};
constexpr std::uint8_t kInventoryUpdatePattern[] = {
    0x41, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x48, 0x8B,
    0xCB, 0x44, 0x89, 0x5C, 0x24, 0x30,
};
constexpr std::uint8_t kOriginalInventoryUpdateBytes[] = {
    0x41, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x48, 0x8B,
    0xCB, 0x44, 0x89, 0x5C, 0x24, 0x30,
};
constexpr std::uint8_t kInventoryCargoPattern[] = {
    0x41, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x48, 0x8B,
    0xCF, 0x44, 0x89, 0x5C, 0x24, 0x30,
};
constexpr std::uint8_t kOriginalInventoryCargoBytes[] = {
    0x41, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x48, 0x8B,
    0xCF, 0x44, 0x89, 0x5C, 0x24, 0x30,
};
constexpr std::uint8_t kGclShipPattern[] = {
    0x48, 0x8B, 0x00, 0xFF, 0x90, 0x00, 0x01, 0x00,
    0x00, 0x3B, 0x43, 0x14, 0x0F, 0x94, 0xC0,
};
constexpr std::uint8_t kOriginalGclShipBytes[] = {
    0x48, 0x8B, 0x00, 0xFF, 0x90, 0x00, 0x01, 0x00,
    0x00, 0x3B, 0x43, 0x14, 0x0F, 0x94, 0xC0,
};
constexpr std::uint8_t kDebugContextPattern[] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48,
    0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48,
    0x89, 0x78, 0x20, 0x41, 0x54, 0x41, 0x55, 0x41,
    0x56, 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00,
    0x4C, 0x8B,
};
constexpr std::uint8_t kOriginalDebugContextBytes[] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08,
};
constexpr std::uint8_t kMissionTimer1Pattern[] = {
    0x48, 0x8B, 0x83, 0x88, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x08, 0x48, 0x8B,
};
constexpr std::uint8_t kOriginalMissionTimer1Bytes[] = {
    0x48, 0x8B, 0x83, 0x88, 0x01, 0x00, 0x00,
};
constexpr std::uint8_t kMissionTimer2Pattern[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48,
    0x83, 0xEC, 0x20, 0x48, 0x83, 0xB9, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x48,
};
constexpr std::uint8_t kOriginalMissionTimer2Bytes[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48,
    0x83, 0xEC, 0x20,
};
constexpr std::uint8_t kMissionTimer3Pattern[] = {
    0x48, 0x8B, 0x46, 0x50, 0x48, 0x8B, 0x08, 0x48, 0x8B, 0x46, 0x38, 0x45,
};
constexpr std::uint8_t kOriginalMissionTimer3Bytes[] = {
    0x48, 0x8B, 0x46, 0x50, 0x48, 0x8B, 0x08,
};
constexpr std::uint8_t kUnlimitedResourcesPattern[] = {
    0x44, 0x2B, 0xDF, 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00,
};
constexpr std::uint8_t kOriginalUnlimitedResourcesBytes[] = {
    0x44, 0x2B, 0xDF,
};
constexpr std::uint8_t kUnlimitedSellingPattern[] = {
    0x44, 0x2B, 0xDB, 0x48, 0x8D, 0x54, 0x24, 0x30,
};
constexpr std::uint8_t kOriginalUnlimitedSellingBytes[] = {
    0x44, 0x2B, 0xDB,
};
constexpr std::uint8_t kHarpoonGodmode1Pattern[] = {
    0xF3, 0x0F, 0x5C, 0xC1, 0x0F, 0x2F, 0xC6, 0xF3, 0x0F, 0x11, 0x87,
};
constexpr std::uint8_t kHarpoonGodmode2Pattern[] = {
    0xF3, 0x0F, 0x5C, 0xC6, 0x0F, 0x28, 0xB4, 0x24,
};
constexpr std::uint8_t kOriginalHarpoonGodmodeBytes[] = {
    0xF3, 0x0F, 0x5C, 0xC1,
};
constexpr std::uint8_t kOriginalHarpoonGodmode2Bytes[] = {
    0xF3, 0x0F, 0x5C, 0xC6,
};
constexpr std::uint8_t kNop3Bytes[] = {
    0x90, 0x90, 0x90,
};
constexpr std::uint8_t kNop4Bytes[] = {
    0x90, 0x90, 0x90, 0x90,
};
constexpr std::uint8_t kGlobalUnlockReadPattern[] = {
    0x0F, 0xB6, 0x41, 0x45, 0x88, 0x41, 0x40, 0x8B, 0x41, 0x34, 0x89, 0x41, 0x14,
};
constexpr std::uint8_t kOriginalGlobalUnlockReadBytes[] = {
    0x0F, 0xB6, 0x41, 0x45,
};
constexpr std::uint8_t kEnabledGlobalUnlockReadBytes[] = {
    0x31, 0xC0, 0x90, 0x90,
};
constexpr std::uint8_t kGlobalUnlockWritePattern[] = {
    0x57, 0x48, 0x83, 0xEC, 0x20, 0x0F, 0xB6, 0xEA, 0x48, 0x8B, 0xD9, 0x88, 0x51, 0x40,
};
constexpr std::size_t kGlobalUnlockWritePatchOffset = 8;
constexpr std::uint8_t kOriginalGlobalUnlockWriteBytes[] = {
    0x48, 0x8B, 0xD9, 0x88, 0x51, 0x40,
};
constexpr std::uint8_t kGlobalUnlockReturnPattern[] = {
    0x5F, 0xC3, 0x48, 0x8B, 0x03, 0x48, 0xB9, 0x2A, 0xF4, 0x57, 0xD0, 0x15, 0x00, 0x00, 0x00,
};
constexpr std::ptrdiff_t kGlobalUnlockReturnPatchBackOffset = 0xB;
constexpr std::uint8_t kOriginalGlobalUnlockReturnBytes[] = {
    0xB0, 0x01,
};
constexpr std::uint8_t kEnabledGlobalUnlockReturnBytes[] = {
    0x30, 0xC0,
};
constexpr std::uint8_t kAnimusHackPattern[] = {
    0x0F, 0xB6, 0x8B, 0xA0, 0x03, 0x00, 0x00,
    0x84, 0xC0, 0x0F, 0x94, 0xC0, 0x84, 0xC9,
};
constexpr std::uint8_t kOriginalAnimusHackBytes[] = {
    0x0F, 0xB6, 0x8B, 0xA0, 0x03, 0x00, 0x00,
    0x84, 0xC0, 0x0F, 0x94, 0xC0, 0x84, 0xC9,
};
constexpr std::uint8_t kAnimusChallengeUnlockPattern[] = {
    0x45, 0x8B, 0x49, 0x24, 0x45, 0x85, 0xD2,
};
constexpr std::ptrdiff_t kAnimusChallengeUnlockPatchOffset = 3;
constexpr std::uint8_t kOriginalAnimusChallengeUnlockBytes[] = {
    0x24,
};
constexpr std::uint8_t kEnabledAnimusChallengeUnlockBytes[] = {
    0x28,
};
constexpr std::uint8_t kCompleteAllChallengesPattern[] = {
    0x8B, 0x45, 0x24, 0x66, 0x0F, 0xEF, 0xC0,
};
constexpr std::uint8_t kOriginalCompleteAllChallengesBytes[] = {
    0x8B, 0x45, 0x24, 0x66, 0x0F, 0xEF, 0xC0,
};
constexpr std::uint8_t kPlayerSuperSpeedPattern[] = {
    0xF3, 0x0F, 0x59, 0x99, 0x60, 0x01, 0x00, 0x00, 0xEB,
};
constexpr std::uint8_t kOriginalPlayerSuperSpeedBytes[] = {
    0xF3, 0x0F, 0x59, 0x99, 0x60, 0x01, 0x00, 0x00,
};
constexpr std::uint8_t kPlayerSuperJumpPattern[] = {
    0x48, 0x8B, 0x03,
    0xF3, 0x0F, 0x10, 0x35, 0x00, 0x00, 0x00, 0x00,
    0xF3, 0x44, 0x0F, 0x10, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x8B, 0xCB,
    0xFF, 0x90, 0x80, 0x02, 0x00, 0x00,
};
constexpr const char* kPlayerSuperJumpPatternMask = "xxxxxxx????xxxxx????xxxxxxxxx";
constexpr std::size_t kPlayerSuperJumpPatchOffset = 3;
constexpr std::size_t kOriginalPlayerSuperJumpSize = 17;
constexpr std::uintptr_t kNativeGhostRootRva = 0x9DCD0;
constexpr std::uintptr_t kNativeGhostPlayerRva = 0x2892D0;
constexpr std::uintptr_t kNativeGhostResolveRva = 0xC3BE0;
constexpr std::uintptr_t kNativeGhostStateObjectRva = 0xD85E0;
constexpr std::uintptr_t kNativeGhostStateValueRva = 0x1A5590;
constexpr std::uintptr_t kNativeGhostEnableRva = 0x12446A0;
constexpr std::uintptr_t kNativeGhostDisableRva = 0x1244730;
constexpr std::uintptr_t kNativeGhostSpeedTablePtrRva = 0x3315FC8;
constexpr std::uintptr_t kDebugNukeYourselfOffset = 0xA51;
constexpr std::uintptr_t kPlayerHealthValueOffset = 0xB0;
constexpr std::uintptr_t kPlayerGodmodeFlagOffset = 0xB6;
constexpr ULONGLONG kPlayerGodmodeBypassMs = 1000;
constexpr std::uint8_t kNativeGhostRootBytes[] = {
    0x48, 0x8B, 0x05, 0xF1, 0xC2, 0x1F, 0x03, 0x48,
    0x85, 0xC0, 0x74, 0x08,
};
constexpr std::uint8_t kNativeGhostResolveBytes[] = {
    0x48, 0x8B, 0xD1, 0x48, 0x81, 0xC1, 0x2A, 0x01,
    0x00, 0x00, 0xE9,
};
constexpr std::uint8_t kNativeGhostPlayerBytes[] = {
    0x48, 0x8B, 0x81, 0x10, 0x0D, 0x00, 0x00,
    0x48, 0x8B, 0x48, 0x08, 0xE9,
};
constexpr std::uint8_t kNativeGhostStateObjectBytes[] = {
    0x48, 0x85, 0xC9, 0x74, 0x11, 0x48, 0x8B, 0x81,
    0x30, 0x01, 0x00, 0x00,
};
constexpr std::uint8_t kNativeGhostStateValueBytes[] = {
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x63, 0xC2,
    0x48, 0x6B, 0xC0, 0x58,
};
constexpr std::uint8_t kNativeGhostEnableBytes[] = {
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
    0x59, 0x40, 0x84, 0xD2,
};
constexpr std::uint8_t kNativeGhostDisableBytes[] = {
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x49, 0x40,
    0x48, 0x85, 0xC9, 0x74,
};

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using GetDeviceStateFn = HRESULT(__stdcall*)(IDirectInputDevice8A*, DWORD, LPVOID);
using GetDeviceDataFn = HRESULT(__stdcall*)(IDirectInputDevice8A*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyboardStateFn = BOOL(WINAPI*)(PBYTE);

enum class LogSeverity {
    Info,
    Warning,
    Error,
};

LONG g_mainStarted = 0;
char g_moduleDir[MAX_PATH]{};
char g_gameExePath[MAX_PATH]{};
char g_gameTimestampText[64] = "unknown";
char g_gameExeSha256[65] = "unknown";
unsigned long long g_gameExeSize = 0;
bool g_consoleLoggingEnabled = false;
bool g_fileLoggingEnabled = false;
bool g_consoleReady = false;
FILE* g_consoleOut = nullptr;
HANDLE g_consoleOutput = nullptr;
WORD g_consoleDefaultAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
HANDLE g_consoleMutex = nullptr;
char g_pendingConsoleLines[96][768]{};
LogSeverity g_pendingConsoleSeverities[96]{};
int g_pendingConsoleLineCount = 0;
int g_pendingConsoleLineStart = 0;

bool g_menuOpen = false;
bool g_imguiReady = false;
bool g_lockMouseToWindow = false;
bool g_disableMouseInputWhenUiOpen = false;
bool g_disableKeyboardInputWhenUiOpen = false;
bool g_disableHotkeysWhileUiOpen = false;
bool g_inputCaptured = false;
int g_menuHotkey = 'B';
int g_hotkeyCaptureAction = -1;
int g_suppressedHotkeyVk = 0;
ImVec2 g_menuPos(60.0f, 40.0f);
ImVec2 g_menuSize(680.0f, 420.0f);
bool g_menuPosDirty = false;
bool g_menuSizeDirty = false;

HWND g_gameWindow = nullptr;
WNDPROC g_originalWndProc = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11RenderTargetView* g_renderTarget = nullptr;
PresentFn g_originalPresent = nullptr;
ResizeBuffersFn g_originalResizeBuffers = nullptr;

GetDeviceStateFn g_originalGetDeviceStateMouse = nullptr;
GetDeviceStateFn g_originalGetDeviceStateKeyboard = nullptr;
GetDeviceDataFn g_originalGetDeviceDataMouse = nullptr;
GetDeviceDataFn g_originalGetDeviceDataKeyboard = nullptr;
void* g_mouseGetDeviceStateTarget = nullptr;
void* g_keyboardGetDeviceStateTarget = nullptr;
void* g_mouseGetDeviceDataTarget = nullptr;
void* g_keyboardGetDeviceDataTarget = nullptr;
GetAsyncKeyStateFn g_originalGetAsyncKeyState = nullptr;
GetKeyStateFn g_originalGetKeyState = nullptr;
GetKeyboardStateFn g_originalGetKeyboardState = nullptr;

struct ToggleAction {
    const char* id;
    const char* label;
    bool* value;
    bool ready;
    int hotkey;
};

bool g_shipGodmode = false;
bool g_noCannonCooldown = false;
bool g_allyGodmode = false;
bool g_shipStealthMode = false;
bool g_playerGodmode = false;
bool g_stealthMode = false;
bool g_noReload = false;
bool g_noFallDamage = false;
bool g_lockConsumables = false;
bool g_unlimitedResources = false;
bool g_unlimitedSelling = false;
bool g_harpoonGodmode = false;
bool g_freezeMissionTimer = false;
bool g_noclip = false;
bool g_playerSuperSpeed = false;
bool g_playerSuperJump = false;
bool g_globalUnlocksInstalled = false;
bool g_animusHackInstalled = false;
float g_playerSuperSpeedMultiplier = 2.75f;
float g_playerSuperJumpDistance = 8.0f;
float g_playerSuperJumpHeight = -3.0f;
float g_noclipSpeed = 1.0f;
float g_noclipBoostSpeed = 5.0f;

enum ActionIndex {
    kActionShipGodmode = 0,
    kActionNoCannonCooldown,
    kActionAllyGodmode,
    kActionShipStealthMode,
    kActionPlayerGodmode,
    kActionDesynchronizeYourself,
    kActionStealthMode,
    kActionNoReload,
    kActionNoFallDamage,
    kActionLockConsumables,
    kActionUnlimitedResources,
    kActionUnlimitedSelling,
    kActionHarpoonGodmode,
    kActionFreezeMissionTimer,
    kActionPlayerSuperSpeed,
    kActionPlayerSuperJump,
    kActionNoclip,
};

ToggleAction g_actions[] = {
    {"ShipGodmode", "Ship Godmode", &g_shipGodmode, false, 0},
    {"NoCannonCooldown", "Instant Reload", &g_noCannonCooldown, false, 0},
    {"AllyGodmode", "Ally Godmode", &g_allyGodmode, false, 0},
    {"ShipStealthMode", "Ship Stealth Mode", &g_shipStealthMode, false, 0},
    {"PlayerGodmode", "Player Godmode", &g_playerGodmode, false, 0},
    {"DesynchronizeYourself", "Desynchronize Yourself", nullptr, false, 0},
    {"StealthMode", "Stealth Mode", &g_stealthMode, false, 0},
    {"NoReload", "No Reload", &g_noReload, false, 0},
    {"NoFallDamage", "No Fall Damage", &g_noFallDamage, false, 0},
    {"LockConsumables", "Lock Consumables", &g_lockConsumables, false, 0},
    {"UnlimitedResources", "Unlimited Resources", &g_unlimitedResources, false, 0},
    {"UnlimitedSelling", "Unlimited Selling", &g_unlimitedSelling, false, 0},
    {"HarpoonGodmode", "Harpoon Godmode", &g_harpoonGodmode, false, 0},
    {"FreezeMissionTimer", "Freeze Mission Timer", &g_freezeMissionTimer, false, 0},
    {"PlayerSuperSpeed", "Player Super Speed", &g_playerSuperSpeed, false, 0},
    {"PlayerSuperJump", "Player Super Jump", &g_playerSuperJump, false, 0},
    {"Noclip", "Noclip", &g_noclip, false, 0},
};
constexpr int kActionCount = sizeof(g_actions) / sizeof(g_actions[0]);

enum class UnlockCategory {
    Swords,
    Pistols,
    Outfits,
    ShipAppearance,
    Blueprints,
    Extras,
};

enum class UnlockMode {
    Normal,
    Collect,
    UplayPack,
    LegacyContent,
};

enum class InventoryCategory {
    Player,
    Ship,
    Cargo,
};

enum class InventoryEditMode {
    Set,
    Add,
};

struct InventoryEditEntry {
    const char* name;
    InventoryCategory category;
    int id;
    int maxValue;
    bool queued;
    InventoryEditMode mode;
    int amount;
    int lastApplied;
};

InventoryEditEntry g_inventoryEntries[] = {
    {"Money", InventoryCategory::Player, 0x01, 9999999, false, InventoryEditMode::Set, 1000, 0},
    {"Smoke Bombs", InventoryCategory::Player, 0x05, 99, false, InventoryEditMode::Set, 10, 0},
    {"Bullets", InventoryCategory::Player, 0x0B, 99, false, InventoryEditMode::Set, 10, 0},
    {"Sleep Darts", InventoryCategory::Player, 0x0D, 99, false, InventoryEditMode::Set, 10, 0},
    {"Berserk Darts", InventoryCategory::Player, 0x0E, 99, false, InventoryEditMode::Set, 10, 0},
    {"Firecracker Darts", InventoryCategory::Player, 0x0F, 99, false, InventoryEditMode::Set, 10, 0},
    {"Shrapnel Grenades", InventoryCategory::Player, 0x10, 99, false, InventoryEditMode::Set, 10, 0},
    {"Sleep Grenades", InventoryCategory::Player, 0x11, 99, false, InventoryEditMode::Set, 10, 0},
    {"Berserk Grenades", InventoryCategory::Player, 0x12, 99, false, InventoryEditMode::Set, 10, 0},
    {"Rope Darts", InventoryCategory::Player, 0x28, 99, false, InventoryEditMode::Set, 10, 0},
    {"Throwing Knives", InventoryCategory::Player, 0x08, 99, false, InventoryEditMode::Set, 10, 0},
    {"Crew", InventoryCategory::Ship, 0x42, 99, false, InventoryEditMode::Set, 50, 0},
    {"Fire Barrels", InventoryCategory::Ship, 0x3B, 99, false, InventoryEditMode::Set, 10, 0},
    {"Heavy Shot", InventoryCategory::Ship, 0x38, 99, false, InventoryEditMode::Set, 10, 0},
    {"Mortar", InventoryCategory::Ship, 0x39, 99, false, InventoryEditMode::Set, 10, 0},
    {"Harpoon", InventoryCategory::Ship, 0x4B, 99, false, InventoryEditMode::Set, 10, 0},
    {"Metal", InventoryCategory::Cargo, 0x33, 9999, false, InventoryEditMode::Set, 50, 0},
    {"Wood", InventoryCategory::Cargo, 0x32, 9999, false, InventoryEditMode::Set, 50, 0},
    {"Stone", InventoryCategory::Cargo, 0x35, 9999, false, InventoryEditMode::Set, 50, 0},
    {"Cloth", InventoryCategory::Cargo, 0x34, 9999, false, InventoryEditMode::Set, 50, 0},
    {"Tobacco", InventoryCategory::Cargo, 0x36, 9999, false, InventoryEditMode::Set, 50, 0},
};
constexpr int kInventoryEntryCount = sizeof(g_inventoryEntries) / sizeof(g_inventoryEntries[0]);

struct UnlockEntry {
    const char* name;
    UnlockCategory category;
    UnlockMode mode;
    std::uint8_t item[8];
    std::uint8_t required[8];
    bool enabled;
    bool found;
    bool patched;
};

UnlockEntry g_unlockEntries[] = {
    {"Government sword", UnlockCategory::Swords, UnlockMode::Normal, {0x3E, 0xC0, 0x6E, 0xCC, 0x20, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Hunting sabre", UnlockCategory::Swords, UnlockMode::Normal, {0xD4, 0x07, 0xBE, 0x85, 0x26, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Admiral's lion sword", UnlockCategory::Swords, UnlockMode::Normal, {0x5E, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Spanish sword", UnlockCategory::Swords, UnlockMode::Normal, {0x12, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Altair's sword", UnlockCategory::Swords, UnlockMode::Collect, {0x53, 0x3C, 0x1C, 0xDC, 0x1D, 0x00, 0x00, 0x00}, {0x15, 0x32, 0x7C, 0xE2, 0x1A, 0x00, 0x00, 0x00}, false, false, false},
    {"Admiral's lion pistol", UnlockCategory::Pistols, UnlockMode::Normal, {0xD7, 0xFE, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Indian flintlock", UnlockCategory::Pistols, UnlockMode::Normal, {0xA9, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Scottish flintlock", UnlockCategory::Pistols, UnlockMode::Normal, {0xC1, 0x36, 0xFB, 0x2F, 0x20, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"English percussion flintlock", UnlockCategory::Pistols, UnlockMode::Collect, {0x54, 0x3C, 0x1C, 0xDC, 0x1D, 0x00, 0x00, 0x00}, {0xAF, 0xCD, 0xA4, 0x44, 0x1B, 0x00, 0x00, 0x00}, false, false, false},
    {"Native outfit", UnlockCategory::Outfits, UnlockMode::Collect, {0xE7, 0x26, 0x8E, 0xC1, 0x31, 0x00, 0x00, 0x00}, {0x00, 0x7F, 0xA3, 0x49, 0x1B, 0x00, 0x00, 0x00}, false, false, false},
    {"Templar 11th century outfit", UnlockCategory::Outfits, UnlockMode::Collect, {0xFC, 0x15, 0x75, 0x4F, 0x34, 0x00, 0x00, 0x00}, {0x7B, 0x99, 0xC5, 0x63, 0x1C, 0x00, 0x00, 0x00}, false, false, false},
    {"Templar enforcer outfit", UnlockCategory::Outfits, UnlockMode::Normal, {0x81, 0xCE, 0xDB, 0x4D, 0x1F, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Viking outfit and sword", UnlockCategory::Outfits, UnlockMode::Collect, {0xC6, 0xBB, 0xA9, 0x99, 0x21, 0x00, 0x00, 0x00}, {0x35, 0x09, 0xB6, 0x6F, 0x20, 0x00, 0x00, 0x00}, false, false, false},
    {"Templar master and Kraken set", UnlockCategory::Outfits, UnlockMode::Normal, {0x8C, 0xCE, 0xDB, 0x4D, 0x1F, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Admiral outfit", UnlockCategory::Outfits, UnlockMode::Normal, {0xB1, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Sir James Gunn armour and sword", UnlockCategory::Outfits, UnlockMode::Collect, {0x63, 0x76, 0xB8, 0xFE, 0x23, 0x00, 0x00, 0x00}, {0xE1, 0x71, 0x9A, 0x4A, 0x1B, 0x00, 0x00, 0x00}, false, false, false},
    {"British lion sails", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0x22, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Gilded French sails", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0x87, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Aveline figurehead", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0x9B, 0xFA, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"British lion figurehead", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0x6E, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Golden African wheel", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0x4E, 0xFF, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"British lion wheel", UnlockCategory::ShipAppearance, UnlockMode::Normal, {0xF0, 0xFE, 0x1B, 0x16, 0x1A, 0x00, 0x00, 0x00}, {}, false, false, false},
    {"Elite Hull", UnlockCategory::Blueprints, UnlockMode::Collect, {0x5F, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xE2, 0x45, 0xC8, 0x73, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Ultimate broadside cannons", UnlockCategory::Blueprints, UnlockMode::Collect, {0x60, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x61, 0xD4, 0x41, 0x72, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Heavy Shot", UnlockCategory::Blueprints, UnlockMode::Collect, {0x1F, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xC9, 0x4B, 0xC8, 0x73, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Mortar", UnlockCategory::Blueprints, UnlockMode::Collect, {0x8C, 0x34, 0x9F, 0x5F, 0x07, 0x00, 0x00, 0x00}, {0x94, 0x34, 0x9F, 0x5F, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Burning Oil", UnlockCategory::Blueprints, UnlockMode::Collect, {0x1D, 0x1C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x93, 0xFF, 0x2C, 0x7F, 0x10, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Icebreaker Ram", UnlockCategory::Blueprints, UnlockMode::Collect, {0x1C, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x1E, 0x4D, 0xC8, 0x73, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Ultimate Round Shot", UnlockCategory::Blueprints, UnlockMode::Collect, {0x64, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xA5, 0x4A, 0xC8, 0x73, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Puckle Gun", UnlockCategory::Blueprints, UnlockMode::Collect, {0x66, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x4C, 0x4E, 0xC8, 0x73, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Explosive Shot", UnlockCategory::Blueprints, UnlockMode::Collect, {0x62, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x3B, 0xB4, 0x8C, 0x99, 0x08, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Puckle Gun Cylinder", UnlockCategory::Blueprints, UnlockMode::Collect, {0x22, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x0C, 0xD0, 0x2B, 0x48, 0x28, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Heavy Shot Storage", UnlockCategory::Blueprints, UnlockMode::Collect, {0x23, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xDB, 0x90, 0xCA, 0xF3, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Mortar Storage", UnlockCategory::Blueprints, UnlockMode::Collect, {0x20, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xC7, 0x90, 0xCA, 0xF3, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Elite Burning Oil Storage", UnlockCategory::Blueprints, UnlockMode::Collect, {0x1E, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xE7, 0x90, 0xCA, 0xF3, 0x07, 0x00, 0x00, 0x00}, false, false, false},
    {"Blackbeard sails", UnlockCategory::Blueprints, UnlockMode::Collect, {0x63, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xED, 0xF1, 0x34, 0x7A, 0x08, 0x00, 0x00, 0x00}, false, false, false},
    {"Aquila sails", UnlockCategory::Blueprints, UnlockMode::Collect, {0x69, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0x06, 0xF2, 0x34, 0x7A, 0x08, 0x00, 0x00, 0x00}, false, false, false},
    {"Blackbeard figurehead", UnlockCategory::Blueprints, UnlockMode::Collect, {0x68, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xCB, 0xEC, 0x34, 0x7A, 0x08, 0x00, 0x00, 0x00}, false, false, false},
    {"Aquila figurehead", UnlockCategory::Blueprints, UnlockMode::Collect, {0x1E, 0x1C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xC2, 0xFA, 0xD0, 0xE6, 0x0A, 0x00, 0x00, 0x00}, false, false, false},
    {"Blackbeard wheel", UnlockCategory::Blueprints, UnlockMode::Collect, {0x21, 0x37, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xE8, 0x9D, 0xDB, 0x84, 0x1C, 0x00, 0x00, 0x00}, false, false, false},
    {"Aquila wheel", UnlockCategory::Blueprints, UnlockMode::Collect, {0x67, 0x2C, 0x22, 0x12, 0x19, 0x00, 0x00, 0x00}, {0xB8, 0xFA, 0xD0, 0xE6, 0x0A, 0x00, 0x00, 0x00}, false, false, false},
    {"Platform reward pack", UnlockCategory::Extras, UnlockMode::UplayPack, {}, {}, false, false, false},
    {"Legacy outfits and extra content", UnlockCategory::Extras, UnlockMode::LegacyContent, {}, {}, false, false, false},
};
constexpr int kUnlockEntryCount = sizeof(g_unlockEntries) / sizeof(g_unlockEntries[0]);

void Log(const char* text);
void LogError(const char* text);
void LogWarning(const char* text);
void SaveConfig();
void ApplyPlayerGodmode();
void DesynchronizeYourself();
void UpdatePlayerGodmodeBypass();
void ApplyStealthMode();
void ApplyShipOptions();
void ApplyBytePatchToggles();
void CaptureNoclipSpeedDefaults(std::uintptr_t tableAddress);
bool RestoreNoclipSpeedDefaults();
bool InstallGlobalUnlocksPatch();
void MaintainUnlocks();
int CountEnabledUnlocks();
bool InstallAnimusHackPatch();
bool InstallDebugContextPatch();
bool WritePatchedBytes(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size, const char* label);
std::uint8_t* BuildCompleteAllChallengesCave();

std::uintptr_t g_mainModuleBase = 0;
std::size_t g_mainModuleSize = 0;
std::uintptr_t g_bhvAssassinAddress = 0;
std::uintptr_t g_fallDamageAddress = 0;
std::uintptr_t g_noReloadAddress = 0;
std::uintptr_t g_gclShipAddress = 0;
void* g_originalBhvAssassin = nullptr;
void* g_originalFallDamage = nullptr;
void* g_originalNoReload = nullptr;
void* g_originalGclShip = nullptr;
void* g_originalLockConsumables1 = nullptr;
void* g_originalLockConsumables2 = nullptr;
void* g_originalInventorySupplies = nullptr;
void* g_originalInventoryUpdate = nullptr;
void* g_originalInventoryCargo = nullptr;
void* g_originalAnimusHack = nullptr;
void* g_originalCompleteAllChallenges = nullptr;
void* g_originalPlayerSuperSpeed = nullptr;
void* g_originalPlayerSuperJump = nullptr;
void* g_originalMissionTimer1 = nullptr;
void* g_originalMissionTimer2 = nullptr;
void* g_originalMissionTimer3 = nullptr;
void* g_originalGlobalUnlockWrite = nullptr;
void* g_originalDebugContext = nullptr;
std::uint8_t* g_bhvAssassinCave = nullptr;
std::uint8_t* g_fallDamageCave = nullptr;
std::uint8_t* g_noReloadCave = nullptr;
std::uint8_t* g_gclShipCave = nullptr;
std::uint8_t* g_lockConsumables1Cave = nullptr;
std::uint8_t* g_lockConsumables2Cave = nullptr;
std::uint8_t* g_inventorySuppliesCave = nullptr;
std::uint8_t* g_inventoryUpdateCave = nullptr;
std::uint8_t* g_inventoryCargoCave = nullptr;
std::uint8_t* g_animusHackCave = nullptr;
std::uint8_t* g_completeAllChallengesCave = nullptr;
std::uint8_t* g_playerSuperSpeedCave = nullptr;
std::uint8_t* g_playerSuperJumpCave = nullptr;
std::uint8_t* g_missionTimer1Cave = nullptr;
std::uint8_t* g_missionTimer2Cave = nullptr;
std::uint8_t* g_missionTimer3Cave = nullptr;
std::uint8_t* g_globalUnlockWriteCave = nullptr;
std::uint8_t* g_debugContextCave = nullptr;
bool g_playerPointerPatchReady = false;
bool g_shipPatchReady = false;
bool g_noFallDamagePatchReady = false;
bool g_noReloadPatchReady = false;
bool g_lockConsumablesPatchReady = false;
bool g_inventoryEditPatchReady = false;
bool g_freezeMissionTimerPatchReady = false;
bool g_unlimitedResourcesPatchReady = false;
bool g_unlimitedSellingPatchReady = false;
bool g_harpoonGodmodePatchReady = false;
bool g_playerSuperSpeedPatchReady = false;
bool g_playerSuperJumpPatchReady = false;
bool g_nativeGhostReady = false;
bool g_debugContextPatchReady = false;
volatile LONG g_playerPointerHits = 0;
volatile LONG g_shipPointerHits = 0;
volatile LONG g_allyShipHits = 0;
volatile LONG g_fallDamageHits = 0;
volatile LONG g_noReloadHits = 0;
volatile LONG g_lockConsumables1Hits = 0;
volatile LONG g_lockConsumables2Hits = 0;
volatile LONG g_inventorySuppliesHits = 0;
volatile LONG g_inventoryUpdateHits = 0;
volatile LONG g_inventoryCargoHits = 0;
volatile LONG g_inventoryEditQueued = 0;
volatile LONG g_inventoryAppliedId = 0;
volatile LONG g_inventoryAppliedValue = 0;
volatile LONG g_animusHackHits = 0;
volatile LONG g_missionTimer1Hits = 0;
volatile LONG g_missionTimer2Hits = 0;
volatile LONG g_missionTimer3Hits = 0;
volatile LONG g_playerSuperSpeedHits = 0;
volatile LONG g_playerSuperJumpHits = 0;
volatile LONG g_nativeGhostCalls = 0;
volatile LONG g_globalUnlockWriteHits = 0;
std::uintptr_t g_playerBhvAssassin = 0;
std::uintptr_t g_playerEntity = 0;
std::uintptr_t g_playerHealth = 0;
std::uintptr_t g_playerInventory = 0;
std::uintptr_t g_inventorySupplies = 0;
std::uintptr_t g_gclShip = 0;
std::uintptr_t g_allyShip = 0;
std::uintptr_t g_lockConsumables1Address = 0;
std::uintptr_t g_lockConsumables2Address = 0;
std::uintptr_t g_inventorySuppliesAddress = 0;
std::uintptr_t g_inventoryUpdateAddress = 0;
std::uintptr_t g_inventoryCargoAddress = 0;
std::uintptr_t g_animusHackAddress = 0;
std::uintptr_t g_animusChallengeUnlockAddress = 0;
std::uintptr_t g_completeAllChallengesAddress = 0;
std::uintptr_t g_animusCheatMgr = 0;
std::uintptr_t g_missionTimer1Address = 0;
std::uintptr_t g_missionTimer2Address = 0;
std::uintptr_t g_missionTimer3Address = 0;
std::uintptr_t g_unlimitedResourcesAddress = 0;
std::uintptr_t g_unlimitedSellingAddress = 0;
std::uintptr_t g_harpoonGodmode1Address = 0;
std::uintptr_t g_harpoonGodmode2Address = 0;
std::uintptr_t g_playerSuperSpeedAddress = 0;
std::uintptr_t g_playerSuperJumpAddress = 0;
std::uintptr_t g_nativeGhostAddress = 0;
std::uintptr_t g_nativeGhostSpeedTable = 0;
bool g_nativeGhostSpeedDefaultsCaptured = false;
float g_nativeGhostSpeedDefaults[11]{};
std::uintptr_t g_debugContextAddress = 0;
std::uintptr_t g_debugContext = 0;
std::uintptr_t g_globalUnlockReadAddress = 0;
std::uintptr_t g_globalUnlockWriteAddress = 0;
std::uintptr_t g_globalUnlockReturnAddress = 0;
std::uintptr_t g_missionTimer1Base = 0;
std::uintptr_t g_missionTimer2Base = 0;
std::uintptr_t g_missionTimer3Base = 0;
std::int64_t g_missionTimer1Delta = 0;
std::int64_t g_missionTimer2Delta = 0;
std::int64_t g_missionTimer3Delta = 0;
std::uint8_t* g_unlockRecordStorage = nullptr;
std::uint8_t* g_unlockRecordNext = nullptr;
std::uint8_t* g_unlockListExpansion = nullptr;
std::uintptr_t g_unlockMemmap = 0;
std::uintptr_t g_unlockParentCollection = 0;
DWORD g_unlockLastScan = 0;
volatile LONG g_unlockScanRunning = 0;
int g_unlockRecordsFound = 0;
int g_unlockRecordsPatched = 0;
int g_unlockLastResult = 0;
bool g_playerGodmodeBypassActive = false;
ULONGLONG g_playerGodmodeBypassUntil = 0;
int g_inventoryQueuedId = 0;
int g_inventoryQueuedAmount = 0;
int g_inventoryQueuedMode = 0;
int g_inventoryQueuedMax = 0;

void LogWarning(const char* text);
void LogWarningf(const char* format, ...);

void BuildModulePath(char* out, size_t outSize, const char* fileName) {
    strcpy_s(out, outSize, g_moduleDir);
    strcat_s(out, outSize, fileName);
}

void InitModuleDir() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(reinterpret_cast<HMODULE>(&__ImageBase), path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) {
        slash[1] = '\0';
    }
    strcpy_s(g_moduleDir, path);
}

bool ComputeFileSha256(const char* path, char* out, size_t outSize) {
    if (!path || !out || outSize < 65) {
        return false;
    }

    HANDLE file = CreateFileA(path,
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool ok = false;
    if (CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        std::uint8_t buffer[64 * 1024]{};
        DWORD read = 0;
        ok = true;
        BOOL readOk = FALSE;
        while ((readOk = ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) && read > 0) {
            if (!CryptHashData(hash, buffer, read, 0)) {
                ok = false;
                break;
            }
        }
        if (ok && !readOk) {
            ok = false;
        }
        if (ok) {
            std::uint8_t digest[32]{};
            DWORD digestSize = sizeof(digest);
            ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0) && digestSize == sizeof(digest);
            if (ok) {
                char* cursor = out;
                size_t remaining = outSize;
                for (DWORD i = 0; i < digestSize && remaining > 2; ++i) {
                    const int written = sprintf_s(cursor, remaining, "%02X", digest[i]);
                    cursor += written;
                    remaining -= written;
                }
            }
        }
    }

    if (hash) {
        CryptDestroyHash(hash);
    }
    if (provider) {
        CryptReleaseContext(provider, 0);
    }
    CloseHandle(file);
    return ok;
}

void InitGameInfo() {
    GetModuleFileNameA(nullptr, g_gameExePath, MAX_PATH);
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExA(g_gameExePath, GetFileExInfoStandard, &data)) {
        ULARGE_INTEGER size{};
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        g_gameExeSize = size.QuadPart;

        FILETIME localFileTime{};
        SYSTEMTIME systemTime{};
        if (FileTimeToLocalFileTime(&data.ftLastWriteTime, &localFileTime) &&
            FileTimeToSystemTime(&localFileTime, &systemTime)) {
            sprintf_s(g_gameTimestampText,
                      "%04u-%02u-%02u %02u:%02u:%02u",
                      systemTime.wYear,
                      systemTime.wMonth,
                      systemTime.wDay,
                      systemTime.wHour,
                      systemTime.wMinute,
                      systemTime.wSecond);
        }
    }
    if (!ComputeFileSha256(g_gameExePath, g_gameExeSha256, sizeof(g_gameExeSha256))) {
        strcpy_s(g_gameExeSha256, "unknown");
    }
}

void CheckSupportedGameFingerprint() {
    if (g_gameExeSha256[0] == '\0' || strcmp(g_gameExeSha256, "unknown") == 0) {
        LogWarning("Detected exe SHA256 unavailable; supported executable fingerprint could not be checked.");
        return;
    }
    if (_stricmp(g_gameExeSha256, kSupportedGameExeSha256) != 0) {
        LogWarningf("Detected exe SHA256 mismatch. Supported=%s Detected=%s.",
                    kSupportedGameExeSha256,
                    g_gameExeSha256);
    }
}

bool InitMainModuleRange() {
    HMODULE module = GetModuleHandleA(nullptr);
    if (!module) {
        LogError("Main module handle unavailable.");
        return false;
    }
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        LogError("Main module DOS header mismatch.");
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        LogError("Main module NT header mismatch.");
        return false;
    }
    g_mainModuleBase = reinterpret_cast<std::uintptr_t>(module);
    g_mainModuleSize = nt->OptionalHeader.SizeOfImage;
    return g_mainModuleBase != 0 && g_mainModuleSize != 0;
}

bool IsTargetGameProcess() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    const char* fileName = strrchr(path, '\\');
    fileName = fileName ? fileName + 1 : path;
    return _stricmp(fileName, kSupportedGameExe) == 0;
}

void WriteConsoleLogLine(const char* text, LogSeverity severity) {
    if (!g_consoleOut) {
        return;
    }
    if (g_consoleOutput && severity == LogSeverity::Error) {
        SetConsoleTextAttribute(g_consoleOutput, FOREGROUND_RED | FOREGROUND_INTENSITY);
        fprintf(g_consoleOut, "%s\n", text);
        SetConsoleTextAttribute(g_consoleOutput, g_consoleDefaultAttributes);
    } else if (g_consoleOutput && severity == LogSeverity::Warning) {
        SetConsoleTextAttribute(g_consoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        fprintf(g_consoleOut, "%s\n", text);
        SetConsoleTextAttribute(g_consoleOutput, g_consoleDefaultAttributes);
    } else {
        fprintf(g_consoleOut, "%s\n", text);
    }
    fflush(g_consoleOut);
}

void InitConsole() {
    if (!g_consoleLoggingEnabled) {
        return;
    }
    if (g_consoleReady) {
        return;
    }
    g_consoleMutex = CreateMutexA(nullptr, TRUE, "Global\\AC5ToolsConsole");
    if (!g_consoleMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        return;
    }
    if (!AllocConsole()) {
        return;
    }
    SetConsoleTitleA("AC5Tools v0.1.0 Log");
    freopen_s(&g_consoleOut, "CONOUT$", "w", stdout);
    FILE* consoleErr = nullptr;
    freopen_s(&consoleErr, "CONOUT$", "w", stderr);
    g_consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo{};
    if (g_consoleOutput && GetConsoleScreenBufferInfo(g_consoleOutput, &consoleInfo)) {
        g_consoleDefaultAttributes = consoleInfo.wAttributes;
    }
    g_consoleReady = true;
    for (int i = 0; i < g_pendingConsoleLineCount; ++i) {
        const int index = (g_pendingConsoleLineStart + i) % 96;
        WriteConsoleLogLine(g_pendingConsoleLines[index], g_pendingConsoleSeverities[index]);
    }
    g_pendingConsoleLineCount = 0;
    g_pendingConsoleLineStart = 0;
    Log("AC5Tools console attached.");
}

void LogWithSeverity(const char* text, LogSeverity severity) {
    if (g_consoleReady && g_consoleOut) {
        WriteConsoleLogLine(text, severity);
    } else if (g_consoleLoggingEnabled) {
        const int index = (g_pendingConsoleLineStart + g_pendingConsoleLineCount) % 96;
        strncpy_s(g_pendingConsoleLines[index], text, _TRUNCATE);
        g_pendingConsoleSeverities[index] = severity;
        if (g_pendingConsoleLineCount < 96) {
            ++g_pendingConsoleLineCount;
        } else {
            g_pendingConsoleLineStart = (g_pendingConsoleLineStart + 1) % 96;
        }
    }
    if (g_fileLoggingEnabled && g_moduleDir[0]) {
        char path[MAX_PATH]{};
        BuildModulePath(path, sizeof(path), "AC5Tools.log");
        FILE* file = nullptr;
        if (fopen_s(&file, path, "a") == 0 && file) {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            fprintf(file,
                    "[%04u-%02u-%02u %02u:%02u:%02u] %s\n",
                    now.wYear,
                    now.wMonth,
                    now.wDay,
                    now.wHour,
                    now.wMinute,
                    now.wSecond,
                    text);
            fclose(file);
        }
    }
}

void Log(const char* text) {
    LogWithSeverity(text, LogSeverity::Info);
}

void LogError(const char* text) {
    LogWithSeverity(text, LogSeverity::Error);
}

void LogWarning(const char* text) {
    LogWithSeverity(text, LogSeverity::Warning);
}

void Logf(const char* format, ...) {
    char buffer[1024]{};
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);
    Log(buffer);
}

void LogErrorf(const char* format, ...) {
    char buffer[1024]{};
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);
    LogError(buffer);
}

void LogWarningf(const char* format, ...) {
    char buffer[1024]{};
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);
    LogWarning(buffer);
}

bool BytesMatch(const void* address, const std::uint8_t* expected, std::size_t size) {
    return memcmp(address, expected, size) == 0;
}

void LogPatchMismatch(const char* label,
                      const std::uint8_t* found,
                      const std::uint8_t* expected,
                      std::size_t size) {
    char line[1024]{};
    char* out = line;
    size_t remaining = sizeof(line);
    int written = sprintf_s(out, remaining, "%s byte mismatch. Expected:", label);
    out += written;
    remaining -= written;
    for (std::size_t i = 0; i < size && remaining > 4; ++i) {
        written = sprintf_s(out, remaining, " %02X", expected[i]);
        out += written;
        remaining -= written;
    }
    written = sprintf_s(out, remaining, " Found:");
    out += written;
    remaining -= written;
    for (std::size_t i = 0; i < size && remaining > 4; ++i) {
        written = sprintf_s(out, remaining, " %02X", found[i]);
        out += written;
        remaining -= written;
    }
    LogError(line);
}

std::uintptr_t FindMainModulePatternUnique(const std::uint8_t* pattern,
                                           std::size_t size,
                                           const char* label) {
    if (!g_mainModuleBase || !g_mainModuleSize || size == 0 || size > g_mainModuleSize) {
        return 0;
    }

    std::uintptr_t found = 0;
    int count = 0;
    const auto* begin = reinterpret_cast<const std::uint8_t*>(g_mainModuleBase);
    for (std::size_t i = 0; i <= g_mainModuleSize - size; ++i) {
        if (memcmp(begin + i, pattern, size) == 0) {
            found = g_mainModuleBase + i;
            ++count;
            if (count > 1) {
                LogErrorf("%s unavailable: signature is not unique.", label);
                return 0;
            }
        }
    }

    if (!found) {
        LogErrorf("%s unavailable: signature was not found.", label);
    }
    return found;
}

std::uintptr_t FindMainModuleMaskedPatternUnique(const std::uint8_t* pattern,
                                                 const char* mask,
                                                 std::size_t size,
                                                 const char* label) {
    if (!g_mainModuleBase || !g_mainModuleSize || size == 0 || size > g_mainModuleSize) {
        return 0;
    }

    std::uintptr_t found = 0;
    int count = 0;
    const auto* begin = reinterpret_cast<const std::uint8_t*>(g_mainModuleBase);
    for (std::size_t i = 0; i <= g_mainModuleSize - size; ++i) {
        bool match = true;
        for (std::size_t j = 0; j < size; ++j) {
            if (mask[j] == 'x' && begin[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            found = g_mainModuleBase + i;
            ++count;
            if (count > 1) {
                LogErrorf("%s unavailable: signature is not unique.", label);
                return 0;
            }
        }
    }

    if (!found) {
        LogErrorf("%s unavailable: signature was not found.", label);
    }
    return found;
}

bool TryReadMemory(std::uintptr_t address, void* out, std::size_t size) {
    __try {
        memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool TryWriteMemory(std::uintptr_t address, const void* data, std::size_t size) {
    DWORD oldProtect = 0;
    auto* target = reinterpret_cast<void*>(address);
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LogErrorf("Memory write failed: VirtualProtect failed at 0x%p, error %lu.", target, GetLastError());
        return false;
    }
    bool ok = false;
    __try {
        memcpy(target, data, size);
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    FlushInstructionCache(GetCurrentProcess(), target, size);
    if (!VirtualProtect(target, size, oldProtect, &oldProtect)) {
        LogWarningf("Memory write warning: restore protection failed at 0x%p, error %lu.", target, GetLastError());
    }
    return ok;
}

bool TryReadQword(std::uintptr_t address, std::uintptr_t& value) {
    return TryReadMemory(address, &value, sizeof(value));
}

bool TryReadWord(std::uintptr_t address, std::uint16_t& value) {
    return TryReadMemory(address, &value, sizeof(value));
}

bool TryReadByte(std::uintptr_t address, std::uint8_t& value) {
    return TryReadMemory(address, &value, sizeof(value));
}

bool TryWriteByte(std::uintptr_t address, std::uint8_t value) {
    return TryWriteMemory(address, &value, sizeof(value));
}

bool IsExecutableProtect(DWORD protect) {
    const DWORD p = protect & 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ ||
           p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadableRegion(const MEMORY_BASIC_INFORMATION& mbi, bool requireNonExecutable) {
    if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) {
        return false;
    }
    if (requireNonExecutable && IsExecutableProtect(mbi.Protect)) {
        return false;
    }
    return true;
}

std::uintptr_t FindProcessBytes(const std::uint8_t* pattern,
                                std::size_t size,
                                std::size_t alignment,
                                bool requireNonExecutable) {
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const std::uintptr_t maxAddress = reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);

    while (address < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            address += 0x1000;
            continue;
        }

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t next = base + mbi.RegionSize;
        if (IsReadableRegion(mbi, requireNonExecutable) && mbi.RegionSize >= size) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
            __try {
                for (std::size_t offset = 0; offset <= mbi.RegionSize - size; ++offset) {
                    const std::uintptr_t current = base + offset;
                    if (alignment > 1 && (current % alignment) != 0) {
                        continue;
                    }
                    if (memcmp(bytes + offset, pattern, size) == 0) {
                        return current;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        if (next <= address) {
            break;
        }
        address = next;
    }
    return 0;
}

std::uintptr_t FindUnlockRecordById(const std::uint8_t id[8]) {
    std::uint8_t pattern[12] = {0x01, 0x00, 0x00, 0x80};
    memcpy(pattern + 4, id, 8);

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const std::uintptr_t maxAddress = reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    while (address < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            address += 0x1000;
            continue;
        }

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t next = base + mbi.RegionSize;
        if (IsReadableRegion(mbi, true) && mbi.RegionSize >= sizeof(pattern)) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
            __try {
                for (std::size_t offset = 0; offset <= mbi.RegionSize - sizeof(pattern); ++offset) {
                    const std::uintptr_t current = base + offset;
                    if ((current % 4) != 0 || current < 12 ||
                        memcmp(bytes + offset, pattern, sizeof(pattern)) != 0) {
                        continue;
                    }

                    std::uintptr_t item = 0;
                    std::uint8_t locked = 0;
                    if (TryReadQword(current - 12, item) &&
                        item &&
                        TryReadByte(item + 0x40, locked)) {
                        return current;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        if (next <= address) {
            break;
        }
        address = next;
    }
    return 0;
}

void MarkUnlockAllocation(std::uintptr_t address) {
    if (!g_unlockMemmap) {
        return;
    }
    const std::uintptr_t index = (address / 0x10000) % 0x10000;
    TryWriteByte(g_unlockMemmap + index, 129);
}

void EmitMovRaxImm64(std::vector<std::uint8_t>& code, std::uintptr_t value) {
    code.push_back(0x48);
    code.push_back(0xB8);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    code.insert(code.end(), bytes, bytes + sizeof(value));
}

void EmitCallRax(std::vector<std::uint8_t>& code) {
    code.push_back(0xFF);
    code.push_back(0xD0);
}

void EmitJmpAbs(std::vector<std::uint8_t>& code, std::uintptr_t target, std::size_t* literalOffset = nullptr) {
    code.push_back(0xFF);
    code.push_back(0x25);
    code.push_back(0x00);
    code.push_back(0x00);
    code.push_back(0x00);
    code.push_back(0x00);
    if (literalOffset) {
        *literalOffset = code.size();
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&target);
    code.insert(code.end(), bytes, bytes + sizeof(target));
}

std::uint8_t* AllocateCodeCave(const std::vector<std::uint8_t>& code, const char* label) {
    auto* cave = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!cave) {
        LogErrorf("VirtualAlloc failed for %s.", label);
        return nullptr;
    }
    memcpy(cave, code.data(), code.size());
    FlushInstructionCache(GetCurrentProcess(), cave, code.size());
    return cave;
}

void CapturePlayerPointers(void* bhvAssassin) {
    InterlockedIncrement(&g_playerPointerHits);
    if (!bhvAssassin) {
        return;
    }

    __try {
        const auto bhv = reinterpret_cast<std::uintptr_t>(bhvAssassin);
        const auto entity = *reinterpret_cast<std::uintptr_t*>(bhv + 0x10);
        const auto playerData = *reinterpret_cast<std::uintptr_t*>(bhv + 0x30);
        const auto healthOwner = playerData ? *reinterpret_cast<std::uintptr_t*>(playerData + 0x60) : 0;
        const auto health = healthOwner ? *reinterpret_cast<std::uintptr_t*>(healthOwner + 0x78) : 0;
        const auto inventoryAccess = playerData ? *reinterpret_cast<std::uintptr_t*>(playerData + 0xB0) : 0;
        const auto inventory = inventoryAccess ? *reinterpret_cast<std::uintptr_t*>(inventoryAccess) : 0;

        g_playerBhvAssassin = bhv;
        g_playerEntity = entity;
        g_playerHealth = health;
        g_playerInventory = inventory;

        *reinterpret_cast<std::uint8_t*>(bhv + 0x48) = g_stealthMode ? 1 : 0;
        if (health) {
            *reinterpret_cast<std::uint8_t*>(health + kPlayerGodmodeFlagOffset) =
                (g_playerGodmode && !g_playerGodmodeBypassActive) ? 1 : 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogError("Player pointer capture skipped after invalid pointer access.");
    }
}

void CaptureDebugContext(void* context) {
    if (context) {
        g_debugContext = reinterpret_cast<std::uintptr_t>(context);
    }
}

void CaptureInventorySupplies(void* logicalInventory) {
    InterlockedIncrement(&g_inventorySuppliesHits);
    if (logicalInventory) {
        g_inventorySupplies = reinterpret_cast<std::uintptr_t>(logicalInventory);
    }
}

int ClampInventoryAmount(const InventoryEditEntry& entry, int amount) {
    if (amount < 0) {
        return 0;
    }
    if (amount > entry.maxValue) {
        return entry.maxValue;
    }
    return amount;
}

void UpdateInventoryEditQueuedFlag() {
    LONG queued = 0;
    for (int i = 0; i < kInventoryEntryCount; ++i) {
        if (g_inventoryEntries[i].queued) {
            g_inventoryQueuedId = g_inventoryEntries[i].id;
            g_inventoryQueuedAmount = ClampInventoryAmount(g_inventoryEntries[i], g_inventoryEntries[i].amount);
            g_inventoryQueuedMode = g_inventoryEntries[i].mode == InventoryEditMode::Add ? 1 : 0;
            g_inventoryQueuedMax = g_inventoryEntries[i].maxValue;
            queued = 1;
            break;
        }
    }
    if (!queued) {
        g_inventoryQueuedId = 0;
        g_inventoryQueuedAmount = 0;
        g_inventoryQueuedMode = 0;
        g_inventoryQueuedMax = 0;
    }
    InterlockedExchange(&g_inventoryEditQueued, queued);
}

void MaintainInventoryEditState() {
    const LONG appliedId = InterlockedExchange(&g_inventoryAppliedId, 0);
    if (!appliedId) {
        return;
    }

    const int appliedValue = static_cast<int>(InterlockedExchange(&g_inventoryAppliedValue, 0));
    for (int i = 0; i < kInventoryEntryCount; ++i) {
        InventoryEditEntry& entry = g_inventoryEntries[i];
        if (entry.id != appliedId) {
            continue;
        }
        entry.lastApplied = appliedValue;
        entry.queued = false;
        Logf("Inventory edit applied: %s -> %d.", entry.name, appliedValue);
        break;
    }
    UpdateInventoryEditQueuedFlag();
}

std::uintptr_t ResolvePointerChain(std::uintptr_t base, const std::size_t* offsets, int count) {
    std::uintptr_t address = base;
    for (int i = 0; i < count - 1; ++i) {
        address = *reinterpret_cast<std::uintptr_t*>(address + offsets[i]);
        if (!address) {
            return 0;
        }
    }
    return address + offsets[count - 1];
}

void WriteShipReloadTimer(std::uintptr_t ship, std::size_t weaponOffset) {
    const std::size_t offsets[] = {0xB8, weaponOffset, 0x0, 0xAC};
    const auto timerAddress = ResolvePointerChain(ship, offsets, 4);
    if (timerAddress) {
        *reinterpret_cast<float*>(timerAddress) = 0.0f;
    }
}

bool IsSaneHealthValue(float value) {
    return value > 0.0f && value < 1000000.0f;
}

void TopUpShipHealth(std::uintptr_t ship) {
    const float maxHealth = *reinterpret_cast<float*>(ship + 0x268);
    if (!IsSaneHealthValue(maxHealth)) {
        return;
    }

    *reinterpret_cast<float*>(ship + 0x26C) = maxHealth;
    *reinterpret_cast<float*>(ship + 0x7B0) = maxHealth;

    const auto healthStruct = *reinterpret_cast<std::uintptr_t*>(ship + 0xB8);
    if (healthStruct) {
        *reinterpret_cast<float*>(healthStruct + 0x2CC) = 0.0f;
        *reinterpret_cast<float*>(healthStruct + 0x2BC) = maxHealth;
    }
}

std::uint16_t ReadShipType(std::uintptr_t ship) {
    __try {
        return *reinterpret_cast<std::uint16_t*>(ship + 0x1E8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0xFFFF;
    }
}

void ApplyShipOptionsTo(std::uintptr_t ship) {
    *reinterpret_cast<std::uint8_t*>(ship + 0x464) = g_shipGodmode ? 1 : 0;
    if (g_shipGodmode) {
        TopUpShipHealth(ship);
    }
    *reinterpret_cast<std::uint8_t*>(ship + 0x7E4) = g_shipStealthMode ? 0 : 1;
    if (g_noCannonCooldown) {
        WriteShipReloadTimer(ship, 0x50); // Broadside cannon
        WriteShipReloadTimer(ship, 0x88); // Heavy shot
        WriteShipReloadTimer(ship, 0x78); // Mortar
        WriteShipReloadTimer(ship, 0x58); // Front carronades
    }
}

void ApplyAllyShipGodmodeTo(std::uintptr_t ship) {
    if (!g_allyGodmode) {
        return;
    }

    TopUpShipHealth(ship);
}

void ApplyShipOptions() {
    if (g_gclShip) {
        __try {
            ApplyShipOptionsTo(g_gclShip);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogError("Ship option write skipped after invalid GclShip pointer access.");
            g_gclShip = 0;
        }
    }

    if (g_allyShip) {
        __try {
            ApplyAllyShipGodmodeTo(g_allyShip);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogError("Ally Godmode write skipped after invalid friendly ship pointer access.");
            g_allyShip = 0;
        }
    }
}

void CaptureShipPointer(void* ship) {
    InterlockedIncrement(&g_shipPointerHits);
    if (!ship) {
        return;
    }

    const auto shipAddress = reinterpret_cast<std::uintptr_t>(ship);
    g_gclShip = shipAddress;
    const auto shipType = ReadShipType(shipAddress);
    if (shipType == 2) {
        InterlockedIncrement(&g_allyShipHits);
        g_allyShip = shipAddress;
        __try {
            ApplyAllyShipGodmodeTo(shipAddress);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogError("Ally Godmode capture skipped after invalid friendly ship pointer access.");
            g_allyShip = 0;
        }
        return;
    }

    ApplyShipOptions();
}

void ResetMissionTimerDeltas() {
    g_missionTimer1Delta = 0;
    g_missionTimer2Delta = 0;
    g_missionTimer3Delta = 0;
}

std::uintptr_t FindUnlockCollectionListForParent(std::uintptr_t parentCollection) {
    std::uint8_t parentBytes[8]{};
    memcpy(parentBytes, &parentCollection, sizeof(parentBytes));

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const std::uintptr_t maxAddress = reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    while (address < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            address += 0x1000;
            continue;
        }

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t next = base + mbi.RegionSize;
        if (IsReadableRegion(mbi, true) && mbi.RegionSize >= sizeof(parentBytes)) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
            __try {
                for (std::size_t offset = 0; offset <= mbi.RegionSize - sizeof(parentBytes); ++offset) {
                    const std::uintptr_t match = base + offset;
                    if ((match % 8) != 0 || memcmp(bytes + offset, parentBytes, sizeof(parentBytes)) != 0) {
                        continue;
                    }

                    std::uintptr_t point1 = 0;
                    std::uintptr_t a = 0;
                    std::uintptr_t b = 0;
                    std::uintptr_t c = 0;
                    std::uintptr_t list = 0;
                    std::uint8_t marker = 0;
                    if (TryReadQword(match + 0x40, point1) &&
                        point1 != match &&
                        point1 != 0 &&
                        TryReadQword(point1 + 0xB0, a) &&
                        TryReadQword(a, b) &&
                        TryReadQword(b + 0x18, c) &&
                        TryReadQword(c + 0x10, list) &&
                        TryReadByte(list + 0x08, marker) &&
                        marker == 9) {
                        return list;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        if (next <= address) {
            break;
        }
        address = next;
    }
    return 0;
}

std::uintptr_t ResolveUnlockParentCollection() {
    constexpr std::uint8_t pattern[] = {
        0x4C, 0x8D, 0x1D, 0, 0, 0, 0, 0x48, 0x8D, 0x8B, 0xB0, 0x00, 0x00, 0x00, 0x33, 0xD2,
    };
    constexpr char mask[] = "xxx????xxxxxxxxx";
    int candidateCount = 0;

    const auto* begin = reinterpret_cast<const std::uint8_t*>(g_mainModuleBase);
    for (std::size_t i = 0; i <= g_mainModuleSize - sizeof(pattern); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < sizeof(pattern); ++j) {
            if (mask[j] == 'x' && begin[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (!match) {
            continue;
        }

        std::int32_t rel = 0;
        const auto address = g_mainModuleBase + i;
        if (!TryReadMemory(address + 3, &rel, sizeof(rel))) {
            continue;
        }
        const auto parent = address + 7 + rel;
        ++candidateCount;
        if (FindUnlockCollectionListForParent(parent)) {
            Logf("Unlock parent collection resolved at 0x%p from %d candidates.",
                 reinterpret_cast<void*>(parent),
                 candidateCount);
            return parent;
        }
    }

    LogErrorf("Unlock parent collection unavailable: no valid runtime chain among %d candidates.", candidateCount);
    return 0;
}

std::uintptr_t FindUnlockCollectionList() {
    if (!g_unlockParentCollection) {
        return 0;
    }
    return FindUnlockCollectionListForParent(g_unlockParentCollection);
}

bool EnsureUnlockSupportMemory() {
    if (g_unlockRecordStorage && g_unlockMemmap && g_unlockParentCollection) {
        return true;
    }

    if (!g_unlockRecordStorage) {
        g_unlockRecordStorage = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_unlockRecordStorage) {
            LogError("Unlock support unavailable: record storage allocation failed.");
            return false;
        }
        g_unlockRecordNext = g_unlockRecordStorage + 16;
    }

    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    if (!g_unlockParentCollection) {
        g_unlockParentCollection = ResolveUnlockParentCollection();
        if (!g_unlockParentCollection) {
            return false;
        }
    }

    if (!g_unlockMemmap) {
        constexpr std::uint8_t pattern[] = {
            0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x85, 0xC9, 0x74, 0, 0x48, 0x83, 0xF8, 0xFF, 0x75, 0x33, 0x44, 0x8D, 0x48, 0x33,
        };
        constexpr char mask[] = "xxx????xxx?xxxxxxxxxx";
        const auto address = FindMainModuleMaskedPatternUnique(pattern, mask, sizeof(pattern), "Unlock memory map");
        if (!address) {
            return false;
        }
        std::int32_t rel = 0;
        std::uintptr_t holder = 0;
        std::uintptr_t mapOwner = 0;
        if (!TryReadMemory(address + 3, &rel, sizeof(rel)) ||
            !TryReadQword(address + 7 + rel, holder) ||
            !TryReadQword(holder + 0x370, mapOwner)) {
            LogError("Unlock memory map unavailable: pointer chain was not ready.");
            return false;
        }
        g_unlockMemmap = mapOwner;
    }

    MarkUnlockAllocation(reinterpret_cast<std::uintptr_t>(g_unlockRecordStorage));
    return true;
}

bool UnlockItemById(const std::uint8_t id[8], std::uintptr_t& itemAddress, bool requireLocked) {
    const auto record = FindUnlockRecordById(id);
    if (!record || record < 12) {
        LogError("Single unlock prerequisite failed: id record was not found.");
        return false;
    }

    std::uintptr_t item = 0;
    std::uint8_t locked = 0;
    if (!TryReadQword(record - 12, item) || !item) {
        LogErrorf("Single unlock prerequisite failed: item pointer unreadable at 0x%p.", reinterpret_cast<void*>(record - 12));
        return false;
    }
    itemAddress = item + 0x40;
    if (!TryReadByte(itemAddress, locked)) {
        LogErrorf("Single unlock prerequisite failed: lock byte unreadable at 0x%p.", reinterpret_cast<void*>(itemAddress));
        return false;
    }
    if (requireLocked && locked == 0) {
        return true;
    }
    if (!TryWriteByte(itemAddress, 0)) {
        LogErrorf("Single unlock prerequisite failed: lock byte write failed at 0x%p, current=%u.",
             reinterpret_cast<void*>(itemAddress),
             static_cast<unsigned>(locked));
        return false;
    }
    std::uint8_t after = 0xFF;
    if (TryReadByte(itemAddress, after) && after != 0) {
        LogErrorf("Single unlock prerequisite failed: lock byte remained %u at 0x%p.",
             static_cast<unsigned>(after),
             reinterpret_cast<void*>(itemAddress));
        return false;
    }
    return true;
}

bool ApplyNormalUnlock(UnlockEntry& entry) {
    const auto record = FindUnlockRecordById(entry.item);
    if (!record || record < 12) {
        return false;
    }

    std::uintptr_t normalRoot = 0;
    std::uint8_t status = 0;
    std::uintptr_t firstLevel = 0;
    std::uintptr_t secondLevel = 0;
    std::uintptr_t thirdLevel = 0;
    std::uintptr_t list = 0;
    std::uint16_t count = 0;
    if (!TryReadQword(record - 12, normalRoot) ||
        !normalRoot ||
        !TryReadByte(normalRoot + 0x14, status)) {
        return false;
    }
    if (status != 1) {
        return true;
    }
    if (!TryWriteByte(normalRoot + 0x14, 3) ||
        !TryReadQword(normalRoot + 0x58, firstLevel) ||
        !TryReadQword(firstLevel, secondLevel) ||
        !TryReadQword(secondLevel + 0x10, thirdLevel) ||
        !TryReadQword(thirdLevel + 0x48, list) ||
        !TryReadWord(thirdLevel + 0x52, count)) {
        return false;
    }
    if (count > 512) {
        Logf("Single unlock skipped: child list count looked unsafe for %s (%u).",
             entry.name,
             static_cast<unsigned>(count));
        return false;
    }

    bool wroteAny = false;
    for (std::uint16_t i = 0; i < count; ++i) {
        std::uintptr_t node = 0;
        std::uintptr_t item = 0;
        if (TryReadQword(list + static_cast<std::uintptr_t>(i) * 8, node) &&
            TryReadQword(node, item) &&
            item &&
            TryWriteByte(item + 0x40, 0)) {
            wroteAny = true;
        }
    }
    return wroteAny;
}

bool AppendUnlockCollectionItem(std::uintptr_t itemPointer) {
    if (!EnsureUnlockSupportMemory()) {
        LogError("Single unlock collect failed: support memory was not ready.");
        return false;
    }

    const auto listFound = FindUnlockCollectionList();
    if (!listFound) {
        LogError("Single unlock collect failed: collection list was not found.");
        return false;
    }

    std::uint16_t length = 0;
    std::uint16_t capacity = 0;
    std::uintptr_t array = 0;
    if (!TryReadWord(listFound + 0x22, length) ||
        !TryReadWord(listFound + 0x20, capacity) ||
        !TryReadQword(listFound + 0x18, array)) {
        LogError("Single unlock collect failed: collection list fields were unreadable.");
        return false;
    }

    for (std::uint16_t i = 0; i < length; ++i) {
        std::uintptr_t record = 0;
        std::uintptr_t existingItem = 0;
        if (TryReadQword(array + static_cast<std::uintptr_t>(i) * 8, record) &&
            record &&
            TryReadQword(record + 8, existingItem) &&
            existingItem == itemPointer) {
            return true;
        }
    }

    if (length + 1 >= capacity) {
        if (!g_unlockListExpansion) {
            g_unlockListExpansion = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 0x8000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (!g_unlockListExpansion) {
                LogError("Single unlock collect failed: list expansion allocation failed.");
                return false;
            }
            MarkUnlockAllocation(reinterpret_cast<std::uintptr_t>(g_unlockListExpansion));
        }
        if (!TryReadMemory(array, g_unlockListExpansion, static_cast<std::size_t>(length) * 8)) {
            LogError("Single unlock collect failed: existing collection array copy failed.");
            return false;
        }
        array = reinterpret_cast<std::uintptr_t>(g_unlockListExpansion);
        const std::uint16_t newCapacity = length + 64;
        TryWriteMemory(listFound + 0x18, &array, sizeof(array));
        TryWriteMemory(listFound + 0x20, &newCapacity, sizeof(newCapacity));
    }

    if (!g_unlockRecordNext || g_unlockRecordNext + 24 > g_unlockRecordStorage + 0x4000) {
        LogError("Single unlock collect failed: record storage is full.");
        return false;
    }

    const std::uintptr_t record = reinterpret_cast<std::uintptr_t>(g_unlockRecordNext);
    const std::uintptr_t typeA = g_mainModuleBase + 0x2A01940;
    const std::uintptr_t typeB = g_mainModuleBase + 0x32DE688;
    if (!TryWriteMemory(record, &typeA, sizeof(typeA)) ||
        !TryWriteMemory(record + 8, &itemPointer, sizeof(itemPointer)) ||
        !TryWriteMemory(record + 16, &typeB, sizeof(typeB)) ||
        !TryWriteMemory(array + static_cast<std::uintptr_t>(length) * 8, &record, sizeof(record))) {
        LogError("Single unlock collect failed: collection record write failed.");
        return false;
    }
    ++length;
    if (!TryWriteMemory(listFound + 0x22, &length, sizeof(length))) {
        LogError("Single unlock collect failed: collection length write failed.");
        return false;
    }
    g_unlockRecordNext += 24;
    return true;
}

bool ApplyCollectUnlock(UnlockEntry& entry) {
    std::uintptr_t requiredItem = 0;
    if (!UnlockItemById(entry.required, requiredItem, true)) {
        LogErrorf("Single unlock collect failed: prerequisite was not found or writable for %s.", entry.name);
        return false;
    }

    const auto record = FindUnlockRecordById(entry.item);
    if (!record || record < 12) {
        LogErrorf("Single unlock collect failed: reward record was not found for %s.", entry.name);
        return false;
    }
    return AppendUnlockCollectionItem(record - 12);
}

bool ApplyPlatformRewardPack() {
    constexpr std::uint8_t pattern[] = {
        0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x0F, 0xB6, 0x88, 0x7D, 0x06, 0x00, 0x00,
    };
    constexpr char mask[] = "xxx????xxxxxxx";
    const auto address = FindMainModuleMaskedPatternUnique(pattern, mask, sizeof(pattern), "Extra Content reward pack");
    if (!address) {
        return false;
    }
    std::int32_t rel = 0;
    std::uintptr_t holder = 0;
    if (!TryReadMemory(address + 3, &rel, sizeof(rel)) ||
        !TryReadQword(address + 7 + rel, holder) ||
        !holder) {
        return false;
    }
    const std::uint8_t value = 15;
    return TryWriteByte(holder + 0x67D, value);
}

bool ApplyLegacyContent() {
    constexpr char key[] = "ULCDRM000000";
    const auto address = FindProcessBytes(reinterpret_cast<const std::uint8_t*>(key), sizeof(key) - 1, 4, true);
    if (!address) {
        return false;
    }

    int writes = 0;
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const std::uintptr_t maxAddress = reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    while (cursor < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            cursor += 0x1000;
            continue;
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t next = base + mbi.RegionSize;
        if (IsReadableRegion(mbi, true) && mbi.RegionSize >= sizeof(key) - 1) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
            __try {
                for (std::size_t offset = 0; offset <= mbi.RegionSize - (sizeof(key) - 1); ++offset) {
                    const std::uintptr_t current = base + offset;
                    if (current % 4 == 0 && memcmp(bytes + offset, key, sizeof(key) - 1) == 0) {
                        if (TryWriteByte(current + 0x11, 1)) {
                            ++writes;
                        }
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        if (next <= cursor) {
            break;
        }
        cursor = next;
    }
    return writes > 0;
}

bool ApplyUnlockEntry(UnlockEntry& entry) {
    switch (entry.mode) {
        case UnlockMode::Normal:
            return ApplyNormalUnlock(entry);
        case UnlockMode::Collect:
            return ApplyCollectUnlock(entry);
        case UnlockMode::UplayPack:
            return ApplyPlatformRewardPack();
        case UnlockMode::LegacyContent:
            return ApplyLegacyContent();
    }
    return false;
}

void MaintainUnlocks() {
    if (CountEnabledUnlocks() == 0) {
        g_unlockLastScan = 0;
        InterlockedExchange(&g_unlockScanRunning, 0);
        return;
    }

    const DWORD now = GetTickCount();
    if (g_unlockLastScan != 0 && now - g_unlockLastScan < 15000) {
        return;
    }
    g_unlockLastScan = now;
    if (InterlockedCompareExchange(&g_unlockScanRunning, 1, 0) != 0) {
        return;
    }

    int found = 0;
    int patched = 0;
    for (int i = 0; i < kUnlockEntryCount; ++i) {
        UnlockEntry& entry = g_unlockEntries[i];
        if (!entry.enabled) {
            continue;
        }
        entry.found = true;
        if (ApplyUnlockEntry(entry)) {
            entry.patched = true;
            entry.enabled = false;
            ++patched;
            Logf("Single unlock applied: %s.", entry.name);
        } else {
            entry.found = false;
        }
        ++found;
    }
    g_unlockRecordsFound = found;
    g_unlockRecordsPatched += patched;
    g_unlockLastResult = patched;
    InterlockedExchange(&g_unlockScanRunning, 0);
}

void UpdateMissionTimer32(std::uintptr_t base,
                          std::uintptr_t& storedBase,
                          std::int64_t& delta,
                          std::size_t endOffset,
                          std::size_t currentPtrOffset) {
    storedBase = base;
    if (!g_freezeMissionTimer) {
        delta = 0;
        return;
    }

    __try {
        const auto currentPtr = *reinterpret_cast<std::uintptr_t*>(base + currentPtrOffset);
        if (!currentPtr) {
            return;
        }
        const auto endValue = *reinterpret_cast<std::int32_t*>(base + endOffset);
        const auto currentValue = *reinterpret_cast<std::int32_t*>(currentPtr);
        if (delta == 0) {
            delta = static_cast<std::int64_t>(endValue) - static_cast<std::int64_t>(currentValue);
        }
        *reinterpret_cast<std::int32_t*>(base + endOffset) =
            static_cast<std::int32_t>(static_cast<std::int64_t>(currentValue) + delta);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        storedBase = 0;
        delta = 0;
        LogWarning("Freeze Mission Timer skipped after invalid timer pointer access.");
    }
}

void UpdateMissionTimer64(std::uintptr_t base,
                          std::uintptr_t& storedBase,
                          std::int64_t& delta,
                          std::size_t endOffset,
                          std::size_t currentPtrOffset) {
    storedBase = base;
    if (!g_freezeMissionTimer) {
        delta = 0;
        return;
    }

    __try {
        const auto currentPtr = *reinterpret_cast<std::uintptr_t*>(base + currentPtrOffset);
        if (!currentPtr) {
            return;
        }
        const auto endValue = *reinterpret_cast<std::int64_t*>(base + endOffset);
        const auto currentValue = *reinterpret_cast<std::int64_t*>(currentPtr);
        if (delta == 0) {
            delta = endValue - currentValue;
        }
        *reinterpret_cast<std::int64_t*>(base + endOffset) = currentValue + delta;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        storedBase = 0;
        delta = 0;
        LogWarning("Freeze Mission Timer skipped after invalid timer pointer access.");
    }
}

void CaptureMissionTimer(void* timerBase, int timerKind) {
    if (!timerBase) {
        return;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(timerBase);
    switch (timerKind) {
        case 1:
            InterlockedIncrement(&g_missionTimer1Hits);
            UpdateMissionTimer32(base, g_missionTimer1Base, g_missionTimer1Delta, 0x170, 0x188);
            break;
        case 2:
            InterlockedIncrement(&g_missionTimer2Hits);
            UpdateMissionTimer32(base, g_missionTimer2Base, g_missionTimer2Delta, 0xA8, 0xC0);
            break;
        case 3:
            InterlockedIncrement(&g_missionTimer3Hits);
            UpdateMissionTimer64(base, g_missionTimer3Base, g_missionTimer3Delta, 0x38, 0x50);
            break;
    }
}

void ApplyStealthMode() {
    if (!g_playerBhvAssassin) {
        return;
    }
    __try {
        *reinterpret_cast<std::uint8_t*>(g_playerBhvAssassin + 0x48) = g_stealthMode ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogWarning("Stealth Mode write skipped after invalid BhvAssassin pointer access.");
        g_playerBhvAssassin = 0;
    }
}

void ApplyPlayerGodmode() {
    if (!g_playerHealth) {
        return;
    }
    __try {
        *reinterpret_cast<std::uint8_t*>(g_playerHealth + kPlayerGodmodeFlagOffset) =
            (g_playerGodmode && !g_playerGodmodeBypassActive) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogWarning("Player Godmode write skipped after invalid health pointer access.");
        g_playerHealth = 0;
    }
}

void UpdatePlayerGodmodeBypass() {
    if (!g_playerGodmodeBypassActive || GetTickCount64() < g_playerGodmodeBypassUntil) {
        return;
    }

    g_playerGodmodeBypassActive = false;
    ApplyPlayerGodmode();
    Log("Player God Mode restored after Desynchronize Yourself.");
}

bool ResolveDebugFunction(std::uintptr_t offset, std::uintptr_t& functionAddress) {
    if (!g_debugContextAddress) {
        return false;
    }

    __try {
        const auto entry = g_debugContextAddress + offset;
        const auto rel = *reinterpret_cast<std::int32_t*>(entry + 3);
        functionAddress = entry + 7 + rel;
        return functionAddress >= g_mainModuleBase &&
               functionAddress < g_mainModuleBase + g_mainModuleSize;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CallNativeDesynchronize() {
    if (!g_debugContextPatchReady || !g_debugContext) {
        return false;
    }

    std::uintptr_t functionAddress = 0;
    if (!ResolveDebugFunction(kDebugNukeYourselfOffset, functionAddress)) {
        LogError("Desynchronize Yourself native call unavailable: function target could not be resolved.");
        return false;
    }

    using DebugActionFn = void (*)(std::uint64_t, void*);
    auto action = reinterpret_cast<DebugActionFn>(functionAddress);
    __try {
        action(1, reinterpret_cast<void*>(g_debugContext));
        Logf("Desynchronize Yourself native call triggered at 0x%p with context 0x%p.",
             reinterpret_cast<void*>(functionAddress),
             reinterpret_cast<void*>(g_debugContext));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogError("Desynchronize Yourself native call failed after guarded exception.");
        return false;
    }
}

void DesynchronizeYourself() {
    if ((!g_playerPointerPatchReady || !g_playerHealth) &&
        (!g_debugContextPatchReady || !g_debugContext)) {
        LogError("Desynchronize Yourself unavailable: player health/debug context is not ready.");
        return;
    }

    const bool restoreGodmode = g_playerGodmode;
    if (restoreGodmode) {
        g_playerGodmodeBypassActive = true;
        g_playerGodmodeBypassUntil = GetTickCount64() + kPlayerGodmodeBypassMs;
        ApplyPlayerGodmode();
    }

    if (CallNativeDesynchronize()) {
        return;
    }

    __try {
        if (!g_playerHealth) {
            LogError("Desynchronize Yourself fallback unavailable: player health pointer is not ready.");
            return;
        }
        *reinterpret_cast<std::uint8_t*>(g_playerHealth + kPlayerGodmodeFlagOffset) = 0;
        *reinterpret_cast<std::int16_t*>(g_playerHealth + kPlayerHealthValueOffset) = -1;
        Logf("Desynchronize Yourself fallback wrote player health component 0x%p.",
             reinterpret_cast<void*>(g_playerHealth));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogWarning("Desynchronize Yourself skipped after invalid health pointer access.");
        g_playerHealth = 0;
    }
}

float ReadIniFloat(const char* section, const char* key, float fallback, const char* path) {
    char value[64]{};
    char defaultValue[64]{};
    sprintf_s(defaultValue, "%f", fallback);
    GetPrivateProfileStringA(section, key, defaultValue, value, sizeof(value), path);
    return static_cast<float>(atof(value));
}

void LoadConfig() {
    char path[MAX_PATH]{};
    BuildModulePath(path, sizeof(path), "AC5Tools.ini");

    g_consoleLoggingEnabled = GetPrivateProfileIntA("Logging", "EnableConsole", 0, path) != 0;
    g_fileLoggingEnabled = GetPrivateProfileIntA("Logging", "EnableFile", 0, path) != 0;
    g_lockMouseToWindow = GetPrivateProfileIntA("System", "LockMouseToWindow", 0, path) != 0;
    g_disableMouseInputWhenUiOpen = GetPrivateProfileIntA("System", "DisableMouseInputWhenUiOpen", 0, path) != 0;
    g_disableKeyboardInputWhenUiOpen = GetPrivateProfileIntA("System", "DisableKeyboardInputWhenUiOpen", 0, path) != 0;
    g_disableHotkeysWhileUiOpen = GetPrivateProfileIntA("System", "DisableHotkeysWhileUiOpen", 0, path) != 0;
    g_menuPos.x = ReadIniFloat("UI", "WindowPosX", 60.0f, path);
    g_menuPos.y = ReadIniFloat("UI", "WindowPosY", 40.0f, path);
    g_menuSize.x = ReadIniFloat("UI", "WindowSizeX", 680.0f, path);
    g_menuSize.y = ReadIniFloat("UI", "WindowSizeY", 420.0f, path);
    g_playerSuperSpeedMultiplier = ReadIniFloat("Game", "PlayerSuperSpeed", 2.75f, path);
    if (g_playerSuperSpeedMultiplier < 0.0001f) {
        g_playerSuperSpeedMultiplier = 0.0001f;
    }
    if (g_playerSuperSpeedMultiplier > 100.0f) {
        g_playerSuperSpeedMultiplier = 100.0f;
    }
    g_playerSuperJumpDistance = ReadIniFloat("Game", "PlayerSuperJumpDistance", 8.0f, path);
    if (g_playerSuperJumpDistance < 0.0001f) {
        g_playerSuperJumpDistance = 0.0001f;
    }
    if (g_playerSuperJumpDistance > 100.0f) {
        g_playerSuperJumpDistance = 100.0f;
    }
    g_playerSuperJumpHeight = ReadIniFloat("Game", "PlayerSuperJumpHeight", -3.0f, path);
    if (g_playerSuperJumpHeight < -100.0f) {
        g_playerSuperJumpHeight = -100.0f;
    }
    if (g_playerSuperJumpHeight > 100.0f) {
        g_playerSuperJumpHeight = 100.0f;
    }
    g_noclipSpeed = ReadIniFloat("Noclip", "Speed", 1.0f, path);
    if (g_noclipSpeed < 0.0001f) {
        g_noclipSpeed = 0.0001f;
    }
    if (g_noclipSpeed > 1000.0f) {
        g_noclipSpeed = 1000.0f;
    }
    g_noclipBoostSpeed = ReadIniFloat("Noclip", "BoostSpeed", 5.0f, path);
    if (g_noclipBoostSpeed < 0.0001f) {
        g_noclipBoostSpeed = 0.0001f;
    }
    if (g_noclipBoostSpeed > 1000.0f) {
        g_noclipBoostSpeed = 1000.0f;
    }

    g_menuHotkey = GetPrivateProfileIntA("Hotkeys", "MenuOpen", 'B', path);
    for (int i = 0; i < kActionCount; ++i) {
        g_actions[i].hotkey = GetPrivateProfileIntA("Hotkeys", g_actions[i].id, 0, path);
    }
}

void SaveConfig() {
    char path[MAX_PATH]{};
    BuildModulePath(path, sizeof(path), "AC5Tools.ini");
    char value[64]{};

    WritePrivateProfileStringA("Logging", "EnableConsole", g_consoleLoggingEnabled ? "1" : "0", path);
    WritePrivateProfileStringA("Logging", "EnableFile", g_fileLoggingEnabled ? "1" : "0", path);
    WritePrivateProfileStringA("System", "LockMouseToWindow", g_lockMouseToWindow ? "1" : "0", path);
    WritePrivateProfileStringA("System", "DisableMouseInputWhenUiOpen", g_disableMouseInputWhenUiOpen ? "1" : "0", path);
    WritePrivateProfileStringA("System", "DisableKeyboardInputWhenUiOpen", g_disableKeyboardInputWhenUiOpen ? "1" : "0", path);
    WritePrivateProfileStringA("System", "DisableHotkeysWhileUiOpen", g_disableHotkeysWhileUiOpen ? "1" : "0", path);

    sprintf_s(value, "%d", static_cast<int>(g_menuPos.x));
    WritePrivateProfileStringA("UI", "WindowPosX", value, path);
    sprintf_s(value, "%d", static_cast<int>(g_menuPos.y));
    WritePrivateProfileStringA("UI", "WindowPosY", value, path);
    sprintf_s(value, "%d", static_cast<int>(g_menuSize.x));
    WritePrivateProfileStringA("UI", "WindowSizeX", value, path);
    sprintf_s(value, "%d", static_cast<int>(g_menuSize.y));
    WritePrivateProfileStringA("UI", "WindowSizeY", value, path);

    sprintf_s(value, "%.6f", g_playerSuperSpeedMultiplier);
    WritePrivateProfileStringA("Game", "PlayerSuperSpeed", value, path);
    sprintf_s(value, "%.6f", g_playerSuperJumpDistance);
    WritePrivateProfileStringA("Game", "PlayerSuperJumpDistance", value, path);
    sprintf_s(value, "%.6f", g_playerSuperJumpHeight);
    WritePrivateProfileStringA("Game", "PlayerSuperJumpHeight", value, path);
    sprintf_s(value, "%.6f", g_noclipSpeed);
    WritePrivateProfileStringA("Noclip", "Speed", value, path);
    sprintf_s(value, "%.6f", g_noclipBoostSpeed);
    WritePrivateProfileStringA("Noclip", "BoostSpeed", value, path);

    sprintf_s(value, "%d", g_menuHotkey);
    WritePrivateProfileStringA("Hotkeys", "MenuOpen", value, path);
    for (int i = 0; i < kActionCount; ++i) {
        sprintf_s(value, "%d", g_actions[i].hotkey);
        WritePrivateProfileStringA("Hotkeys", g_actions[i].id, value, path);
    }
}

bool IsMouseVirtualKey(int vk) {
    return vk >= VK_LBUTTON && vk <= VK_XBUTTON2;
}

bool IsBindableHotkey(int vk) {
    if (vk == 0) {
        return true;
    }
    if (IsMouseVirtualKey(vk)) {
        return false;
    }
    return vk >= VK_BACK && vk <= VK_F24;
}

const char* HotkeyName(int vk) {
    static char names[8][64]{};
    static int index = 0;
    char* out = names[index++ % 8];
    if (vk == 0) {
        strcpy_s(out, 64, "None");
        return out;
    }
    switch (vk) {
        case VK_SHIFT: strcpy_s(out, 64, "Shift"); return out;
        case VK_CONTROL: strcpy_s(out, 64, "Ctrl"); return out;
        case VK_MENU: strcpy_s(out, 64, "Alt"); return out;
        case VK_SPACE: strcpy_s(out, 64, "Space"); return out;
        case VK_ESCAPE: strcpy_s(out, 64, "Esc"); return out;
        case VK_RETURN: strcpy_s(out, 64, "Enter"); return out;
        case VK_BACK: strcpy_s(out, 64, "Backspace"); return out;
        case VK_DELETE: strcpy_s(out, 64, "Delete"); return out;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        sprintf_s(out, 64, "F%d", vk - VK_F1 + 1);
        return out;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        sprintf_s(out, 64, "Num%d", vk - VK_NUMPAD0);
        return out;
    }
    UINT scanCode = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    LONG lparam = static_cast<LONG>(scanCode << 16);
    if (GetKeyNameTextA(lparam, out, 64) > 0) {
        return out;
    }
    sprintf_s(out, 64, "VK_%02X", vk);
    return out;
}

SHORT QueryPhysicalKeyState(int vk) {
    if (g_originalGetAsyncKeyState) {
        return g_originalGetAsyncKeyState(vk);
    }
    return GetAsyncKeyState(vk);
}

void AssignMenuHotkey(int vk) {
    if (!IsBindableHotkey(vk)) {
        return;
    }
    g_menuHotkey = vk;
    g_suppressedHotkeyVk = vk;
    SaveConfig();
    Logf("Hotkey for Open/Close Menu set to %s.", HotkeyName(vk));
}

void AssignHotkey(int actionIndex, int vk) {
    if (actionIndex < 0 || actionIndex >= kActionCount || !IsBindableHotkey(vk)) {
        return;
    }
    if (vk != 0 && g_menuHotkey == vk) {
        g_menuHotkey = 0;
    }
    g_actions[actionIndex].hotkey = vk;
    g_suppressedHotkeyVk = vk;
    SaveConfig();
    Logf("Hotkey for %s set to %s.", g_actions[actionIndex].label, HotkeyName(vk));
}

bool ValidateNativeGhostFunction(std::uintptr_t rva,
                                 const std::uint8_t* expected,
                                 std::size_t size,
                                 const char* label) {
    if (!g_mainModuleBase || rva + size > g_mainModuleSize) {
        LogErrorf("Noclip native unavailable: %s address is out of range.", label);
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_mainModuleBase + rva);
    if (!BytesMatch(target, expected, size)) {
        LogPatchMismatch(label, target, expected, size);
        return false;
    }
    return true;
}

bool RefreshNativeGhost() {
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        g_nativeGhostReady = false;
        g_nativeGhostAddress = 0;
        return false;
    }

    const bool ok =
        ValidateNativeGhostFunction(kNativeGhostRootRva,
                                    kNativeGhostRootBytes,
                                    sizeof(kNativeGhostRootBytes),
                                    "Noclip native root") &&
        ValidateNativeGhostFunction(kNativeGhostPlayerRva,
                                    kNativeGhostPlayerBytes,
                                    sizeof(kNativeGhostPlayerBytes),
                                    "Noclip native player") &&
        ValidateNativeGhostFunction(kNativeGhostResolveRva,
                                    kNativeGhostResolveBytes,
                                    sizeof(kNativeGhostResolveBytes),
                                    "Noclip native resolver") &&
        ValidateNativeGhostFunction(kNativeGhostStateObjectRva,
                                    kNativeGhostStateObjectBytes,
                                    sizeof(kNativeGhostStateObjectBytes),
                                    "Noclip native state object") &&
        ValidateNativeGhostFunction(kNativeGhostStateValueRva,
                                    kNativeGhostStateValueBytes,
                                    sizeof(kNativeGhostStateValueBytes),
                                    "Noclip native state value") &&
        ValidateNativeGhostFunction(kNativeGhostEnableRva,
                                    kNativeGhostEnableBytes,
                                    sizeof(kNativeGhostEnableBytes),
                                    "Noclip native enable") &&
        ValidateNativeGhostFunction(kNativeGhostDisableRva,
                                    kNativeGhostDisableBytes,
                                    sizeof(kNativeGhostDisableBytes),
                                    "Noclip native disable");

    g_nativeGhostAddress = g_mainModuleBase + kNativeGhostEnableRva;
    g_nativeGhostReady = ok;
    return ok;
}

bool RefreshNoclipReady() {
    const bool nativeReady = RefreshNativeGhost();
    g_actions[kActionNoclip].ready = nativeReady;
    if (!nativeReady) {
        g_noclip = false;
    }
    return nativeReady;
}

bool ApplyNoclipSpeedSettings() {
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    bool ok = false;
    __try {
        auto* tablePtr = reinterpret_cast<std::uintptr_t*>(g_mainModuleBase + kNativeGhostSpeedTablePtrRva);
        g_nativeGhostSpeedTable = tablePtr ? *tablePtr : 0;
        if (g_nativeGhostSpeedTable) {
            CaptureNoclipSpeedDefaults(g_nativeGhostSpeedTable);
            // Directional ghost speeds are grouped in the engine table. Keep the sentinel at +0x84 intact.
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x68) = g_noclipSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x6C) = g_noclipSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x74) = g_noclipSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x78) = g_noclipSpeed;

            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x70) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x7C) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x80) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x88) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x8C) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0xA8) = g_noclipBoostSpeed;
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0xAC) = g_noclipBoostSpeed;
            ok = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_nativeGhostSpeedTable = 0;
        ok = false;
    }

    if (!ok) {
        LogWarning("Noclip speed update skipped: engine speed table was not ready.");
    }
    return ok;
}

void CaptureNoclipSpeedDefaults(std::uintptr_t tableAddress) {
    if (!tableAddress || g_nativeGhostSpeedDefaultsCaptured) {
        return;
    }
    g_nativeGhostSpeedDefaults[0] = *reinterpret_cast<float*>(tableAddress + 0x68);
    g_nativeGhostSpeedDefaults[1] = *reinterpret_cast<float*>(tableAddress + 0x6C);
    g_nativeGhostSpeedDefaults[2] = *reinterpret_cast<float*>(tableAddress + 0x74);
    g_nativeGhostSpeedDefaults[3] = *reinterpret_cast<float*>(tableAddress + 0x78);
    g_nativeGhostSpeedDefaults[4] = *reinterpret_cast<float*>(tableAddress + 0x70);
    g_nativeGhostSpeedDefaults[5] = *reinterpret_cast<float*>(tableAddress + 0x7C);
    g_nativeGhostSpeedDefaults[6] = *reinterpret_cast<float*>(tableAddress + 0x80);
    g_nativeGhostSpeedDefaults[7] = *reinterpret_cast<float*>(tableAddress + 0x88);
    g_nativeGhostSpeedDefaults[8] = *reinterpret_cast<float*>(tableAddress + 0x8C);
    g_nativeGhostSpeedDefaults[9] = *reinterpret_cast<float*>(tableAddress + 0xA8);
    g_nativeGhostSpeedDefaults[10] = *reinterpret_cast<float*>(tableAddress + 0xAC);
    g_nativeGhostSpeedDefaultsCaptured = true;
}

bool RestoreNoclipSpeedDefaults() {
    if (!g_nativeGhostSpeedDefaultsCaptured) {
        return true;
    }
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    bool ok = false;
    __try {
        auto* tablePtr = reinterpret_cast<std::uintptr_t*>(g_mainModuleBase + kNativeGhostSpeedTablePtrRva);
        g_nativeGhostSpeedTable = tablePtr ? *tablePtr : 0;
        if (g_nativeGhostSpeedTable) {
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x68) = g_nativeGhostSpeedDefaults[0];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x6C) = g_nativeGhostSpeedDefaults[1];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x74) = g_nativeGhostSpeedDefaults[2];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x78) = g_nativeGhostSpeedDefaults[3];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x70) = g_nativeGhostSpeedDefaults[4];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x7C) = g_nativeGhostSpeedDefaults[5];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x80) = g_nativeGhostSpeedDefaults[6];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x88) = g_nativeGhostSpeedDefaults[7];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0x8C) = g_nativeGhostSpeedDefaults[8];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0xA8) = g_nativeGhostSpeedDefaults[9];
            *reinterpret_cast<float*>(g_nativeGhostSpeedTable + 0xAC) = g_nativeGhostSpeedDefaults[10];
            ok = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    if (!ok) {
        LogWarning("Noclip speed restore skipped: engine speed table was not ready.");
    }
    return ok;
}

bool SetNativeGhostRequested(bool enabled, const char* source) {
    if (!RefreshNativeGhost()) {
        return false;
    }

    using RootFn = void*(*)();
    using ResolveFn = void*(*)(void*);
    using PtrFn = void*(*)(void*);
    using StateFn = int(*)(void*, int);
    using EnableFn = void(*)(void*, bool);
    using DisableFn = void(*)(void*);

    bool ok = false;
    int state = -1;
    const char* failedStep = "unknown";
    void* root = nullptr;
    void* player = nullptr;
    void* stateObject = nullptr;
    void* ghost = nullptr;
    __try {
        failedStep = "root";
        root = reinterpret_cast<RootFn>(g_mainModuleBase + kNativeGhostRootRva)();
        failedStep = "player";
        player = root ? reinterpret_cast<ResolveFn>(g_mainModuleBase + kNativeGhostPlayerRva)(root) : nullptr;
        failedStep = "state object";
        stateObject = player ? reinterpret_cast<PtrFn>(g_mainModuleBase + kNativeGhostStateObjectRva)(player) : nullptr;
        if (stateObject) {
            failedStep = "state value";
            state = reinterpret_cast<StateFn>(g_mainModuleBase + kNativeGhostStateValueRva)(stateObject, 1);
        }
        failedStep = "ghost";
        ghost = player ? reinterpret_cast<ResolveFn>(g_mainModuleBase + kNativeGhostResolveRva)(player) : nullptr;
        if (ghost) {
            failedStep = "transition";
            if (enabled && state != 3) {
                ApplyNoclipSpeedSettings();
                reinterpret_cast<EnableFn>(g_mainModuleBase + kNativeGhostEnableRva)(ghost, false);
            } else if (!enabled && state == 3) {
                reinterpret_cast<DisableFn>(g_mainModuleBase + kNativeGhostDisableRva)(ghost);
            }
            if (!enabled) {
                RestoreNoclipSpeedDefaults();
            }
            ok = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    if (!ok) {
    LogErrorf("Noclip native toggle failed at %s. root=0x%p player=0x%p stateObject=0x%p ghost=0x%p state=%d",
             failedStep,
             root,
             player,
             stateObject,
             ghost,
             state);
        return false;
    }

    InterlockedIncrement(&g_nativeGhostCalls);
    g_noclip = enabled;
    Logf("Noclip %s from %s. state=%d speed=%.6f boost=%.6f",
         g_noclip ? "requested ON" : "requested OFF",
         source,
         state,
         g_noclipSpeed,
         g_noclipBoostSpeed);
    return true;
}

bool SetEngineGhostRequested(bool enabled, const char* source) {
    return SetNativeGhostRequested(enabled, source);
}

void ToggleActionFromHotkey(int actionIndex) {
    if (actionIndex < 0 || actionIndex >= kActionCount) {
        return;
    }
    ToggleAction& action = g_actions[actionIndex];
    if (!action.ready) {
        return;
    }
    if (actionIndex == kActionNoclip) {
        SetEngineGhostRequested(!g_noclip, "hotkey");
        return;
    }
    if (actionIndex == kActionDesynchronizeYourself) {
        DesynchronizeYourself();
        return;
    }
    if (action.value) {
        *action.value = !*action.value;
    }
    if (actionIndex == kActionPlayerGodmode) {
        ApplyPlayerGodmode();
    } else if (actionIndex == kActionStealthMode) {
        ApplyStealthMode();
    } else if (actionIndex == kActionShipGodmode ||
               actionIndex == kActionNoCannonCooldown ||
               actionIndex == kActionAllyGodmode ||
               actionIndex == kActionShipStealthMode) {
        ApplyShipOptions();
    } else if (actionIndex == kActionUnlimitedResources ||
               actionIndex == kActionUnlimitedSelling ||
               actionIndex == kActionHarpoonGodmode) {
        ApplyBytePatchToggles();
    } else if (actionIndex == kActionFreezeMissionTimer) {
        ResetMissionTimerDeltas();
    }
    Logf("%s triggered from hotkey %s.", action.label, HotkeyName(action.hotkey));
}

void ProcessHotkeys() {
    if ((g_menuOpen && g_disableHotkeysWhileUiOpen) || GetForegroundWindow() != g_gameWindow) {
        return;
    }
    RefreshNoclipReady();
    for (int i = 0; i < kActionCount; ++i) {
        const int vk = g_actions[i].hotkey;
        if (vk == 0 || g_suppressedHotkeyVk == vk) {
            continue;
        }
        if ((QueryPhysicalKeyState(vk) & 0x8000) != 0) {
            ToggleActionFromHotkey(i);
            g_suppressedHotkeyVk = vk;
        }
    }
}

void SetGameInputCaptured(bool captured) {
    if (g_inputCaptured == captured) {
        return;
    }
    g_inputCaptured = captured;
    ClipCursor(nullptr);
}

bool IsMouseInputCaptureActive() {
    return g_menuOpen && g_disableMouseInputWhenUiOpen;
}

bool IsKeyboardInputCaptureActive() {
    return g_menuOpen && g_disableKeyboardInputWhenUiOpen;
}

void RefreshInputCaptureState() {
    SetGameInputCaptured(IsMouseInputCaptureActive() || IsKeyboardInputCaptureActive());
}

void ReleaseMenuInputState() {
    g_hotkeyCaptureAction = -1;
    SetGameInputCaptured(false);
    ClipCursor(nullptr);
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
}

bool ShouldBlockPolledKeyboardKey(int vk) {
    if (!IsKeyboardInputCaptureActive()) {
        return false;
    }
    if (vk == g_menuHotkey || IsMouseVirtualKey(vk)) {
        return false;
    }
    return true;
}

bool IsMouseDevice(IDirectInputDevice8A* device) {
    if (!device) {
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    return vtable[9] == g_mouseGetDeviceStateTarget || vtable[10] == g_mouseGetDeviceDataTarget;
}

bool IsKeyboardDevice(IDirectInputDevice8A* device) {
    if (!device) {
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    return vtable[9] == g_keyboardGetDeviceStateTarget || vtable[10] == g_keyboardGetDeviceDataTarget;
}

void UpdateMouseWindowLock() {
    if (!g_lockMouseToWindow || !g_gameWindow || GetForegroundWindow() != g_gameWindow) {
        ClipCursor(nullptr);
        return;
    }

    RECT rect{};
    if (!GetClientRect(g_gameWindow, &rect)) {
        ClipCursor(nullptr);
        return;
    }

    POINT topLeft{rect.left, rect.top};
    POINT bottomRight{rect.right, rect.bottom};
    if (!ClientToScreen(g_gameWindow, &topLeft) ||
        !ClientToScreen(g_gameWindow, &bottomRight)) {
        ClipCursor(nullptr);
        return;
    }

    RECT clip{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&clip);
}

float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

ImVec2 GameWindowClientSize() {
    RECT rect{};
    if (!g_gameWindow || !GetClientRect(g_gameWindow, &rect)) {
        return ImVec2(0.0f, 0.0f);
    }
    return ImVec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));
}

ImVec2 ClampMenuPosToGameWindow(ImVec2 pos, ImVec2 size) {
    const ImVec2 clientSize = GameWindowClientSize();
    if (clientSize.x <= 0.0f || clientSize.y <= 0.0f) {
        return pos;
    }
    pos.x = ClampFloat(pos.x, 0.0f, clientSize.x > size.x ? clientSize.x - size.x : 0.0f);
    pos.y = ClampFloat(pos.y, 0.0f, clientSize.y > size.y ? clientSize.y - size.y : 0.0f);
    return pos;
}

void ApplyRedStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.44f, 0.44f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.05f, 0.05f, 0.88f);
    colors[ImGuiCol_Border] = ImVec4(0.78f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.65f, 0.12f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.34f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.55f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.10f, 0.16f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.32f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.48f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.72f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.32f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.62f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.48f, 0.12f, 0.16f, 1.00f);
}

void CreateRenderTarget(IDXGISwapChain* swapChain) {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
        backBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_renderTarget) {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }
}

void DrawInfoRow(const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value);
}

void DrawInfoRowf(const char* label, const char* format, ...) {
    char value[256]{};
    va_list args;
    va_start(args, format);
    vsprintf_s(value, format, args);
    va_end(args);
    DrawInfoRow(label, value);
}

int CountEnabledUnlocks() {
    int count = 0;
    for (int i = 0; i < kUnlockEntryCount; ++i) {
        if (g_unlockEntries[i].enabled) {
            ++count;
        }
    }
    return count;
}

const char* UnlockCategoryName(UnlockCategory category) {
    switch (category) {
        case UnlockCategory::Swords: return "Swords";
        case UnlockCategory::Pistols: return "Pistols";
        case UnlockCategory::Outfits: return "Outfits and Extras";
        case UnlockCategory::ShipAppearance: return "Ship Appearance";
        case UnlockCategory::Blueprints: return "Blueprints";
        case UnlockCategory::Extras: return "Extra Content";
    }
    return "Unlocks";
}

void DrawUnlockCategory(UnlockCategory category) {
    ImGui::TextUnformatted(UnlockCategoryName(category));
    if (ImGui::BeginTable(UnlockCategoryName(category), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        for (int i = 0; i < kUnlockEntryCount; ++i) {
            UnlockEntry& entry = g_unlockEntries[i];
            if (entry.category != category) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            ImGui::Checkbox(entry.name, &entry.enabled);
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            if (entry.patched) {
                ImGui::TextDisabled("done");
            } else if (entry.enabled && entry.found) {
                ImGui::TextDisabled("found");
            } else if (entry.enabled) {
                ImGui::TextDisabled("waiting");
            } else {
                ImGui::TextDisabled("ready");
            }
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
}

void DrawUnlocksTab() {
    ImGui::TextDisabled("Back up your save first. Unlock changes can become permanent once the game saves.");
    ImGui::TextDisabled("Use broad unlocks at the main menu before loading a save; it stays active until the game exits.");
    ImGui::Separator();

    ImGui::TextUnformatted("Global unlocks");
    if (g_globalUnlocksInstalled) {
        ImGui::TextDisabled("Global Unlocks installed for this session.");
    } else if (ImGui::Button("Install Global Unlocks")) {
        InstallGlobalUnlocksPatch();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Single unlocks");
    ImGui::TextWrapped("Check entries to queue them. They scan in the background and auto-clear after a successful apply.");
    DrawUnlockCategory(UnlockCategory::Swords);
    DrawUnlockCategory(UnlockCategory::Pistols);
    DrawUnlockCategory(UnlockCategory::Outfits);
    DrawUnlockCategory(UnlockCategory::ShipAppearance);
    DrawUnlockCategory(UnlockCategory::Blueprints);
    DrawUnlockCategory(UnlockCategory::Extras);

    ImGui::Text("Pending: %d", CountEnabledUnlocks());
    ImGui::Text("Done this session: %d", g_unlockRecordsPatched);
    if (g_unlockScanRunning != 0) {
        ImGui::TextDisabled("Scan running in background...");
    } else if (CountEnabledUnlocks() > 0) {
        ImGui::TextDisabled("If an item stays waiting, load the main menu or a relevant inventory/shop screen.");
    }
}

bool TryReadAnimusAllowSaving(bool& allowSaving) {
    if (!g_animusCheatMgr) {
        return false;
    }
    std::uint8_t disabled = 0;
    if (!TryReadByte(g_animusCheatMgr + 0x3E0, disabled)) {
        return false;
    }
    allowSaving = disabled == 0;
    return true;
}

bool TryWriteAnimusAllowSaving(bool allowSaving) {
    if (!g_animusCheatMgr) {
        return false;
    }
    const std::uint8_t disabled = allowSaving ? 0 : 1;
    return TryWriteByte(g_animusCheatMgr + 0x3E0, disabled);
}

bool InstallAnimusChallengeUnlockPatch() {
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }
    if (!g_animusChallengeUnlockAddress) {
        const auto baseAddress = FindMainModulePatternUnique(kAnimusChallengeUnlockPattern,
                                                             sizeof(kAnimusChallengeUnlockPattern),
                                                             "Animus Hacks challenge unlock patch");
        if (!baseAddress) {
            return false;
        }
        g_animusChallengeUnlockAddress = baseAddress + kAnimusChallengeUnlockPatchOffset;
    }

    auto* target = reinterpret_cast<std::uint8_t*>(g_animusChallengeUnlockAddress);
    if (BytesMatch(target, kEnabledAnimusChallengeUnlockBytes, sizeof(kEnabledAnimusChallengeUnlockBytes))) {
        return true;
    }
    if (!BytesMatch(target, kOriginalAnimusChallengeUnlockBytes, sizeof(kOriginalAnimusChallengeUnlockBytes))) {
        LogPatchMismatch("Animus Hacks challenge unlock patch",
                         target,
                         kOriginalAnimusChallengeUnlockBytes,
                         sizeof(kOriginalAnimusChallengeUnlockBytes));
        return false;
    }
    if (!WritePatchedBytes(g_animusChallengeUnlockAddress,
                           kEnabledAnimusChallengeUnlockBytes,
                           sizeof(kEnabledAnimusChallengeUnlockBytes),
                           "Animus Hacks challenge unlock patch")) {
        return false;
    }
    Logf("Installed Animus Hacks challenge unlock patch at 0x%p.", target);
    return true;
}

bool InstallCompleteAllChallengesPatch() {
    if (g_originalCompleteAllChallenges) {
        return true;
    }
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    g_completeAllChallengesAddress = FindMainModulePatternUnique(kCompleteAllChallengesPattern,
                                                                 sizeof(kCompleteAllChallengesPattern),
                                                                 "Complete All Challenges hook");
    if (!g_completeAllChallengesAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_completeAllChallengesAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing Complete All Challenges hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalCompleteAllChallengesBytes, sizeof(kOriginalCompleteAllChallengesBytes))) {
        LogPatchMismatch("Complete All Challenges hook",
                         target,
                         kOriginalCompleteAllChallengesBytes,
                         sizeof(kOriginalCompleteAllChallengesBytes));
        return false;
    }

    g_completeAllChallengesCave = BuildCompleteAllChallengesCave();
    if (!g_completeAllChallengesCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for Complete All Challenges hook.");
        return false;
    }
    if (MH_CreateHook(target, g_completeAllChallengesCave, &g_originalCompleteAllChallenges) != MH_OK) {
        LogError("MinHook CreateHook failed for Complete All Challenges hook.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for Complete All Challenges hook.");
        return false;
    }
    Logf("Installed Complete All Challenges hook at 0x%p.", target);
    return true;
}

bool UnlockAllAnimusHacks() {
    if (!g_animusCheatMgr) {
        return false;
    }

    __try {
        for (int i = 0; i < 14; ++i) {
            *reinterpret_cast<int*>(g_animusCheatMgr + 0x1F8 + static_cast<std::uintptr_t>(i) * 0x20) = 1;
        }
        InstallAnimusChallengeUnlockPatch();
        Log("Unlocked all in-game Animus Hacks.");
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogWarning("Unlock All Animus Hacks skipped after invalid cheat manager pointer access.");
        g_animusCheatMgr = 0;
        return false;
    }
}

void DrawAnimusHacksTab() {
    ImGui::TextWrapped("Install the save patch first, then return to gameplay briefly if the manager pointer is still waiting.");
    if (!g_animusHackInstalled) {
        if (ImGui::Button("Enable Animus Hacks Save Patch")) {
            InstallAnimusHackPatch();
        }
    } else {
        ImGui::TextDisabled("Animus Hacks Save Patch installed.");
    }

    const bool ready = g_animusHackInstalled && g_animusCheatMgr != 0;
    if (!ready) {
        ImGui::TextDisabled("Waiting for cheat manager pointer.");
    }

    if (!g_originalCompleteAllChallenges) {
        if (ImGui::Button("Complete All Challenges")) {
            InstallCompleteAllChallengesPatch();
        }
    } else {
        ImGui::TextDisabled("Complete All Challenges hook installed.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Session hook: challenge records are completed when the game reads them. Return to the main menu to save changes.");
    }

    if (!ready) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Unlock All In-Game Cheats")) {
        UnlockAllAnimusHacks();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Marks the built-in in-game cheat entries as unlocked.");
    }

    bool allowSaving = false;
    const bool haveSavingState = TryReadAnimusAllowSaving(allowSaving);
    if (!haveSavingState) {
        allowSaving = true;
    }
    if (ImGui::Checkbox("Allow Saving", &allowSaving)) {
        if (TryWriteAnimusAllowSaving(allowSaving)) {
            Log(allowSaving ? "Animus Hacks saving allowed." : "Animus Hacks saving disabled.");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Writes the in-game cheat-manager save-disable flag. Checked means saving is allowed.");
    }

    if (!ready) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::Text("Cheat manager: 0x%p", reinterpret_cast<void*>(g_animusCheatMgr));
    ImGui::Text("Hook hits: %ld", g_animusHackHits);
}

void DrawHotkeysTab() {
    if (ImGui::BeginTable("HotkeyTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Hotkey", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Set", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Open/Close Menu");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("Menu");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_hotkeyCaptureAction == kMenuHotkeyCapture ? "Press key..." : HotkeyName(g_menuHotkey));
        ImGui::TableSetColumnIndex(3);
        ImGui::PushID("MenuOpen");
        if (ImGui::Button(g_hotkeyCaptureAction == kMenuHotkeyCapture ? "Cancel" : "Set", ImVec2(64.0f, 0.0f))) {
            g_hotkeyCaptureAction = g_hotkeyCaptureAction == kMenuHotkeyCapture ? -1 : kMenuHotkeyCapture;
        }
        ImGui::SameLine();
        if (ImGui::Button("None", ImVec2(64.0f, 0.0f))) {
            AssignMenuHotkey(0);
            if (g_hotkeyCaptureAction == kMenuHotkeyCapture) {
                g_hotkeyCaptureAction = -1;
            }
        }
        ImGui::PopID();

        for (int i = 0; i < kActionCount; ++i) {
            ToggleAction& action = g_actions[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (action.ready) {
                ImGui::TextUnformatted(action.label);
            } else {
                ImGui::TextDisabled("%s", action.label);
            }
            ImGui::TableSetColumnIndex(1);
            if (!action.ready) {
                ImGui::TextDisabled("Unavailable");
            } else if (!action.value) {
                ImGui::TextUnformatted("Action");
            } else {
                ImGui::TextUnformatted(*action.value ? "On" : "Off");
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(g_hotkeyCaptureAction == i ? "Press key..." : HotkeyName(action.hotkey));
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(i);
            if (ImGui::Button(g_hotkeyCaptureAction == i ? "Cancel" : "Set", ImVec2(64.0f, 0.0f))) {
                g_hotkeyCaptureAction = g_hotkeyCaptureAction == i ? -1 : i;
            }
            ImGui::SameLine();
            if (ImGui::Button("None", ImVec2(64.0f, 0.0f))) {
                AssignHotkey(i, 0);
                if (g_hotkeyCaptureAction == i) {
                    g_hotkeyCaptureAction = -1;
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

}

void DrawSystemTab() {
    if (ImGui::Checkbox("Lock Mouse to Window", &g_lockMouseToWindow)) {
        SaveConfig();
        Log(g_lockMouseToWindow ? "Lock Mouse to Window enabled." : "Lock Mouse to Window disabled.");
    }
    if (ImGui::Checkbox("Disable Mouse Input while UI is open", &g_disableMouseInputWhenUiOpen)) {
        SaveConfig();
        RefreshInputCaptureState();
        Log(g_disableMouseInputWhenUiOpen ? "Disable Mouse Input while UI is open enabled." :
                                            "Disable Mouse Input while UI is open disabled.");
    }
    if (ImGui::Checkbox("Disable Keyboard Input while UI is open", &g_disableKeyboardInputWhenUiOpen)) {
        SaveConfig();
        RefreshInputCaptureState();
        Log(g_disableKeyboardInputWhenUiOpen ? "Disable Keyboard Input while UI is open enabled." :
                                               "Disable Keyboard Input while UI is open disabled.");
    }
    if (ImGui::Checkbox("Disable Hotkeys while UI is open", &g_disableHotkeysWhileUiOpen)) {
        SaveConfig();
        Log(g_disableHotkeysWhileUiOpen ? "Disable Hotkeys while UI is open enabled." :
                                          "Disable Hotkeys while UI is open disabled.");
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("SystemInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        DrawInfoRow("Tool", kToolName);
        DrawInfoRow("Version", kToolVersion);
        DrawInfoRow("Process", g_gameExePath[0] ? g_gameExePath : "unknown");
        DrawInfoRow("Supported exe", kSupportedGameExe);
        DrawInfoRowf("Supported exe size", "%llu bytes", kSupportedGameExeSize);
        DrawInfoRow("Supported SHA256", kSupportedGameExeSha256);
        DrawInfoRowf("Detected exe size", "%llu bytes", g_gameExeSize);
        DrawInfoRow("Detected SHA256", g_gameExeSha256);
        DrawInfoRow("Detected timestamp", g_gameTimestampText);
        DrawInfoRow("Patch mode", "guarded hooks");
        DrawInfoRow("GclShip hook", g_shipPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Player pointer hook", g_playerPointerPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Debug context hook", g_debugContextPatchReady ? "installed" : "unavailable");
        DrawInfoRow("No Fall Damage hook", g_noFallDamagePatchReady ? "installed" : "unavailable");
        DrawInfoRow("No Reload hook", g_noReloadPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Lock Consumables hooks", g_lockConsumablesPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Inventory edit hooks", g_inventoryEditPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Animus Hacks Save Patch", g_animusHackInstalled ? "installed" : "not installed");
        DrawInfoRow("Complete All Challenges", g_originalCompleteAllChallenges ? "installed" : "not installed");
        DrawInfoRow("Unlimited Resources patch", g_unlimitedResourcesPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Unlimited Selling patch", g_unlimitedSellingPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Harpoon Godmode patches", g_harpoonGodmodePatchReady ? "installed" : "unavailable");
        DrawInfoRow("Freeze Mission Timer hooks", g_freezeMissionTimerPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Player Super Speed hook", g_playerSuperSpeedPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Player Super Jump hook", g_playerSuperJumpPatchReady ? "installed" : "unavailable");
        DrawInfoRow("Noclip native engine path", g_nativeGhostReady ? "ready" : "unavailable");
        DrawInfoRow("Global Unlocks", g_globalUnlocksInstalled ? "installed" : "not installed");
        DrawInfoRowf("Unlock queue", "%d pending", CountEnabledUnlocks());
        DrawInfoRowf("Unlocks done this session", "%d", g_unlockRecordsPatched);
        DrawInfoRowf("BhvAssassin", "0x%p", reinterpret_cast<void*>(g_playerBhvAssassin));
        DrawInfoRowf("GclShip", "0x%p", reinterpret_cast<void*>(g_gclShip));
        DrawInfoRowf("Friendly ship", "0x%p", reinterpret_cast<void*>(g_allyShip));
        DrawInfoRowf("Player entity", "0x%p", reinterpret_cast<void*>(g_playerEntity));
        DrawInfoRowf("Player health", "0x%p", reinterpret_cast<void*>(g_playerHealth));
        DrawInfoRowf("Debug context", "0x%p", reinterpret_cast<void*>(g_debugContext));
        DrawInfoRowf("Player inventory", "0x%p", reinterpret_cast<void*>(g_playerInventory));
        DrawInfoRowf("Inventory supplies", "0x%p", reinterpret_cast<void*>(g_inventorySupplies));
        DrawInfoRowf("Cheat manager", "0x%p", reinterpret_cast<void*>(g_animusCheatMgr));
        DrawInfoRow("Menu hotkey", HotkeyName(g_menuHotkey));
        DrawInfoRow("Console logging", g_consoleLoggingEnabled ? "enabled" : "disabled");
        DrawInfoRow("File logging", g_fileLoggingEnabled ? "enabled" : "disabled");
        DrawInfoRow("Mouse window lock", g_lockMouseToWindow ? "enabled" : "disabled");
        DrawInfoRow("Disable mouse input on UI open", g_disableMouseInputWhenUiOpen ? "enabled" : "disabled");
        DrawInfoRow("Disable keyboard input on UI open", g_disableKeyboardInputWhenUiOpen ? "enabled" : "disabled");
        DrawInfoRow("Disable hotkeys on UI open", g_disableHotkeysWhileUiOpen ? "enabled" : "disabled");
        DrawInfoRow("Input captured", g_inputCaptured ? "yes" : "no");
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("PatchStatusTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Patch");
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("GclShip");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_gclShipAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_shipPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_shipPointerHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Friendly Ship");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_allyShip));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_shipPatchReady ? "tracked" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_allyShipHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Player Pointer");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_bhvAssassinAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_playerPointerPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_playerPointerHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("No Fall Damage");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_fallDamageAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_noFallDamagePatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_fallDamageHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("No Reload");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_noReloadAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_noReloadPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_noReloadHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Lock Consumables Cargo");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_lockConsumables1Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_lockConsumables1Cave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_lockConsumables1Hits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Lock Consumables Ammo");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_lockConsumables2Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_lockConsumables2Cave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_lockConsumables2Hits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Inventory Supplies");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_inventorySuppliesAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_inventorySuppliesCave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_inventorySuppliesHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Inventory Update");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_inventoryUpdateAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_inventoryUpdateCave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_inventoryUpdateHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Inventory Cargo");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_inventoryCargoAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_inventoryCargoCave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_inventoryCargoHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Animus Hacks");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_animusHackAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_animusHackInstalled ? "ready" : "not installed");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_animusHackHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Mission Timer I");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_missionTimer1Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_missionTimer1Cave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_missionTimer1Hits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Mission Timer II");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_missionTimer2Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_missionTimer2Cave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_missionTimer2Hits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Mission Timer III");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_missionTimer3Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_missionTimer3Cave ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_missionTimer3Hits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Player Super Speed");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_playerSuperSpeedAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_playerSuperSpeedPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_playerSuperSpeedHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Player Super Jump");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_playerSuperJumpAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_playerSuperJumpPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_playerSuperJumpHits);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Noclip");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_nativeGhostSpeedTable ? g_nativeGhostSpeedTable : g_nativeGhostAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_nativeGhostReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_nativeGhostCalls);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Unlimited Resources");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_unlimitedResourcesAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_unlimitedResourcesPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Unlimited Selling");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_unlimitedSellingAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_unlimitedSellingPatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Harpoon Godmode I");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_harpoonGodmode1Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_harpoonGodmodePatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Harpoon Godmode II");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_harpoonGodmode2Address));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_harpoonGodmodePatchReady ? "ready" : "unavailable");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Global Unlocks Read");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_globalUnlockReadAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_globalUnlocksInstalled ? "ready" : "not installed");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Global Unlocks Write");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("0x%p", reinterpret_cast<void*>(g_globalUnlockWriteAddress));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(g_globalUnlocksInstalled ? "ready" : "not installed");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%ld", g_globalUnlockWriteHits);

        ImGui::EndTable();
    }
}

void DrawShipTab() {
    if (!g_shipPatchReady) {
        ImGui::TextDisabled("Ship options unavailable: GclShip hook was not installed.");
        return;
    }

    bool godmode = g_shipGodmode;
    if (ImGui::Checkbox("God Mode", &godmode)) {
        g_shipGodmode = godmode;
        ApplyShipOptions();
        Log(g_shipGodmode ? "Ship God Mode toggled ON from ImGui." :
                            "Ship God Mode toggled OFF from ImGui.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sets the ship god-mode flag.");
    }

    bool allyGodmode = g_allyGodmode;
    if (ImGui::Checkbox("Ally Godmode", &allyGodmode)) {
        g_allyGodmode = allyGodmode;
        ApplyShipOptions();
        Log(g_allyGodmode ? "Ally Godmode toggled ON from ImGui." :
                            "Ally Godmode toggled OFF from ImGui.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Tracks friendly ships by ship type.");
    }

    bool stealth = g_shipStealthMode;
    if (ImGui::Checkbox("Stealth Mode", &stealth)) {
        g_shipStealthMode = stealth;
        ApplyShipOptions();
        Log(g_shipStealthMode ? "Ship Stealth Mode toggled ON from ImGui." :
                                "Ship Stealth Mode toggled OFF from ImGui.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sets the ship visibility flag to the invisible state.");
    }

    bool instantReload = g_noCannonCooldown;
    if (ImGui::Checkbox("Instant Reload", &instantReload)) {
        g_noCannonCooldown = instantReload;
        ApplyShipOptions();
        Log(g_noCannonCooldown ? "Ship Instant Reload toggled ON from ImGui." :
                                 "Ship Instant Reload toggled OFF from ImGui.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sets broadside, heavy shot, mortar, and front carronade reload timers to zero while enabled.");
    }

    if (g_harpoonGodmodePatchReady) {
        bool value = g_harpoonGodmode;
        if (ImGui::Checkbox("Harpoon Godmode", &value)) {
            g_harpoonGodmode = value;
            ApplyBytePatchToggles();
            Log(g_harpoonGodmode ? "Harpoon Godmode toggled ON from ImGui." :
                                    "Harpoon Godmode toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Nops guarded harpoon damage subtraction while enabled.");
        }
    } else {
        ImGui::TextDisabled("Harpoon Godmode unavailable: patches were not installed.");
    }

    if (!g_gclShip) {
        ImGui::TextDisabled("Waiting for ship pointer; board/enter naval gameplay.");
    } else {
        __try {
            const float currentHealth = *reinterpret_cast<float*>(g_gclShip + 0x26C);
            const float maxHealth = *reinterpret_cast<float*>(g_gclShip + 0x268);
            const float calculatedHealth = *reinterpret_cast<float*>(g_gclShip + 0x7B0);
            ImGui::Text("Ship Health: %.1f / %.1f", currentHealth, maxHealth);
            ImGui::Text("Calculated Health: %.1f", calculatedHealth);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ImGui::TextDisabled("Ship health values unavailable for current pointer.");
        }
    }

    if (g_allyShip) {
        __try {
            const float allyHealth = *reinterpret_cast<float*>(g_allyShip + 0x26C);
            const float allyMaxHealth = *reinterpret_cast<float*>(g_allyShip + 0x268);
            ImGui::Text("Friendly Ship Health: %.1f / %.1f", allyHealth, allyMaxHealth);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ImGui::TextDisabled("Friendly ship health values unavailable for current pointer.");
        }
    }
}

const char* InventoryCategoryName(InventoryCategory category) {
    switch (category) {
        case InventoryCategory::Player: return "Player Items";
        case InventoryCategory::Ship: return "Ship Supplies";
        case InventoryCategory::Cargo: return "Cargo";
    }
    return "Inventory";
}

void DrawInventoryCategory(InventoryCategory category) {
    ImGui::TextUnformatted(InventoryCategoryName(category));
    if (ImGui::BeginTable(InventoryCategoryName(category),
                          5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Queue", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < kInventoryEntryCount; ++i) {
            InventoryEditEntry& entry = g_inventoryEntries[i];
            if (entry.category != category) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.name);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(i);
            if (ImGui::Checkbox("##queue", &entry.queued)) {
                UpdateInventoryEditQueuedFlag();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(entry.category == InventoryCategory::Cargo || entry.id == 0x01 ?
                                  "Queues one edit for the next matching store buy/sell update for this resource/cargo or money value. Gaining or looting usually will not trigger it." :
                                  "Queues one edit for the next matching spend/use/buy update. Selling, gaining, or looting this resource usually will not trigger it.");
            }

            ImGui::TableSetColumnIndex(2);
            const char* modes[] = {"Set", "Add"};
            int mode = entry.mode == InventoryEditMode::Add ? 1 : 0;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##mode", &mode, modes, 2)) {
                entry.mode = mode == 1 ? InventoryEditMode::Add : InventoryEditMode::Set;
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##amount", &entry.amount, 1, 10)) {
                entry.amount = ClampInventoryAmount(entry, entry.amount);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(entry.maxValue == 99 ? "Clamped to 0-99 for safer game inventory limits." :
                                                          "Money uses a larger safe cap.");
            }

            ImGui::TableSetColumnIndex(4);
            if (entry.lastApplied > 0) {
                ImGui::Text("%d", entry.lastApplied);
            } else if (entry.queued) {
                ImGui::TextDisabled("queued");
            } else {
                ImGui::TextDisabled("-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
}

void DrawInventoryEditor() {
    ImGui::Separator();
    ImGui::TextUnformatted("Inventory Add/Set");
    if (!g_inventoryEditPatchReady) {
        ImGui::TextDisabled("Inventory editor unavailable: guarded hooks were not installed.");
        return;
    }
    ImGui::TextWrapped("Queued edits apply on the next matching in-game inventory change, then auto-unqueue.");
    ImGui::TextDisabled("Triggered by matching spend/use/buy updates. Store resources/cargo and money can also trigger from buy/sell updates. Gaining or looting usually will not trigger queued edits.");
    if (!g_inventorySupplies) {
        ImGui::TextDisabled("Waiting for inventory pointer; load into gameplay as the player and spend/use something. You can queue changes now; they apply once the matching spend/use update runs.");
    }

    DrawInventoryCategory(InventoryCategory::Player);
    DrawInventoryCategory(InventoryCategory::Ship);
    DrawInventoryCategory(InventoryCategory::Cargo);
}

void DrawPlayerTab() {
    if (!g_playerPointerPatchReady && !g_noFallDamagePatchReady) {
        ImGui::TextDisabled("Player options unavailable: hooks were not installed.");
        return;
    }

    if (g_playerPointerPatchReady) {
        bool value = g_playerGodmode;
        if (ImGui::Checkbox("God Mode", &value)) {
            g_playerGodmode = value;
            ApplyPlayerGodmode();
            Log(g_playerGodmode ? "Player God Mode toggled ON from ImGui." :
                                  "Player God Mode toggled OFF from ImGui.");
        }
        if (!g_playerHealth) {
            ImGui::TextDisabled("Waiting for player health pointer; load into gameplay.");
        }
        bool stealth = g_stealthMode;
        if (ImGui::Checkbox("Stealth Mode", &stealth)) {
            g_stealthMode = stealth;
            ApplyStealthMode();
            Log(g_stealthMode ? "Stealth Mode toggled ON from ImGui." :
                                "Stealth Mode toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sets the player invisibility flag while enabled.");
        }
    } else {
        ImGui::TextDisabled("God Mode unavailable: player pointer hook was not installed.");
        ImGui::TextDisabled("Stealth Mode unavailable: player pointer hook was not installed.");
    }

    if (g_noReloadPatchReady) {
        bool value = g_noReload;
        if (ImGui::Checkbox("No Reload", &value)) {
            g_noReload = value;
            Log(g_noReload ? "No Reload toggled ON from ImGui." :
                             "No Reload toggled OFF from ImGui.");
        }
    } else {
        ImGui::TextDisabled("No Reload unavailable: hook was not installed.");
    }

    if (g_lockConsumablesPatchReady) {
        bool value = g_lockConsumables;
        if (ImGui::Checkbox("Lock Consumables", &value)) {
            g_lockConsumables = value;
            Log(g_lockConsumables ? "Lock Consumables toggled ON from ImGui." :
                                    "Lock Consumables toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Zeros guarded consume amounts for cargo, money, ammo, and items.");
        }
    } else {
        ImGui::TextDisabled("Lock Consumables unavailable: hooks were not installed.");
    }

    if (g_unlimitedResourcesPatchReady) {
        bool value = g_unlimitedResources;
        if (ImGui::Checkbox("Unlimited Resources", &value)) {
            g_unlimitedResources = value;
            ApplyBytePatchToggles();
            Log(g_unlimitedResources ? "Unlimited Resources toggled ON from ImGui." :
                                       "Unlimited Resources toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Stops guarded upgrade/resource decrement while enabled.");
        }
    } else {
        ImGui::TextDisabled("Unlimited Resources unavailable: patch was not installed.");
    }

    if (g_unlimitedSellingPatchReady) {
        bool value = g_unlimitedSelling;
        if (ImGui::Checkbox("Unlimited Selling", &value)) {
            g_unlimitedSelling = value;
            ApplyBytePatchToggles();
            Log(g_unlimitedSelling ? "Unlimited Selling toggled ON from ImGui." :
                                     "Unlimited Selling toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Stops guarded resource decrement while selling while enabled.");
        }
    } else {
        ImGui::TextDisabled("Unlimited Selling unavailable: patch was not installed.");
    }

    if (g_noFallDamagePatchReady) {
        bool value = g_noFallDamage;
        if (ImGui::Checkbox("No Fall Damage", &value)) {
            g_noFallDamage = value;
            Log(g_noFallDamage ? "No Fall Damage toggled ON from ImGui." :
                                 "No Fall Damage toggled OFF from ImGui.");
        }
        if (!g_playerEntity) {
            ImGui::TextDisabled("Waiting for player entity pointer; load into gameplay.");
        }
    } else {
        ImGui::TextDisabled("No Fall Damage unavailable: hook was not installed.");
    }

    ImGui::Spacing();
    const bool desyncReady = g_playerHealth != 0 || (g_debugContextPatchReady && g_debugContext != 0);
    if (!desyncReady) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Desynchronize Yourself")) {
        DesynchronizeYourself();
    }
    if (!desyncReady) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Uses the native desync action when ready; briefly bypasses Player God Mode if it is enabled.");
    }

    DrawInventoryEditor();
}

void DrawGameTab() {
    if (g_playerSuperSpeedPatchReady) {
        bool enabled = g_playerSuperSpeed;
        ImGui::Checkbox("##PlayerSuperSpeedEnabled", &enabled);
        if (ImGui::IsItemDeactivatedAfterEdit() || enabled != g_playerSuperSpeed) {
            g_playerSuperSpeed = enabled;
            SaveConfig();
            Logf("Player Super Speed %s value=%.6f hits=%ld",
                 g_playerSuperSpeed ? "ON" : "OFF",
                 g_playerSuperSpeedMultiplier,
                 g_playerSuperSpeedHits);
        }
        ImGui::SameLine();
    } else {
        ImGui::TextDisabled("Player Super Speed unavailable: hook was not installed.");
    }

    float speed = g_playerSuperSpeedMultiplier;
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::DragFloat("Player Super Speed", &speed, 0.001f, 0.0001f, 100.0f, "%.6f")) {
        if (speed < 0.0001f) {
            speed = 0.0001f;
        }
        if (speed > 100.0f) {
            speed = 100.0f;
        }
        g_playerSuperSpeedMultiplier = speed;
        SaveConfig();
        Logf("Player Super Speed value changed to %.6f hits=%ld",
             g_playerSuperSpeedMultiplier,
             g_playerSuperSpeedHits);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Multiplies player movement speed when the guarded player context matches.");
    }

    if (g_playerSuperJumpPatchReady) {
        bool enabled = g_playerSuperJump;
        ImGui::Checkbox("##PlayerSuperJumpEnabled", &enabled);
        if (ImGui::IsItemDeactivatedAfterEdit() || enabled != g_playerSuperJump) {
            g_playerSuperJump = enabled;
            SaveConfig();
            Logf("Player Super Jump %s distance=%.6f height=%.6f hits=%ld",
                 g_playerSuperJump ? "ON" : "OFF",
                 g_playerSuperJumpDistance,
                 g_playerSuperJumpHeight,
                 g_playerSuperJumpHits);
        }
        ImGui::SameLine();
    } else {
        ImGui::TextDisabled("Player Super Jump unavailable: hook was not installed.");
    }

    float jumpDistance = g_playerSuperJumpDistance;
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::DragFloat("Player Super Jump Distance", &jumpDistance, 0.001f, 0.0001f, 100.0f, "%.6f")) {
        if (jumpDistance < 0.0001f) {
            jumpDistance = 0.0001f;
        }
        if (jumpDistance > 100.0f) {
            jumpDistance = 100.0f;
        }
        g_playerSuperJumpDistance = jumpDistance;
        SaveConfig();
        Logf("Player Super Jump distance changed to %.6f hits=%ld",
             g_playerSuperJumpDistance,
             g_playerSuperJumpHits);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Controls the forward jump distance value. If enabled, you may need to double tap Space to trigger Super Jump.");
    }

    float jumpHeight = g_playerSuperJumpHeight;
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::DragFloat("Player Super Jump Height", &jumpHeight, 0.001f, -100.0f, 100.0f, "%.6f")) {
        if (jumpHeight < -100.0f) {
            jumpHeight = -100.0f;
        }
        if (jumpHeight > 100.0f) {
            jumpHeight = 100.0f;
        }
        g_playerSuperJumpHeight = jumpHeight;
        SaveConfig();
        Logf("Player Super Jump height changed to %.6f hits=%ld",
             g_playerSuperJumpHeight,
             g_playerSuperJumpHits);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Controls the vertical jump value. The default enhanced value is negative. If enabled, you may need to double tap Space to trigger Super Jump.");
    }

    RefreshNoclipReady();
    if (g_actions[kActionNoclip].ready) {
        bool enabled = g_noclip;
        if (ImGui::Checkbox("Noclip", &enabled)) {
            SetEngineGhostRequested(enabled, "ImGui");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggles the engine ghost movement mode. State is session-only.");
        }

        float noclipSpeed = g_noclipSpeed;
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::DragFloat("Noclip Speed", &noclipSpeed, 0.001f, 0.0001f, 1000.0f, "%.6f")) {
            if (noclipSpeed < 0.0001f) {
                noclipSpeed = 0.0001f;
            }
            if (noclipSpeed > 1000.0f) {
                noclipSpeed = 1000.0f;
            }
            g_noclipSpeed = noclipSpeed;
            if (g_noclip) {
                ApplyNoclipSpeedSettings();
            }
            SaveConfig();
            Logf("Noclip speed changed to %.6f.", g_noclipSpeed);
        }

        float noclipBoostSpeed = g_noclipBoostSpeed;
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::DragFloat("Noclip Boost Speed", &noclipBoostSpeed, 0.001f, 0.0001f, 1000.0f, "%.6f")) {
            if (noclipBoostSpeed < 0.0001f) {
                noclipBoostSpeed = 0.0001f;
            }
            if (noclipBoostSpeed > 1000.0f) {
                noclipBoostSpeed = 1000.0f;
            }
            g_noclipBoostSpeed = noclipBoostSpeed;
            if (g_noclip) {
                ApplyNoclipSpeedSettings();
            }
            SaveConfig();
            Logf("Noclip boost speed changed to %.6f.", g_noclipBoostSpeed);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Used by the engine's boosted ghost movement entries, including Shift boost.");
        }
    } else {
        ImGui::TextDisabled("Noclip unavailable: engine ghost path was not ready.");
    }

    ImGui::Separator();

    if (g_freezeMissionTimerPatchReady) {
        bool value = g_freezeMissionTimer;
        if (ImGui::Checkbox("Freeze Mission Timer", &value)) {
            g_freezeMissionTimer = value;
            ResetMissionTimerDeltas();
            Log(g_freezeMissionTimer ? "Freeze Mission Timer toggled ON from ImGui." :
                                       "Freeze Mission Timer toggled OFF from ImGui.");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Holds active mission timer end values at their current remaining offset.");
        }
    } else {
        ImGui::TextDisabled("Freeze Mission Timer unavailable: hooks were not installed.");
    }
}

void DrawMenu() {
    const ImVec2 clientSize = GameWindowClientSize();
    if (clientSize.x > 0.0f && clientSize.y > 0.0f) {
        ImVec2 minSize(320.0f, 240.0f);
        minSize.x = clientSize.x < minSize.x ? clientSize.x : minSize.x;
        minSize.y = clientSize.y < minSize.y ? clientSize.y : minSize.y;
        g_menuSize.x = ClampFloat(g_menuSize.x, minSize.x, clientSize.x);
        g_menuSize.y = ClampFloat(g_menuSize.y, minSize.y, clientSize.y);
        ImGui::SetNextWindowSizeConstraints(minSize, clientSize);
    }

    ImGui::SetNextWindowPos(g_menuPos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(g_menuSize, ImGuiCond_Appearing);
    if (!ImGui::Begin(kToolTitle, &g_menuOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImVec2 currentMenuPos = ImGui::GetWindowPos();
    const ImVec2 currentMenuSize = ImGui::GetWindowSize();
    const ImVec2 clampedMenuPos = ClampMenuPosToGameWindow(currentMenuPos, currentMenuSize);
    if (clampedMenuPos.x != currentMenuPos.x || clampedMenuPos.y != currentMenuPos.y) {
        ImGui::SetWindowPos(clampedMenuPos);
        currentMenuPos = clampedMenuPos;
    }
    if (currentMenuPos.x != g_menuPos.x || currentMenuPos.y != g_menuPos.y) {
        g_menuPos = currentMenuPos;
        g_menuPosDirty = true;
    }
    if (currentMenuSize.x != g_menuSize.x || currentMenuSize.y != g_menuSize.y) {
        g_menuSize = currentMenuSize;
        g_menuSizeDirty = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && (g_menuPosDirty || g_menuSizeDirty)) {
        SaveConfig();
        g_menuPosDirty = false;
        g_menuSizeDirty = false;
    }

    if (ImGui::BeginTabBar("Tabs")) {
        if (ImGui::BeginTabItem("Ship")) {
            DrawShipTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Player")) {
            DrawPlayerTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animus Hacks")) {
            DrawAnimusHacksTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Game")) {
            DrawGameTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Unlocks")) {
            DrawUnlocksTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hotkeys")) {
            DrawHotkeysTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("System")) {
            DrawSystemTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

std::uint8_t* BuildPlayerPointerCave(std::size_t& trampolineOffset) {
    std::vector<std::uint8_t> code;
    code.push_back(0x50);             // push rax
    code.push_back(0x51);             // push rcx
    code.push_back(0x52);             // push rdx
    code.push_back(0x41); code.push_back(0x50); // push r8
    code.push_back(0x41); code.push_back(0x51); // push r9
    code.push_back(0x41); code.push_back(0x52); // push r10
    code.push_back(0x41); code.push_back(0x53); // push r11
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20); // sub rsp,20h
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xD1); // mov rcx,rdx
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&CapturePlayerPointers));
    EmitCallRax(code);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x20); // add rsp,20h
    code.push_back(0x41); code.push_back(0x5B); // pop r11
    code.push_back(0x41); code.push_back(0x5A); // pop r10
    code.push_back(0x41); code.push_back(0x59); // pop r9
    code.push_back(0x41); code.push_back(0x58); // pop r8
    code.push_back(0x5A);             // pop rdx
    code.push_back(0x59);             // pop rcx
    code.push_back(0x58);             // pop rax
    EmitJmpAbs(code, 0, &trampolineOffset);
    return AllocateCodeCave(code, "player pointer hook");
}

std::uint8_t* BuildDebugContextCave(std::size_t& trampolineOffset) {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    code.push_back(0x51); // push rcx
    code.push_back(0x52); // push rdx
    code.push_back(0x41); code.push_back(0x50); // push r8
    code.push_back(0x41); code.push_back(0x51); // push r9
    code.push_back(0x41); code.push_back(0x52); // push r10
    code.push_back(0x41); code.push_back(0x53); // push r11
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20); // sub rsp,20h
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&CaptureDebugContext));
    EmitCallRax(code);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x20); // add rsp,20h
    code.push_back(0x41); code.push_back(0x5B); // pop r11
    code.push_back(0x41); code.push_back(0x5A); // pop r10
    code.push_back(0x41); code.push_back(0x59); // pop r9
    code.push_back(0x41); code.push_back(0x58); // pop r8
    code.push_back(0x5A); // pop rdx
    code.push_back(0x59); // pop rcx
    code.push_back(0x58); // pop rax
    EmitJmpAbs(code, 0, &trampolineOffset);
    return AllocateCodeCave(code, "debug context hook");
}

std::uint8_t* BuildFallDamageCave(std::size_t& trampolineOffset) {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_fallDamageHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_noFallDamage));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    code.push_back(0x74); code.push_back(0x17); // je skip
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerEntity));
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x00); // mov rax,[rax]
    code.push_back(0x48); code.push_back(0x39); code.push_back(0xD0); // cmp rax,rdx
    code.push_back(0x75); code.push_back(0x05); // jne skip
    code.push_back(0xF3); code.push_back(0x41); code.push_back(0x0F); code.push_back(0x10); code.push_back(0xD2); // movss xmm2,xmm10
    code.push_back(0x58); // pop rax
    EmitJmpAbs(code, 0, &trampolineOffset);
    return AllocateCodeCave(code, "fall damage hook");
}

std::uint8_t* BuildNoReloadCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_noReloadHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_noReload));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    code.push_back(0x58); // pop rax
    code.push_back(0x75); code.push_back(0x05); // jne enabled
    code.push_back(0x8B); code.push_back(0x50); code.push_back(0x1C); // mov edx,[rax+1C]
    code.push_back(0xEB); code.push_back(0x02); // jmp clipsize
    code.push_back(0x31); code.push_back(0xD2); // xor edx,edx
    code.push_back(0x8B); code.push_back(0x48); code.push_back(0x10); // mov ecx,[rax+10]
    EmitJmpAbs(code, g_noReloadAddress + sizeof(kOriginalNoReloadBytes));
    return AllocateCodeCave(code, "No Reload hook");
}

std::uint8_t* BuildLockConsumables1Cave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_lockConsumables1Hits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_lockConsumables));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    code.push_back(0x58); // pop rax
    code.push_back(0x74); code.push_back(0x02); // je original amount
    code.push_back(0x31); code.push_back(0xD2); // xor edx,edx
    code.push_back(0x49); code.push_back(0x8B); code.push_back(0x08); // mov rcx,[r8]
    code.push_back(0x8B); code.push_back(0xFA); // mov edi,edx
    code.push_back(0x48); code.push_back(0x63); code.push_back(0x41); code.push_back(0x0C); // movsxd rax,dword ptr [rcx+0C]
    code.push_back(0x48); code.push_back(0xC1); code.push_back(0xE0); code.push_back(0x20); // shl rax,20
    code.push_back(0x48); code.push_back(0xC1); code.push_back(0xF8); code.push_back(0x3F); // sar rax,3F
    EmitJmpAbs(code, g_lockConsumables1Address + sizeof(kOriginalLockConsumables1Bytes));
    return AllocateCodeCave(code, "Lock Consumables cargo hook");
}

std::uint8_t* BuildLockConsumables2Cave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_lockConsumables2Hits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_lockConsumables));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    code.push_back(0x58); // pop rax
    code.push_back(0x74); code.push_back(0x02); // je original amount
    code.push_back(0x31); code.push_back(0xD2); // xor edx,edx
    code.push_back(0x8B); code.push_back(0xFA); // mov edi,edx
    code.push_back(0x48); code.push_back(0x8D); code.push_back(0x54); code.push_back(0x24); code.push_back(0x38); // lea rdx,[rsp+38]
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0xCB); // mov rcx,rbx
    code.push_back(0x41); code.push_back(0xB8); code.push_back(0x04); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00); // mov r8d,4
    EmitJmpAbs(code, g_lockConsumables2Address + sizeof(kOriginalLockConsumables2Bytes));
    return AllocateCodeCave(code, "Lock Consumables ammo hook");
}

std::uint8_t* BuildInventorySuppliesCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    code.push_back(0x51); // push rcx
    code.push_back(0x52); // push rdx
    code.push_back(0x41); code.push_back(0x50); // push r8
    code.push_back(0x41); code.push_back(0x51); // push r9
    code.push_back(0x41); code.push_back(0x52); // push r10
    code.push_back(0x41); code.push_back(0x53); // push r11
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20); // sub rsp,20h
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&CaptureInventorySupplies));
    EmitCallRax(code);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x20); // add rsp,20h
    code.push_back(0x41); code.push_back(0x5B); // pop r11
    code.push_back(0x41); code.push_back(0x5A); // pop r10
    code.push_back(0x41); code.push_back(0x59); // pop r9
    code.push_back(0x41); code.push_back(0x58); // pop r8
    code.push_back(0x5A); // pop rdx
    code.push_back(0x59); // pop rcx
    code.push_back(0x58); // pop rax
    code.push_back(0x4C); code.push_back(0x8B); code.push_back(0x49); code.push_back(0x08); // mov r9,[rcx+08]
    code.push_back(0x0F); code.push_back(0xB7); code.push_back(0x41); code.push_back(0x12); // movzx eax,word ptr [rcx+12]
    code.push_back(0x8B); code.push_back(0xFA); // mov edi,edx
    code.push_back(0xC1); code.push_back(0xE0); code.push_back(0x03); // shl eax,3
    code.push_back(0x44); code.push_back(0x8B); code.push_back(0xD0); // mov r10d,eax
    EmitJmpAbs(code, g_inventorySuppliesAddress + sizeof(kOriginalInventorySuppliesBytes));
    return AllocateCodeCave(code, "Inventory supplies hook");
}

void PatchRel32(std::vector<std::uint8_t>& code, std::size_t offset, std::size_t target) {
    const auto rel = static_cast<std::int32_t>(target - (offset + sizeof(std::int32_t)));
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&rel);
    memcpy(code.data() + offset, bytes, sizeof(rel));
}

std::size_t EmitRel32Branch(std::vector<std::uint8_t>& code, std::uint8_t condition) {
    code.push_back(0x0F);
    code.push_back(condition);
    const std::size_t offset = code.size();
    const std::uint32_t zero = 0;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&zero);
    code.insert(code.end(), bytes, bytes + sizeof(zero));
    return offset;
}

std::size_t EmitRel32Jmp(std::vector<std::uint8_t>& code) {
    code.push_back(0xE9);
    const std::size_t offset = code.size();
    const std::uint32_t zero = 0;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&zero);
    code.insert(code.end(), bytes, bytes + sizeof(zero));
    return offset;
}

void EmitInventoryInlineApply(std::vector<std::uint8_t>& code, std::uint8_t itemPointerOffset) {
    std::vector<std::size_t> doneBranches;

    code.push_back(0x50); // push rax
    code.push_back(0x51); // push rcx
    code.push_back(0x52); // push rdx

    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryEditQueued));
    code.push_back(0x83); code.push_back(0x38); code.push_back(0x00); // cmp dword ptr [rax],0
    doneBranches.push_back(EmitRel32Branch(code, 0x84)); // je done

    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x4C); code.push_back(0x24); code.push_back(itemPointerOffset); // mov rcx,[rsp+offset]
    code.push_back(0x48); code.push_back(0x85); code.push_back(0xC9); // test rcx,rcx
    doneBranches.push_back(EmitRel32Branch(code, 0x84)); // je done

    code.push_back(0x8B); code.push_back(0x51); code.push_back(0x08); // mov edx,[rcx+08]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedId));
    code.push_back(0x3B); code.push_back(0x10); // cmp edx,[rax]
    doneBranches.push_back(EmitRel32Branch(code, 0x85)); // jne done

    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedMode));
    code.push_back(0x83); code.push_back(0x38); code.push_back(0x00); // cmp dword ptr [rax],0
    const std::size_t addBranch = EmitRel32Branch(code, 0x85); // jne add

    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedAmount));
    code.push_back(0x8B); code.push_back(0x10); // mov edx,[rax]
    const std::size_t clampJump = EmitRel32Jmp(code);

    const std::size_t addOffset = code.size();
    PatchRel32(code, addBranch, addOffset);
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedAmount));
    code.push_back(0x8B); code.push_back(0x10); // mov edx,[rax]
    code.push_back(0x44); code.push_back(0x01); code.push_back(0xDA); // add edx,r11d

    const std::size_t clampOffset = code.size();
    PatchRel32(code, clampJump, clampOffset);
    code.push_back(0x85); code.push_back(0xD2); // test edx,edx
    const std::size_t nonNegativeBranch = EmitRel32Branch(code, 0x8D); // jge non-negative
    code.push_back(0x31); code.push_back(0xD2); // xor edx,edx

    const std::size_t nonNegativeOffset = code.size();
    PatchRel32(code, nonNegativeBranch, nonNegativeOffset);
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedMax));
    code.push_back(0x3B); code.push_back(0x10); // cmp edx,[rax]
    const std::size_t storeBranch = EmitRel32Branch(code, 0x8E); // jle store
    code.push_back(0x8B); code.push_back(0x10); // mov edx,[rax]

    const std::size_t storeOffset = code.size();
    PatchRel32(code, storeBranch, storeOffset);
    code.push_back(0x41); code.push_back(0x89); code.push_back(0xD3); // mov r11d,edx
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryAppliedValue));
    code.push_back(0x89); code.push_back(0x10); // mov [rax],edx
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryQueuedId));
    code.push_back(0x8B); code.push_back(0x10); // mov edx,[rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryAppliedId));
    code.push_back(0x89); code.push_back(0x10); // mov [rax],edx
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_inventoryEditQueued));
    code.push_back(0xC7); code.push_back(0x00); // mov dword ptr [rax],0
    const std::uint32_t zero = 0;
    const auto* zeroBytes = reinterpret_cast<const std::uint8_t*>(&zero);
    code.insert(code.end(), zeroBytes, zeroBytes + sizeof(zero));

    const std::size_t doneOffset = code.size();
    for (const auto branch : doneBranches) {
        PatchRel32(code, branch, doneOffset);
    }
    code.push_back(0x5A); // pop rdx
    code.push_back(0x59); // pop rcx
    code.push_back(0x58); // pop rax
}

std::uint8_t* BuildInventoryUpdateCave() {
    std::vector<std::uint8_t> code;
    EmitInventoryInlineApply(code, 0x68); // original [rsp+50h] after push rax/rcx/rdx
    code.push_back(0x41); code.push_back(0xB8); code.push_back(0x04); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00); // mov r8d,4
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0xCB); // mov rcx,rbx
    code.push_back(0x44); code.push_back(0x89); code.push_back(0x5C); code.push_back(0x24); code.push_back(0x30); // mov [rsp+30],r11d
    EmitJmpAbs(code, g_inventoryUpdateAddress + sizeof(kOriginalInventoryUpdateBytes));
    return AllocateCodeCave(code, "Inventory update hook");
}

std::uint8_t* BuildInventoryCargoCave() {
    std::vector<std::uint8_t> code;
    EmitInventoryInlineApply(code, 0x48); // original [rsp+30h] after push rax/rcx/rdx
    code.push_back(0x41); code.push_back(0xB8); code.push_back(0x04); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00); // mov r8d,4
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0xCF); // mov rcx,rdi
    code.push_back(0x44); code.push_back(0x89); code.push_back(0x5C); code.push_back(0x24); code.push_back(0x30); // mov [rsp+30],r11d
    EmitJmpAbs(code, g_inventoryCargoAddress + sizeof(kOriginalInventoryCargoBytes));
    return AllocateCodeCave(code, "Inventory cargo hook");
}

std::uint8_t* BuildGclShipCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50);             // push rax
    code.push_back(0x51);             // push rcx
    code.push_back(0x52);             // push rdx
    code.push_back(0x41); code.push_back(0x50); // push r8
    code.push_back(0x41); code.push_back(0x51); // push r9
    code.push_back(0x41); code.push_back(0x52); // push r10
    code.push_back(0x41); code.push_back(0x53); // push r11
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20); // sub rsp,20h
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC1); // mov rcx,rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&CaptureShipPointer));
    EmitCallRax(code);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x20); // add rsp,20h
    code.push_back(0x41); code.push_back(0x5B); // pop r11
    code.push_back(0x41); code.push_back(0x5A); // pop r10
    code.push_back(0x41); code.push_back(0x59); // pop r9
    code.push_back(0x41); code.push_back(0x58); // pop r8
    code.push_back(0x5A);             // pop rdx
    code.push_back(0x59);             // pop rcx
    code.push_back(0x58);             // pop rax
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x00); // mov rax,[rax]
    code.push_back(0xFF); code.push_back(0x90); code.push_back(0x00); code.push_back(0x01); code.push_back(0x00); code.push_back(0x00); // call qword ptr [rax+100]
    code.push_back(0x3B); code.push_back(0x43); code.push_back(0x14); // cmp eax,[rbx+14]
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0); // sete al
    EmitJmpAbs(code, g_gclShipAddress + sizeof(kOriginalGclShipBytes));
    return AllocateCodeCave(code, "GclShip hook");
}

void EmitMissionTimerCaptureCall(std::vector<std::uint8_t>& code, int timerKind) {
    code.push_back(0x50);             // push rax
    code.push_back(0x51);             // push rcx
    code.push_back(0x52);             // push rdx
    code.push_back(0x41); code.push_back(0x50); // push r8
    code.push_back(0x41); code.push_back(0x51); // push r9
    code.push_back(0x41); code.push_back(0x52); // push r10
    code.push_back(0x41); code.push_back(0x53); // push r11
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20); // sub rsp,20h
    if (timerKind == 1) {
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xD9); // mov rcx,rbx
    } else if (timerKind == 3) {
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xF1); // mov rcx,rsi
    }
    code.push_back(0xBA); // mov edx,imm32
    const auto kind = static_cast<std::uint32_t>(timerKind);
    const auto* kindBytes = reinterpret_cast<const std::uint8_t*>(&kind);
    code.insert(code.end(), kindBytes, kindBytes + sizeof(kind));
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&CaptureMissionTimer));
    EmitCallRax(code);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x20); // add rsp,20h
    code.push_back(0x41); code.push_back(0x5B); // pop r11
    code.push_back(0x41); code.push_back(0x5A); // pop r10
    code.push_back(0x41); code.push_back(0x59); // pop r9
    code.push_back(0x41); code.push_back(0x58); // pop r8
    code.push_back(0x5A);             // pop rdx
    code.push_back(0x59);             // pop rcx
    code.push_back(0x58);             // pop rax
}

std::uint8_t* BuildMissionTimer1Cave() {
    std::vector<std::uint8_t> code;
    EmitMissionTimerCaptureCall(code, 1);
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x83); // mov rax,[rbx+188]
    code.push_back(0x88); code.push_back(0x01); code.push_back(0x00); code.push_back(0x00);
    EmitJmpAbs(code, g_missionTimer1Address + sizeof(kOriginalMissionTimer1Bytes));
    return AllocateCodeCave(code, "Freeze Mission Timer I hook");
}

std::uint8_t* BuildMissionTimer2Cave() {
    std::vector<std::uint8_t> code;
    EmitMissionTimerCaptureCall(code, 2);
    code.push_back(0x48); code.push_back(0x89); code.push_back(0x5C); code.push_back(0x24); code.push_back(0x08);
    code.push_back(0x48); code.push_back(0x89); code.push_back(0x74); code.push_back(0x24); code.push_back(0x10);
    code.push_back(0x57);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x20);
    EmitJmpAbs(code, g_missionTimer2Address + sizeof(kOriginalMissionTimer2Bytes));
    return AllocateCodeCave(code, "Freeze Mission Timer II hook");
}

std::uint8_t* BuildMissionTimer3Cave() {
    std::vector<std::uint8_t> code;
    EmitMissionTimerCaptureCall(code, 3);
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x46); code.push_back(0x50); // mov rax,[rsi+50]
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0x08); // mov rcx,[rax]
    EmitJmpAbs(code, g_missionTimer3Address + sizeof(kOriginalMissionTimer3Bytes));
    return AllocateCodeCave(code, "Freeze Mission Timer III hook");
}

std::uint8_t* BuildGlobalUnlockWriteCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_globalUnlockWriteHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    code.push_back(0x58); // pop rax
    code.push_back(0x48); code.push_back(0x8B); code.push_back(0xD9); // mov rbx,rcx
    code.push_back(0xC6); code.push_back(0x41); code.push_back(0x40); code.push_back(0x00); // mov byte ptr [rcx+40],0
    EmitJmpAbs(code, g_globalUnlockWriteAddress + sizeof(kOriginalGlobalUnlockWriteBytes));
    return AllocateCodeCave(code, "Global Unlocks write hook");
}

std::uint8_t* BuildAnimusHackCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0x8B); // movzx ecx,byte ptr [rbx+3A0]
    code.push_back(0xA0); code.push_back(0x03); code.push_back(0x00); code.push_back(0x00);
    code.push_back(0x50); // push rax
    code.push_back(0x52); // push rdx
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_animusHackHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_animusCheatMgr));
    code.push_back(0x48); code.push_back(0x89); code.push_back(0x18); // mov [rax],rbx
    code.push_back(0x5A); // pop rdx
    code.push_back(0x58); // pop rax
    code.push_back(0x80); code.push_back(0xF9); code.push_back(0x00); // cmp cl,0
    code.push_back(0x75); code.push_back(0x02); // jne original
    code.push_back(0xB0); code.push_back(0x01); // mov al,1
    code.push_back(0x84); code.push_back(0xC0); // test al,al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0); // sete al
    code.push_back(0x84); code.push_back(0xC9); // test cl,cl
    EmitJmpAbs(code, g_animusHackAddress + sizeof(kOriginalAnimusHackBytes));
    return AllocateCodeCave(code, "Animus Hacks Save Patch");
}

std::uint8_t* BuildCompleteAllChallengesCave() {
    std::vector<std::uint8_t> code;
    code.push_back(0x8B); code.push_back(0x45); code.push_back(0x28); // mov eax,[rbp+28]
    code.push_back(0x89); code.push_back(0x45); code.push_back(0x24); // mov [rbp+24],eax
    code.push_back(0x66); code.push_back(0x0F); code.push_back(0xEF); code.push_back(0xC0); // pxor xmm0,xmm0
    EmitJmpAbs(code, g_completeAllChallengesAddress + sizeof(kOriginalCompleteAllChallengesBytes));
    return AllocateCodeCave(code, "Complete All Challenges hook");
}

std::uint8_t* BuildPlayerSuperSpeedCave() {
    std::vector<std::uint8_t> code;
    std::vector<std::size_t> originalBranches;

    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperSpeed));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    originalBranches.push_back(EmitRel32Branch(code, 0x84)); // je original
    code.push_back(0x81); code.push_back(0xB9); // cmp dword ptr [rcx+2C4],7FF8h
    const std::uint32_t guardOffset = 0x2C4;
    const auto* guardOffsetBytes = reinterpret_cast<const std::uint8_t*>(&guardOffset);
    code.insert(code.end(), guardOffsetBytes, guardOffsetBytes + sizeof(guardOffset));
    const std::uint32_t guardValue = 0x7FF8;
    const auto* guardValueBytes = reinterpret_cast<const std::uint8_t*>(&guardValue);
    code.insert(code.end(), guardValueBytes, guardValueBytes + sizeof(guardValue));
    originalBranches.push_back(EmitRel32Branch(code, 0x85)); // jne original
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperSpeedHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperSpeedMultiplier));
    code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x59); code.push_back(0x18); // mulss xmm3,[rax]
    code.push_back(0x58); // pop rax
    EmitJmpAbs(code, g_playerSuperSpeedAddress + sizeof(kOriginalPlayerSuperSpeedBytes));

    const std::size_t originalOffset = code.size();
    for (const auto branch : originalBranches) {
        PatchRel32(code, branch, originalOffset);
    }
    code.push_back(0x58); // pop rax
    code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x59); code.push_back(0x99);
    code.push_back(0x60); code.push_back(0x01); code.push_back(0x00); code.push_back(0x00); // mulss xmm3,[rcx+160]
    EmitJmpAbs(code, g_playerSuperSpeedAddress + sizeof(kOriginalPlayerSuperSpeedBytes));
    return AllocateCodeCave(code, "Player Super Speed hook");
}

std::uint8_t* BuildPlayerSuperJumpCave(std::size_t& trampolineOffset) {
    std::vector<std::uint8_t> code;
    std::vector<std::size_t> originalBranches;

    code.push_back(0x50); // push rax
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperJump));
    code.push_back(0x80); code.push_back(0x38); code.push_back(0x00); // cmp byte ptr [rax],0
    originalBranches.push_back(EmitRel32Branch(code, 0x84)); // je original
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperJumpHits));
    code.push_back(0xFF); code.push_back(0x00); // inc dword ptr [rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperJumpDistance));
    code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x30); // movss xmm6,[rax]
    EmitMovRaxImm64(code, reinterpret_cast<std::uintptr_t>(&g_playerSuperJumpHeight));
    code.push_back(0xF3); code.push_back(0x44); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x00); // movss xmm8,[rax]
    code.push_back(0x58); // pop rax
    EmitJmpAbs(code, g_playerSuperJumpAddress + kOriginalPlayerSuperJumpSize);

    const std::size_t originalOffset = code.size();
    for (const auto branch : originalBranches) {
        PatchRel32(code, branch, originalOffset);
    }
    code.push_back(0x58); // pop rax
    EmitJmpAbs(code, 0, &trampolineOffset);
    return AllocateCodeCave(code, "Player Super Jump hook");
}

void PatchCaveJumpTarget(std::uint8_t* cave, std::size_t offset, std::uintptr_t target) {
    DWORD oldProtect = 0;
    if (VirtualProtect(cave + offset, sizeof(target), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(cave + offset, &target, sizeof(target));
        FlushInstructionCache(GetCurrentProcess(), cave + offset, sizeof(target));
        VirtualProtect(cave + offset, sizeof(target), oldProtect, &oldProtect);
    }
}

bool InstallPlayerPointerPatch() {
    g_bhvAssassinAddress = FindMainModulePatternUnique(
        kBhvAssassinPattern, sizeof(kBhvAssassinPattern), "Player pointer hook");
    if (!g_bhvAssassinAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_bhvAssassinAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing player pointer hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalBhvAssassinBytes, sizeof(kOriginalBhvAssassinBytes))) {
        LogPatchMismatch("Player pointer hook",
                         target,
                         kOriginalBhvAssassinBytes,
                         sizeof(kOriginalBhvAssassinBytes));
        return false;
    }

    std::size_t trampolineOffset = 0;
    g_bhvAssassinCave = BuildPlayerPointerCave(trampolineOffset);
    if (!g_bhvAssassinCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for player pointer hook.");
        return false;
    }
    if (MH_CreateHook(target, g_bhvAssassinCave, &g_originalBhvAssassin) != MH_OK) {
        LogError("MinHook CreateHook failed for player pointer hook.");
        return false;
    }
    PatchCaveJumpTarget(g_bhvAssassinCave, trampolineOffset, reinterpret_cast<std::uintptr_t>(g_originalBhvAssassin));
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for player pointer hook.");
        return false;
    }
    Logf("Installed player pointer hook at 0x%p.", target);
    return true;
}

bool InstallDebugContextPatch() {
    g_debugContextAddress = FindMainModulePatternUnique(
        kDebugContextPattern, sizeof(kDebugContextPattern), "debug context hook");
    if (!g_debugContextAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_debugContextAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing debug context hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalDebugContextBytes, sizeof(kOriginalDebugContextBytes))) {
        LogPatchMismatch("debug context hook",
                         target,
                         kOriginalDebugContextBytes,
                         sizeof(kOriginalDebugContextBytes));
        return false;
    }

    std::size_t trampolineOffset = 0;
    g_debugContextCave = BuildDebugContextCave(trampolineOffset);
    if (!g_debugContextCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for debug context hook.");
        return false;
    }
    if (MH_CreateHook(target, g_debugContextCave, &g_originalDebugContext) != MH_OK) {
        LogError("MinHook CreateHook failed for debug context hook.");
        return false;
    }
    PatchCaveJumpTarget(g_debugContextCave, trampolineOffset, reinterpret_cast<std::uintptr_t>(g_originalDebugContext));
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for debug context hook.");
        return false;
    }
    Logf("Installed debug context hook at 0x%p.", target);
    return true;
}

bool InstallFallDamagePatch() {
    g_fallDamageAddress = FindMainModulePatternUnique(
        kFallDamagePattern, sizeof(kFallDamagePattern), "No Fall Damage hook");
    if (!g_fallDamageAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_fallDamageAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing No Fall Damage hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalFallDamageBytes, sizeof(kOriginalFallDamageBytes))) {
        LogPatchMismatch("No Fall Damage hook",
                         target,
                         kOriginalFallDamageBytes,
                         sizeof(kOriginalFallDamageBytes));
        return false;
    }

    std::size_t trampolineOffset = 0;
    g_fallDamageCave = BuildFallDamageCave(trampolineOffset);
    if (!g_fallDamageCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for No Fall Damage hook.");
        return false;
    }
    if (MH_CreateHook(target, g_fallDamageCave, &g_originalFallDamage) != MH_OK) {
        LogError("MinHook CreateHook failed for No Fall Damage hook.");
        return false;
    }
    PatchCaveJumpTarget(g_fallDamageCave, trampolineOffset, reinterpret_cast<std::uintptr_t>(g_originalFallDamage));
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for No Fall Damage hook.");
        return false;
    }
    Logf("Installed No Fall Damage hook at 0x%p.", target);
    return true;
}

bool InstallNoReloadPatch() {
    g_noReloadAddress = FindMainModulePatternUnique(
        kNoReloadPattern, sizeof(kNoReloadPattern), "No Reload hook");
    if (!g_noReloadAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_noReloadAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing No Reload hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalNoReloadBytes, sizeof(kOriginalNoReloadBytes))) {
        LogPatchMismatch("No Reload hook",
                         target,
                         kOriginalNoReloadBytes,
                         sizeof(kOriginalNoReloadBytes));
        return false;
    }

    g_noReloadCave = BuildNoReloadCave();
    if (!g_noReloadCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for No Reload hook.");
        return false;
    }
    if (MH_CreateHook(target, g_noReloadCave, &g_originalNoReload) != MH_OK) {
        LogError("MinHook CreateHook failed for No Reload hook.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for No Reload hook.");
        return false;
    }
    Logf("Installed No Reload hook at 0x%p.", target);
    return true;
}

bool InstallGclShipPatch() {
    g_gclShipAddress = FindMainModulePatternUnique(
        kGclShipPattern, sizeof(kGclShipPattern), "GclShip hook");
    if (!g_gclShipAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_gclShipAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing GclShip hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalGclShipBytes, sizeof(kOriginalGclShipBytes))) {
        LogPatchMismatch("GclShip hook",
                         target,
                         kOriginalGclShipBytes,
                         sizeof(kOriginalGclShipBytes));
        return false;
    }

    g_gclShipCave = BuildGclShipCave();
    if (!g_gclShipCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for GclShip hook.");
        return false;
    }
    if (MH_CreateHook(target, g_gclShipCave, &g_originalGclShip) != MH_OK) {
        LogError("MinHook CreateHook failed for GclShip hook.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for GclShip hook.");
        return false;
    }
    Logf("Installed GclShip hook at 0x%p.", target);
    return true;
}

bool InstallLockConsumablesPatch(const std::uint8_t* pattern,
                                 std::size_t patternSize,
                                 const std::uint8_t* originalBytes,
                                 std::size_t originalSize,
                                 const char* label,
                                 std::uintptr_t& address,
                                 std::uint8_t*& cave,
                                 void*& original,
                                 std::uint8_t* (*buildCave)()) {
    address = FindMainModulePatternUnique(pattern, patternSize, label);
    if (!address) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(address);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogErrorf("Refusing %s: target is already hooked by another tool.", label);
        return false;
    }
    if (!BytesMatch(target, originalBytes, originalSize)) {
        LogPatchMismatch(label, target, originalBytes, originalSize);
        return false;
    }

    cave = buildCave();
    if (!cave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogErrorf("MinHook initialize failed for %s.", label);
        return false;
    }
    if (MH_CreateHook(target, cave, &original) != MH_OK) {
        LogErrorf("MinHook CreateHook failed for %s.", label);
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogErrorf("MinHook EnableHook failed for %s.", label);
        return false;
    }
    Logf("Installed %s at 0x%p.", label, target);
    return true;
}

bool InstallLockConsumablesPatches() {
    const bool cargo = InstallLockConsumablesPatch(kLockConsumables1Pattern,
                                                   sizeof(kLockConsumables1Pattern),
                                                   kOriginalLockConsumables1Bytes,
                                                   sizeof(kOriginalLockConsumables1Bytes),
                                                   "Lock Consumables cargo hook",
                                                   g_lockConsumables1Address,
                                                   g_lockConsumables1Cave,
                                                   g_originalLockConsumables1,
                                                   &BuildLockConsumables1Cave);
    const bool ammo = InstallLockConsumablesPatch(kLockConsumables2Pattern,
                                                  sizeof(kLockConsumables2Pattern),
                                                  kOriginalLockConsumables2Bytes,
                                                  sizeof(kOriginalLockConsumables2Bytes),
                                                  "Lock Consumables ammo hook",
                                                  g_lockConsumables2Address,
                                                  g_lockConsumables2Cave,
                                                  g_originalLockConsumables2,
                                                  &BuildLockConsumables2Cave);
    return cargo || ammo;
}

bool InstallInventoryPatch(const std::uint8_t* pattern,
                           std::size_t patternSize,
                           std::size_t patchOffset,
                           const std::uint8_t* originalBytes,
                           std::size_t originalSize,
                           const char* label,
                           std::uintptr_t& address,
                           std::uint8_t*& cave,
                           void*& original,
                           std::uint8_t* (*buildCave)()) {
    const auto patternAddress = FindMainModulePatternUnique(pattern, patternSize, label);
    if (!patternAddress) {
        return false;
    }
    address = patternAddress + patchOffset;
    auto* target = reinterpret_cast<std::uint8_t*>(address);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogErrorf("Refusing %s: target is already hooked by another tool.", label);
        return false;
    }
    if (!BytesMatch(target, originalBytes, originalSize)) {
        LogPatchMismatch(label, target, originalBytes, originalSize);
        return false;
    }

    cave = buildCave();
    if (!cave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogErrorf("MinHook initialize failed for %s.", label);
        return false;
    }
    if (MH_CreateHook(target, cave, &original) != MH_OK) {
        LogErrorf("MinHook CreateHook failed for %s.", label);
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogErrorf("MinHook EnableHook failed for %s.", label);
        return false;
    }
    Logf("Installed %s at 0x%p.", label, target);
    return true;
}

bool InstallInventoryEditPatches() {
    const bool supplies = InstallInventoryPatch(kInventorySuppliesPattern,
                                                sizeof(kInventorySuppliesPattern),
                                                kInventorySuppliesPatchOffset,
                                                kOriginalInventorySuppliesBytes,
                                                sizeof(kOriginalInventorySuppliesBytes),
                                                "Inventory supplies hook",
                                                g_inventorySuppliesAddress,
                                                g_inventorySuppliesCave,
                                                g_originalInventorySupplies,
                                                &BuildInventorySuppliesCave);
    const bool update = InstallInventoryPatch(kInventoryUpdatePattern,
                                              sizeof(kInventoryUpdatePattern),
                                              0,
                                              kOriginalInventoryUpdateBytes,
                                              sizeof(kOriginalInventoryUpdateBytes),
                                              "Inventory update hook",
                                              g_inventoryUpdateAddress,
                                              g_inventoryUpdateCave,
                                              g_originalInventoryUpdate,
                                              &BuildInventoryUpdateCave);
    const bool cargo = InstallInventoryPatch(kInventoryCargoPattern,
                                             sizeof(kInventoryCargoPattern),
                                             0,
                                             kOriginalInventoryCargoBytes,
                                             sizeof(kOriginalInventoryCargoBytes),
                                             "Inventory cargo hook",
                                             g_inventoryCargoAddress,
                                             g_inventoryCargoCave,
                                             g_originalInventoryCargo,
                                             &BuildInventoryCargoCave);
    return supplies && update && cargo;
}

bool InstallMissionTimerPatch(const std::uint8_t* pattern,
                              std::size_t patternSize,
                              const std::uint8_t* originalBytes,
                              std::size_t originalSize,
                              const char* label,
                              std::uintptr_t& address,
                              std::uint8_t*& cave,
                              void*& original,
                              std::uint8_t* (*buildCave)()) {
    address = FindMainModulePatternUnique(pattern, patternSize, label);
    if (!address) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(address);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogErrorf("Refusing %s: target is already hooked by another tool.", label);
        return false;
    }
    if (!BytesMatch(target, originalBytes, originalSize)) {
        LogPatchMismatch(label, target, originalBytes, originalSize);
        return false;
    }

    cave = buildCave();
    if (!cave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogErrorf("MinHook initialize failed for %s.", label);
        return false;
    }
    if (MH_CreateHook(target, cave, &original) != MH_OK) {
        LogErrorf("MinHook CreateHook failed for %s.", label);
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogErrorf("MinHook EnableHook failed for %s.", label);
        return false;
    }
    Logf("Installed %s at 0x%p.", label, target);
    return true;
}

bool InstallFreezeMissionTimerPatches() {
    const bool timer1 = InstallMissionTimerPatch(kMissionTimer1Pattern,
                                                 sizeof(kMissionTimer1Pattern),
                                                 kOriginalMissionTimer1Bytes,
                                                 sizeof(kOriginalMissionTimer1Bytes),
                                                 "Freeze Mission Timer I hook",
                                                 g_missionTimer1Address,
                                                 g_missionTimer1Cave,
                                                 g_originalMissionTimer1,
                                                 &BuildMissionTimer1Cave);
    const bool timer2 = InstallMissionTimerPatch(kMissionTimer2Pattern,
                                                 sizeof(kMissionTimer2Pattern),
                                                 kOriginalMissionTimer2Bytes,
                                                 sizeof(kOriginalMissionTimer2Bytes),
                                                 "Freeze Mission Timer II hook",
                                                 g_missionTimer2Address,
                                                 g_missionTimer2Cave,
                                                 g_originalMissionTimer2,
                                                 &BuildMissionTimer2Cave);
    const bool timer3 = InstallMissionTimerPatch(kMissionTimer3Pattern,
                                                 sizeof(kMissionTimer3Pattern),
                                                 kOriginalMissionTimer3Bytes,
                                                 sizeof(kOriginalMissionTimer3Bytes),
                                                 "Freeze Mission Timer III hook",
                                                 g_missionTimer3Address,
                                                 g_missionTimer3Cave,
                                                 g_originalMissionTimer3,
                                                 &BuildMissionTimer3Cave);
    return timer1 || timer2 || timer3;
}

bool WritePatchedBytes(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size, const char* label) {
    DWORD oldProtect = 0;
    auto* target = reinterpret_cast<void*>(address);
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LogErrorf("%s failed: VirtualProtect failed.", label);
        return false;
    }
    memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    VirtualProtect(target, size, oldProtect, &oldProtect);
    return true;
}

bool InstallBytePatch(const std::uint8_t* pattern,
                      std::size_t patternSize,
                      const std::uint8_t* originalBytes,
                      std::size_t originalSize,
                      const char* label,
                      std::uintptr_t& address) {
    address = FindMainModulePatternUnique(pattern, patternSize, label);
    if (!address) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(address);
    if (!BytesMatch(target, originalBytes, originalSize)) {
        LogPatchMismatch(label, target, originalBytes, originalSize);
        return false;
    }
    return true;
}

void ApplyOptionalBytePatch(bool ready,
                            bool enabled,
                            std::uintptr_t address,
                            const std::uint8_t* enabledBytes,
                            const std::uint8_t* originalBytes,
                            std::size_t size,
                            const char* label) {
    if (!ready || !address) {
        return;
    }
    WritePatchedBytes(address, enabled ? enabledBytes : originalBytes, size, label);
}

void ApplyBytePatchToggles() {
    ApplyOptionalBytePatch(g_unlimitedResourcesPatchReady,
                           g_unlimitedResources,
                           g_unlimitedResourcesAddress,
                           kNop3Bytes,
                           kOriginalUnlimitedResourcesBytes,
                           sizeof(kOriginalUnlimitedResourcesBytes),
                           "Unlimited Resources patch");
    ApplyOptionalBytePatch(g_unlimitedSellingPatchReady,
                           g_unlimitedSelling,
                           g_unlimitedSellingAddress,
                           kNop3Bytes,
                           kOriginalUnlimitedSellingBytes,
                           sizeof(kOriginalUnlimitedSellingBytes),
                           "Unlimited Selling patch");
    ApplyOptionalBytePatch(g_harpoonGodmodePatchReady,
                           g_harpoonGodmode,
                           g_harpoonGodmode1Address,
                           kNop4Bytes,
                           kOriginalHarpoonGodmodeBytes,
                           sizeof(kOriginalHarpoonGodmodeBytes),
                           "Harpoon Godmode I patch");
    ApplyOptionalBytePatch(g_harpoonGodmodePatchReady,
                           g_harpoonGodmode,
                           g_harpoonGodmode2Address,
                           kNop4Bytes,
                           kOriginalHarpoonGodmode2Bytes,
                           sizeof(kOriginalHarpoonGodmode2Bytes),
                           "Harpoon Godmode II patch");
}

bool InstallUnlimitedResourcesPatch() {
    const auto updateAddress = FindMainModulePatternUnique(
        kInventoryUpdatePattern, sizeof(kInventoryUpdatePattern), "Unlimited Resources patch anchor");
    if (!updateAddress || updateAddress < sizeof(kOriginalUnlimitedResourcesBytes)) {
        return false;
    }

    g_unlimitedResourcesAddress = updateAddress - sizeof(kOriginalUnlimitedResourcesBytes);
    auto* target = reinterpret_cast<std::uint8_t*>(g_unlimitedResourcesAddress);
    if (!BytesMatch(target, kOriginalUnlimitedResourcesBytes, sizeof(kOriginalUnlimitedResourcesBytes))) {
        LogPatchMismatch("Unlimited Resources patch",
                         target,
                         kOriginalUnlimitedResourcesBytes,
                         sizeof(kOriginalUnlimitedResourcesBytes));
        return false;
    }
    return true;
}

bool InstallUnlimitedSellingPatch() {
    return InstallBytePatch(kUnlimitedSellingPattern,
                            sizeof(kUnlimitedSellingPattern),
                            kOriginalUnlimitedSellingBytes,
                            sizeof(kOriginalUnlimitedSellingBytes),
                            "Unlimited Selling patch",
                            g_unlimitedSellingAddress);
}

bool InstallHarpoonGodmodePatches() {
    const bool harpoon1 = InstallBytePatch(kHarpoonGodmode1Pattern,
                                           sizeof(kHarpoonGodmode1Pattern),
                                           kOriginalHarpoonGodmodeBytes,
                                           sizeof(kOriginalHarpoonGodmodeBytes),
                                           "Harpoon Godmode I patch",
                                           g_harpoonGodmode1Address);
    const bool harpoon2 = InstallBytePatch(kHarpoonGodmode2Pattern,
                                           sizeof(kHarpoonGodmode2Pattern),
                                           kOriginalHarpoonGodmode2Bytes,
                                           sizeof(kOriginalHarpoonGodmode2Bytes),
                                           "Harpoon Godmode II patch",
                                           g_harpoonGodmode2Address);
    return harpoon1 && harpoon2;
}

bool InstallGlobalUnlocksPatch() {
    if (g_globalUnlocksInstalled) {
        return true;
    }
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    g_globalUnlockReadAddress = FindMainModulePatternUnique(
        kGlobalUnlockReadPattern, sizeof(kGlobalUnlockReadPattern), "Global Unlocks read patch");
    const auto writePatternAddress = FindMainModulePatternUnique(
        kGlobalUnlockWritePattern, sizeof(kGlobalUnlockWritePattern), "Global Unlocks write hook");
    const auto returnPatternAddress = FindMainModulePatternUnique(
        kGlobalUnlockReturnPattern, sizeof(kGlobalUnlockReturnPattern), "Global Unlocks return patch");
    if (!g_globalUnlockReadAddress || !writePatternAddress || !returnPatternAddress) {
        return false;
    }
    if (returnPatternAddress < kGlobalUnlockReturnPatchBackOffset) {
        LogError("Global Unlocks return patch unavailable: signature address underflow.");
        return false;
    }
    g_globalUnlockWriteAddress = writePatternAddress + kGlobalUnlockWritePatchOffset;
    g_globalUnlockReturnAddress = returnPatternAddress - kGlobalUnlockReturnPatchBackOffset;

    auto* readTarget = reinterpret_cast<std::uint8_t*>(g_globalUnlockReadAddress);
    auto* writeTarget = reinterpret_cast<std::uint8_t*>(g_globalUnlockWriteAddress);
    auto* returnTarget = reinterpret_cast<std::uint8_t*>(g_globalUnlockReturnAddress);
    if (writeTarget[0] == 0xE9 || writeTarget[0] == 0xFF) {
        LogError("Refusing Global Unlocks write hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(readTarget, kOriginalGlobalUnlockReadBytes, sizeof(kOriginalGlobalUnlockReadBytes))) {
        LogPatchMismatch("Global Unlocks read patch",
                         readTarget,
                         kOriginalGlobalUnlockReadBytes,
                         sizeof(kOriginalGlobalUnlockReadBytes));
        return false;
    }
    if (!BytesMatch(writeTarget, kOriginalGlobalUnlockWriteBytes, sizeof(kOriginalGlobalUnlockWriteBytes))) {
        LogPatchMismatch("Global Unlocks write hook",
                         writeTarget,
                         kOriginalGlobalUnlockWriteBytes,
                         sizeof(kOriginalGlobalUnlockWriteBytes));
        return false;
    }
    if (!BytesMatch(returnTarget, kOriginalGlobalUnlockReturnBytes, sizeof(kOriginalGlobalUnlockReturnBytes))) {
        LogPatchMismatch("Global Unlocks return patch",
                         returnTarget,
                         kOriginalGlobalUnlockReturnBytes,
                         sizeof(kOriginalGlobalUnlockReturnBytes));
        return false;
    }

    g_globalUnlockWriteCave = BuildGlobalUnlockWriteCave();
    if (!g_globalUnlockWriteCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for Global Unlocks write hook.");
        return false;
    }
    if (MH_CreateHook(writeTarget, g_globalUnlockWriteCave, &g_originalGlobalUnlockWrite) != MH_OK) {
        LogError("MinHook CreateHook failed for Global Unlocks write hook.");
        return false;
    }
    bool readPatched = false;
    bool returnPatched = false;
    auto rollbackSessionPatches = [&]() {
        if (returnPatched) {
            WritePatchedBytes(g_globalUnlockReturnAddress,
                              kOriginalGlobalUnlockReturnBytes,
                              sizeof(kOriginalGlobalUnlockReturnBytes),
                              "Global Unlocks return patch rollback");
        }
        if (readPatched) {
            WritePatchedBytes(g_globalUnlockReadAddress,
                              kOriginalGlobalUnlockReadBytes,
                              sizeof(kOriginalGlobalUnlockReadBytes),
                              "Global Unlocks read patch rollback");
        }
        MH_RemoveHook(writeTarget);
    };

    readPatched = WritePatchedBytes(g_globalUnlockReadAddress,
                                    kEnabledGlobalUnlockReadBytes,
                                    sizeof(kEnabledGlobalUnlockReadBytes),
                                    "Global Unlocks read patch");
    if (!readPatched) {
        rollbackSessionPatches();
        return false;
    }
    returnPatched = WritePatchedBytes(g_globalUnlockReturnAddress,
                                      kEnabledGlobalUnlockReturnBytes,
                                      sizeof(kEnabledGlobalUnlockReturnBytes),
                                      "Global Unlocks return patch");
    if (!returnPatched) {
        rollbackSessionPatches();
        return false;
    }
    if (MH_EnableHook(writeTarget) != MH_OK) {
        LogError("MinHook EnableHook failed for Global Unlocks write hook.");
        rollbackSessionPatches();
        return false;
    }

    g_globalUnlocksInstalled = true;
    Logf("Installed Global Unlocks session patches at 0x%p, 0x%p, 0x%p.",
         readTarget,
         writeTarget,
         returnTarget);
    return true;
}

bool InstallAnimusHackPatch() {
    if (g_animusHackInstalled) {
        return true;
    }
    if (!g_mainModuleBase && !InitMainModuleRange()) {
        return false;
    }

    g_animusHackAddress = FindMainModulePatternUnique(
        kAnimusHackPattern, sizeof(kAnimusHackPattern), "Animus Hacks Save Patch");
    if (!g_animusHackAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_animusHackAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing Animus Hacks Save Patch: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalAnimusHackBytes, sizeof(kOriginalAnimusHackBytes))) {
        LogPatchMismatch("Animus Hacks Save Patch",
                         target,
                         kOriginalAnimusHackBytes,
                         sizeof(kOriginalAnimusHackBytes));
        return false;
    }

    g_animusHackCave = BuildAnimusHackCave();
    if (!g_animusHackCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for Animus Hacks Save Patch.");
        return false;
    }
    if (MH_CreateHook(target, g_animusHackCave, &g_originalAnimusHack) != MH_OK) {
        LogError("MinHook CreateHook failed for Animus Hacks Save Patch.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for Animus Hacks Save Patch.");
        return false;
    }

    g_animusHackInstalled = true;
    Logf("Installed Animus Hacks Save Patch at 0x%p.", target);
    return true;
}

bool InstallPlayerSuperSpeedPatch() {
    g_playerSuperSpeedAddress = FindMainModulePatternUnique(
        kPlayerSuperSpeedPattern, sizeof(kPlayerSuperSpeedPattern), "Player Super Speed hook");
    if (!g_playerSuperSpeedAddress) {
        return false;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(g_playerSuperSpeedAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing Player Super Speed hook: target is already hooked by another tool.");
        return false;
    }
    if (!BytesMatch(target, kOriginalPlayerSuperSpeedBytes, sizeof(kOriginalPlayerSuperSpeedBytes))) {
        LogPatchMismatch("Player Super Speed hook",
                         target,
                         kOriginalPlayerSuperSpeedBytes,
                         sizeof(kOriginalPlayerSuperSpeedBytes));
        return false;
    }

    g_playerSuperSpeedCave = BuildPlayerSuperSpeedCave();
    if (!g_playerSuperSpeedCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for Player Super Speed hook.");
        return false;
    }
    if (MH_CreateHook(target, g_playerSuperSpeedCave, &g_originalPlayerSuperSpeed) != MH_OK) {
        LogError("MinHook CreateHook failed for Player Super Speed hook.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for Player Super Speed hook.");
        return false;
    }
    Logf("Installed Player Super Speed hook at 0x%p.", target);
    return true;
}

bool PlayerSuperJumpBytesMatch(const std::uint8_t* target) {
    return target[0] == 0xF3 && target[1] == 0x0F && target[2] == 0x10 && target[3] == 0x35 &&
           target[8] == 0xF3 && target[9] == 0x44 && target[10] == 0x0F && target[11] == 0x10 &&
           target[12] == 0x05;
}

bool InstallPlayerSuperJumpPatch() {
    const auto patternAddress = FindMainModuleMaskedPatternUnique(
        kPlayerSuperJumpPattern,
        kPlayerSuperJumpPatternMask,
        sizeof(kPlayerSuperJumpPattern),
        "Player Super Jump hook");
    if (!patternAddress) {
        return false;
    }
    g_playerSuperJumpAddress = patternAddress + kPlayerSuperJumpPatchOffset;
    auto* target = reinterpret_cast<std::uint8_t*>(g_playerSuperJumpAddress);
    if (target[0] == 0xE9 || target[0] == 0xFF) {
        LogError("Refusing Player Super Jump hook: target is already hooked by another tool.");
        return false;
    }
    if (!PlayerSuperJumpBytesMatch(target)) {
        LogError("Player Super Jump hook byte mismatch; target did not match expected jump vector loads.");
        return false;
    }

    std::size_t trampolineOffset = 0;
    g_playerSuperJumpCave = BuildPlayerSuperJumpCave(trampolineOffset);
    if (!g_playerSuperJumpCave) {
        return false;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for Player Super Jump hook.");
        return false;
    }
    if (MH_CreateHook(target, g_playerSuperJumpCave, &g_originalPlayerSuperJump) != MH_OK) {
        LogError("MinHook CreateHook failed for Player Super Jump hook.");
        return false;
    }
    PatchCaveJumpTarget(g_playerSuperJumpCave,
                        trampolineOffset,
                        reinterpret_cast<std::uintptr_t>(g_originalPlayerSuperJump));
    if (MH_EnableHook(target) != MH_OK) {
        LogError("MinHook EnableHook failed for Player Super Jump hook.");
        return false;
    }
    Logf("Installed Player Super Jump hook at 0x%p.", target);
    return true;
}

void InstallGameplayPatches() {
    if (!InitMainModuleRange()) {
        return;
    }

    g_shipPatchReady = InstallGclShipPatch();
    g_playerPointerPatchReady = InstallPlayerPointerPatch();
    g_debugContextPatchReady = InstallDebugContextPatch();
    g_noFallDamagePatchReady = InstallFallDamagePatch();
    g_noReloadPatchReady = InstallNoReloadPatch();
    g_lockConsumablesPatchReady = InstallLockConsumablesPatches();
    g_unlimitedResourcesPatchReady = InstallUnlimitedResourcesPatch();
    g_freezeMissionTimerPatchReady = InstallFreezeMissionTimerPatches();
    g_unlimitedSellingPatchReady = InstallUnlimitedSellingPatch();
    g_harpoonGodmodePatchReady = InstallHarpoonGodmodePatches();
    g_inventoryEditPatchReady = InstallInventoryEditPatches();
    g_playerSuperSpeedPatchReady = InstallPlayerSuperSpeedPatch();
    g_playerSuperJumpPatchReady = InstallPlayerSuperJumpPatch();
    RefreshNoclipReady();
    g_actions[kActionShipGodmode].ready = g_shipPatchReady;
    g_actions[kActionNoCannonCooldown].ready = g_shipPatchReady;
    g_actions[kActionAllyGodmode].ready = g_shipPatchReady;
    g_actions[kActionShipStealthMode].ready = g_shipPatchReady;
    g_actions[kActionHarpoonGodmode].ready = g_harpoonGodmodePatchReady;
    g_actions[kActionPlayerGodmode].ready = g_playerPointerPatchReady;
    g_actions[kActionDesynchronizeYourself].ready = g_playerPointerPatchReady || g_debugContextPatchReady;
    g_actions[kActionStealthMode].ready = g_playerPointerPatchReady;
    g_actions[kActionNoReload].ready = g_noReloadPatchReady;
    g_actions[kActionNoFallDamage].ready = g_noFallDamagePatchReady;
    g_actions[kActionLockConsumables].ready = g_lockConsumablesPatchReady;
    g_actions[kActionUnlimitedResources].ready = g_unlimitedResourcesPatchReady;
    g_actions[kActionUnlimitedSelling].ready = g_unlimitedSellingPatchReady;
    g_actions[kActionFreezeMissionTimer].ready = g_freezeMissionTimerPatchReady;
    g_actions[kActionPlayerSuperSpeed].ready = g_playerSuperSpeedPatchReady;
    g_actions[kActionPlayerSuperJump].ready = g_playerSuperJumpPatchReady;
    g_actions[kActionNoclip].ready = g_nativeGhostReady;
    if (!g_shipPatchReady) {
        g_shipGodmode = false;
        g_noCannonCooldown = false;
        g_shipStealthMode = false;
        g_allyGodmode = false;
    }
    if (!g_harpoonGodmodePatchReady) {
        g_harpoonGodmode = false;
    }
    if (!g_playerPointerPatchReady) {
        g_playerGodmode = false;
        g_stealthMode = false;
    }
    if (!g_noFallDamagePatchReady) {
        g_noFallDamage = false;
    }
    if (!g_noReloadPatchReady) {
        g_noReload = false;
    }
    if (!g_lockConsumablesPatchReady) {
        g_lockConsumables = false;
    }
    if (!g_unlimitedResourcesPatchReady) {
        g_unlimitedResources = false;
    }
    if (!g_unlimitedSellingPatchReady) {
        g_unlimitedSelling = false;
    }
    if (!g_freezeMissionTimerPatchReady) {
        g_freezeMissionTimer = false;
    }
    if (!g_playerSuperSpeedPatchReady) {
        g_playerSuperSpeed = false;
    }
    if (!g_playerSuperJumpPatchReady) {
        g_playerSuperJump = false;
    }
    if (!g_actions[kActionNoclip].ready) {
        g_noclip = false;
    }
    ApplyBytePatchToggles();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_ACTIVATEAPP || msg == WM_KILLFOCUS || msg == WM_CANCELMODE) {
        if (wparam == FALSE || msg != WM_ACTIVATEAPP) {
            g_menuOpen = false;
            ReleaseMenuInputState();
        }
    }

    if (g_menuOpen) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
        if (g_hotkeyCaptureAction != -1 && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
            const int vk = static_cast<int>(wparam);
            if (vk == VK_ESCAPE) {
                g_hotkeyCaptureAction = -1;
                Log("Hotkey capture cancelled.");
            } else if (vk == VK_BACK || vk == VK_DELETE) {
                if (g_hotkeyCaptureAction == kMenuHotkeyCapture) {
                    AssignMenuHotkey(0);
                } else {
                    AssignHotkey(g_hotkeyCaptureAction, 0);
                }
                g_hotkeyCaptureAction = -1;
            } else if (IsBindableHotkey(vk)) {
                if (g_hotkeyCaptureAction == kMenuHotkeyCapture) {
                    AssignMenuHotkey(vk);
                } else {
                    AssignHotkey(g_hotkeyCaptureAction, vk);
                }
                g_hotkeyCaptureAction = -1;
            }
            return TRUE;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (IsMouseInputCaptureActive() && io.WantCaptureMouse &&
            (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
             msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
             msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
             msg == WM_MOUSEMOVE || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)) {
            return TRUE;
        }
        if (IsKeyboardInputCaptureActive() &&
            (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR ||
             msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
             msg == WM_SYSCHAR || msg == WM_DEADCHAR || msg == WM_SYSDEADCHAR)) {
            return TRUE;
        }
        if (IsMouseInputCaptureActive() && msg == WM_SETCURSOR) {
            SetCursor(nullptr);
            return TRUE;
        }
        if ((IsMouseInputCaptureActive() || IsKeyboardInputCaptureActive()) &&
            (msg == WM_INPUT || msg == WM_CAPTURECHANGED)) {
            return TRUE;
        }
    }
    return CallWindowProcA(g_originalWndProc, hwnd, msg, wparam, lparam);
}

bool InitImGui(IDXGISwapChain* swapChain) {
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        return false;
    }
    InitConsole();
    g_device->GetImmediateContext(&g_context);
    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    g_gameWindow = desc.OutputWindow;

    CreateRenderTarget(swapChain);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigWindowsMoveFromTitleBarOnly = false;
    ApplyRedStyle();
    ImGui_ImplWin32_Init(g_gameWindow);
    ImGui_ImplDX11_Init(g_device, g_context);
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(g_gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    g_imguiReady = true;
    Log("Dear ImGui DX11 overlay initialized.");
    Log("AC5Tools overlay initialized with guarded gameplay hooks.");
    return true;
}

HRESULT __stdcall HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    if (!g_imguiReady) {
        InitImGui(swapChain);
    }

    static bool menuWasDown = false;
    const bool menuIsDown =
        g_menuHotkey != 0 &&
        g_suppressedHotkeyVk != g_menuHotkey &&
        (QueryPhysicalKeyState(g_menuHotkey) & 0x8000) != 0;
    if (menuIsDown && !menuWasDown) {
        g_menuOpen = !g_menuOpen;
        RefreshInputCaptureState();
        if (!g_menuOpen) {
            ReleaseMenuInputState();
        }
    }
    menuWasDown = menuIsDown;
    if (g_suppressedHotkeyVk != 0 && (QueryPhysicalKeyState(g_suppressedHotkeyVk) & 0x8000) == 0) {
        g_suppressedHotkeyVk = 0;
    }

    if (g_imguiReady) {
        if (g_menuOpen && GetForegroundWindow() != g_gameWindow) {
            g_menuOpen = false;
            ReleaseMenuInputState();
        }
        UpdateMouseWindowLock();
        UpdatePlayerGodmodeBypass();
        ProcessHotkeys();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = g_menuOpen;
        io.MouseDown[0] = g_menuOpen && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.MouseDown[1] = g_menuOpen && ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        io.MouseDown[2] = g_menuOpen && ((GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
        ImGui::NewFrame();
        if (g_menuOpen) {
            DrawMenu();
        }
        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return g_originalPresent(swapChain, syncInterval, flags);
}

HRESULT __stdcall HookResizeBuffers(IDXGISwapChain* swapChain,
                                    UINT bufferCount,
                                    UINT width,
                                    UINT height,
                                    DXGI_FORMAT newFormat,
                                    UINT swapChainFlags) {
    CleanupRenderTarget();
    const HRESULT result = g_originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
    if (SUCCEEDED(result) && g_device) {
        CreateRenderTarget(swapChain);
    }
    return result;
}

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

bool InstallDx11Hook() {
    WNDCLASSA wc{};
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AC5ToolsDummyWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,
                                               D3D_DRIVER_TYPE_HARDWARE,
                                               nullptr,
                                               0,
                                               levels,
                                               2,
                                               D3D11_SDK_VERSION,
                                               &sd,
                                               &swapChain,
                                               &device,
                                               &featureLevel,
                                               &context);
    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        LogError("D3D11CreateDeviceAndSwapChain dummy creation failed.");
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(swapChain);
    void* present = vtable[8];
    void* resizeBuffers = vtable[13];

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed.");
    }
    if (MH_CreateHook(present, &HookPresent, reinterpret_cast<void**>(&g_originalPresent)) == MH_OK &&
        MH_EnableHook(present) == MH_OK) {
        Log("DX11 Present hook installed.");
    } else {
        LogError("DX11 Present hook failed.");
    }
    if (MH_CreateHook(resizeBuffers, &HookResizeBuffers, reinterpret_cast<void**>(&g_originalResizeBuffers)) == MH_OK &&
        MH_EnableHook(resizeBuffers) == MH_OK) {
        Log("DX11 ResizeBuffers hook installed.");
    } else {
        LogError("DX11 ResizeBuffers hook failed.");
    }

    swapChain->Release();
    context->Release();
    device->Release();
    DestroyWindow(hwnd);
    return true;
}

HRESULT __stdcall HookGetDeviceState(IDirectInputDevice8A* device, DWORD dataSize, LPVOID data) {
    GetDeviceStateFn original = nullptr;
    if (IsMouseDevice(device)) {
        original = g_originalGetDeviceStateMouse ? g_originalGetDeviceStateMouse : g_originalGetDeviceStateKeyboard;
    } else {
        original = g_originalGetDeviceStateKeyboard ? g_originalGetDeviceStateKeyboard : g_originalGetDeviceStateMouse;
    }
    if (!original) {
        return DIERR_NOTINITIALIZED;
    }
    const HRESULT result = original(device, dataSize, data);
    if (SUCCEEDED(result) && data && dataSize > 0 && g_menuOpen) {
        if ((IsMouseInputCaptureActive() && IsMouseDevice(device)) ||
            (IsKeyboardInputCaptureActive() && IsKeyboardDevice(device))) {
            memset(data, 0, dataSize);
            return DI_OK;
        }
    }
    return result;
}

HRESULT __stdcall HookGetDeviceData(IDirectInputDevice8A* device,
                                    DWORD objectDataSize,
                                    LPDIDEVICEOBJECTDATA objectData,
                                    LPDWORD inOut,
                                    DWORD flags) {
    GetDeviceDataFn original = nullptr;
    if (IsMouseDevice(device)) {
        original = g_originalGetDeviceDataMouse ? g_originalGetDeviceDataMouse : g_originalGetDeviceDataKeyboard;
    } else {
        original = g_originalGetDeviceDataKeyboard ? g_originalGetDeviceDataKeyboard : g_originalGetDeviceDataMouse;
    }
    if (!original) {
        return DIERR_NOTINITIALIZED;
    }
    const HRESULT result = original(device, objectDataSize, objectData, inOut, flags);
    if (SUCCEEDED(result) && inOut && g_menuOpen) {
        if ((IsMouseInputCaptureActive() && IsMouseDevice(device)) ||
            (IsKeyboardInputCaptureActive() && IsKeyboardDevice(device))) {
            *inOut = 0;
            return DI_OK;
        }
    }
    return result;
}

SHORT WINAPI HookGetAsyncKeyState(int vk) {
    if (ShouldBlockPolledKeyboardKey(vk)) {
        return 0;
    }
    return g_originalGetAsyncKeyState(vk);
}

SHORT WINAPI HookGetKeyState(int vk) {
    if (ShouldBlockPolledKeyboardKey(vk)) {
        return 0;
    }
    return g_originalGetKeyState(vk);
}

BOOL WINAPI HookGetKeyboardState(PBYTE keyState) {
    const BOOL result = g_originalGetKeyboardState(keyState);
    if (result && keyState && IsKeyboardInputCaptureActive()) {
        for (int vk = 0; vk < 256; ++vk) {
            if (ShouldBlockPolledKeyboardKey(vk)) {
                keyState[vk] = 0;
            }
        }
    }
    return result;
}

bool InstallDirectInputHook() {
    IDirectInput8A* directInput = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandleA(nullptr),
                                    DIRECTINPUT_VERSION,
                                    IID_IDirectInput8A,
                                    reinterpret_cast<void**>(&directInput),
                                    nullptr);
    if (FAILED(hr) || !directInput) {
        LogError("DirectInput8Create failed for hook discovery.");
        return false;
    }

    IDirectInputDevice8A* mouse = nullptr;
    hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
    if (FAILED(hr) || !mouse) {
        directInput->Release();
        LogError("DirectInput CreateDevice(GUID_SysMouse) failed for hook discovery.");
        return false;
    }

    IDirectInputDevice8A* keyboard = nullptr;
    hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
    if (FAILED(hr) || !keyboard) {
        mouse->Release();
        directInput->Release();
        LogError("DirectInput CreateDevice(GUID_SysKeyboard) failed for hook discovery.");
        return false;
    }

    void** mouseVtable = *reinterpret_cast<void***>(mouse);
    void** keyboardVtable = *reinterpret_cast<void***>(keyboard);
    g_mouseGetDeviceStateTarget = mouseVtable[9];
    g_mouseGetDeviceDataTarget = mouseVtable[10];
    g_keyboardGetDeviceStateTarget = keyboardVtable[9];
    g_keyboardGetDeviceDataTarget = keyboardVtable[10];

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for DirectInput.");
    }

    if (MH_CreateHook(g_mouseGetDeviceStateTarget, &HookGetDeviceState, reinterpret_cast<void**>(&g_originalGetDeviceStateMouse)) == MH_OK &&
        MH_EnableHook(g_mouseGetDeviceStateTarget) == MH_OK) {
        Log("DirectInput mouse GetDeviceState hook installed.");
    } else {
        LogError("DirectInput mouse GetDeviceState hook failed.");
    }
    if (MH_CreateHook(g_mouseGetDeviceDataTarget, &HookGetDeviceData, reinterpret_cast<void**>(&g_originalGetDeviceDataMouse)) == MH_OK &&
        MH_EnableHook(g_mouseGetDeviceDataTarget) == MH_OK) {
        Log("DirectInput mouse GetDeviceData hook installed.");
    } else {
        LogError("DirectInput mouse GetDeviceData hook failed.");
    }

    if (g_keyboardGetDeviceStateTarget == g_mouseGetDeviceStateTarget) {
        g_originalGetDeviceStateKeyboard = g_originalGetDeviceStateMouse;
    } else if (MH_CreateHook(g_keyboardGetDeviceStateTarget, &HookGetDeviceState, reinterpret_cast<void**>(&g_originalGetDeviceStateKeyboard)) == MH_OK &&
               MH_EnableHook(g_keyboardGetDeviceStateTarget) == MH_OK) {
        Log("DirectInput keyboard GetDeviceState hook installed.");
    } else {
        LogError("DirectInput keyboard GetDeviceState hook failed.");
    }

    if (g_keyboardGetDeviceDataTarget == g_mouseGetDeviceDataTarget) {
        g_originalGetDeviceDataKeyboard = g_originalGetDeviceDataMouse;
    } else if (MH_CreateHook(g_keyboardGetDeviceDataTarget, &HookGetDeviceData, reinterpret_cast<void**>(&g_originalGetDeviceDataKeyboard)) == MH_OK &&
               MH_EnableHook(g_keyboardGetDeviceDataTarget) == MH_OK) {
        Log("DirectInput keyboard GetDeviceData hook installed.");
    } else {
        LogError("DirectInput keyboard GetDeviceData hook failed.");
    }

    keyboard->Release();
    mouse->Release();
    directInput->Release();
    return true;
}

bool InstallKeyboardStateHooks() {
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        Log("user32.dll not available for keyboard state hooks.");
        return false;
    }
    void* getAsyncKeyState = reinterpret_cast<void*>(GetProcAddress(user32, "GetAsyncKeyState"));
    void* getKeyState = reinterpret_cast<void*>(GetProcAddress(user32, "GetKeyState"));
    void* getKeyboardState = reinterpret_cast<void*>(GetProcAddress(user32, "GetKeyboardState"));
    if (!getAsyncKeyState || !getKeyState || !getKeyboardState) {
        LogError("Keyboard state hook discovery failed.");
        return false;
    }

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MinHook initialize failed for keyboard state hooks.");
    }
    if (MH_CreateHook(getAsyncKeyState, &HookGetAsyncKeyState, reinterpret_cast<void**>(&g_originalGetAsyncKeyState)) == MH_OK &&
        MH_EnableHook(getAsyncKeyState) == MH_OK) {
        Log("GetAsyncKeyState hook installed.");
    } else {
        LogError("GetAsyncKeyState hook failed.");
    }
    if (MH_CreateHook(getKeyState, &HookGetKeyState, reinterpret_cast<void**>(&g_originalGetKeyState)) == MH_OK &&
        MH_EnableHook(getKeyState) == MH_OK) {
        Log("GetKeyState hook installed.");
    } else {
        LogError("GetKeyState hook failed.");
    }
    if (MH_CreateHook(getKeyboardState, &HookGetKeyboardState, reinterpret_cast<void**>(&g_originalGetKeyboardState)) == MH_OK &&
        MH_EnableHook(getKeyboardState) == MH_OK) {
        Log("GetKeyboardState hook installed.");
    } else {
        LogError("GetKeyboardState hook failed.");
    }
    return true;
}

DWORD WINAPI MainThread(void*) {
    if (!IsTargetGameProcess()) {
        return 0;
    }
    InitModuleDir();
    InitGameInfo();
    LoadConfig();
    Log("AC5Tools starting.");
    CheckSupportedGameFingerprint();
    InstallGameplayPatches();
    InstallDx11Hook();
    InstallDirectInputHook();
    InstallKeyboardStateHooks();
    for (;;) {
        Sleep(250);
        ApplyShipOptions();
        ApplyStealthMode();
        ApplyPlayerGodmode();
        ApplyBytePatchToggles();
        MaintainInventoryEditState();
        UpdateInventoryEditQueuedFlag();
        MaintainUnlocks();
    }
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (InterlockedCompareExchange(&g_mainStarted, 1, 0) == 0) {
            HANDLE thread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
            if (thread) {
                CloseHandle(thread);
            }
        }
    }
    return TRUE;
}
