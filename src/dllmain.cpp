#include "stdafx.h"
#include "helper.hpp"
#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);

// Ini parser setup
inipp::Ini<char> ini;
std::string sFixName = "SO4Fix";
std::string sFixVer = "0.9.0";
std::string sLogFile = "SO4Fix.log";
std::string sConfigFile = "SO4Fix.ini";
std::string sExeName;
std::filesystem::path sExePath;
std::pair DesktopDimensions = { 0,0 };

// Ini variables
bool bCustomRes;
int iCustomResX;
int iCustomResY;
bool bBorderlessMode;
bool bIntroSkip;
bool bFixUI;
bool bFixFOV;
bool bFixShadowBug;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fDefaultHUDWidth = (float)1920;
float fDefaultHUDHeight = (float)1080;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
const char* sWindowClassName = "AskaWnd";
uintptr_t BattleMarkerRightValue;
uintptr_t BattleMarkerFlipValue;
float fCurrentFrametime = 0.0166667f;
int iWindowMode = 0;

void Logging()
{
    // spdlog initialisation
    {
        try
        {
            auto logger = spdlog::basic_logger_mt(sFixName.c_str(), sLogFile, true);
            spdlog::set_default_logger(logger);

        }
        catch (const spdlog::spdlog_ex& ex)
        {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
        }
    }

    spdlog::flush_on(spdlog::level::debug);
    spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
    spdlog::info("----------");

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();

    // Log module details
    spdlog::info("Module Name: {0:s}", sExeName.c_str());
    spdlog::info("Module Path: {0:s}", sExePath.string().c_str());
    spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
    spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
    spdlog::info("----------");
}

void ReadConfig()
{
    // Initialise config
    std::ifstream iniFile(sConfigFile);
    if (!iniFile)
    {
        spdlog::critical("Failed to load config file.");
        spdlog::critical("Make sure {} is present in the game folder.", sConfigFile);
    }
    else
    {
        ini.parse(iniFile);
    }

    // Read ini file
    inipp::get_value(ini.sections["Custom Resolution"], "Enabled", bCustomRes);
    inipp::get_value(ini.sections["Custom Resolution"], "Width", iCustomResX);
    inipp::get_value(ini.sections["Custom Resolution"], "Height", iCustomResY);
    inipp::get_value(ini.sections["Custom Resolution"], "Borderless", bBorderlessMode);
    inipp::get_value(ini.sections["Fix UI"], "Enabled", bFixUI);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    inipp::get_value(ini.sections["Fix Shadow Buffer Bug"], "Enabled", bFixShadowBug);

    // Log config parse
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);
    spdlog::info("Config Parse: bBorderlessMode: {}", bBorderlessMode);
    spdlog::info("Config Parse: bFixUI: {}", bFixUI);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);
    spdlog::info("Config Parse: bFixShadowBug: {}", bFixShadowBug);
    spdlog::info("----------");

    // Calculate aspect ratio / use desktop res instead
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();

    if (iCustomResX > 0 && iCustomResY > 0)
    {
        fAspectRatio = (float)iCustomResX / (float)iCustomResY;
    }
    else
    {
        iCustomResX = (int)DesktopDimensions.first;
        iCustomResY = (int)DesktopDimensions.second;
        fAspectRatio = (float)DesktopDimensions.first / (float)DesktopDimensions.second;
        spdlog::info("Custom Resolution: iCustomResX: Desktop Width: {}", iCustomResX);
        spdlog::info("Custom Resolution: iCustomResY: Desktop Height: {}", iCustomResY);
    }
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCustomResY * fNativeAspect;
    fHUDHeight = (float)iCustomResY;
    fHUDWidthOffset = (float)(iCustomResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect)
    {
        fHUDWidth = (float)iCustomResX;
        fHUDHeight = (float)iCustomResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCustomResY - fHUDHeight) / 2;
    }

    // Log aspect ratio stuff
    spdlog::info("Custom Resolution: fAspectRatio: {}", fAspectRatio);
    spdlog::info("Custom Resolution: fAspectMultiplier: {}", fAspectMultiplier);
    spdlog::info("Custom Resolution: fHUDWidth: {}", fHUDWidth);
    spdlog::info("Custom Resolution: fHUDHeight: {}", fHUDHeight);
    spdlog::info("Custom Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
    spdlog::info("Custom Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
    spdlog::info("----------");
}

void IntroSkip()
{
    // Intro Skip
    uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "89 ?? ?? 48 89 ?? ?? 88 ?? ?? 48 89 ?? ?? 48 89 ?? ?? 48 89 ?? ?? 48 89 ?? ??") + 0x7;
    if (IntroSkipScanResult)
    {
        spdlog::info("Intro Skip: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult - (uintptr_t)baseModule);

        // Skip intro logos
        static SafetyHookMid IntroSkipMidHook{};
        IntroSkipMidHook = safetyhook::create_mid(IntroSkipScanResult,
            [](SafetyHookContext& ctx)
            {
                ctx.rdx = 1;
            });
    }
    else if (!IntroSkipScanResult)
    {
        spdlog::error("Intro Skip: Pattern scan failed.");
    }
}

// CSetWindowLongA Hook
SafetyHookInline SetWindowLongA_hook{};
LONG WINAPI SetWindowLongA_hooked(HWND hWnd, int nIndex, LONG dwNewLong)
{
    auto windowLong = SetWindowLongA_hook.stdcall<LONG>(hWnd, nIndex, dwNewLong);

    // Get window class name
    char windowClassName[256];
    GetClassNameA(hWnd, windowClassName, sizeof(windowClassName));

    // Check if game is in windowed mode and that the class name is correct.
    if (iWindowMode == 0 && !strcmp(windowClassName, sWindowClassName) && bBorderlessMode)
    {
        // Get window style
        LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);

        // Check for borderless (WS_POPUP)
        if ((lStyle & WS_POPUP) != WS_POPUP)
        {
            // Apply borderless style (WS_POPUP)
            lStyle &= ~(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
            SetWindowLong(hWnd, GWL_STYLE, lStyle);

            // Maximize window and put window on top
            SetWindowPos(hWnd, HWND_TOP, 0, 0, DesktopDimensions.first, DesktopDimensions.second, NULL);

            // Set window focus
            SetFocus(hWnd);
        }
    }

    return windowLong;
}

void Resolution()
{
    if (bCustomRes)
    {
        // Apply custom resolution
        uint8_t* ApplyResolutionScanResult = Memory::PatternScan(baseModule, "40 ?? 48 ?? ?? ?? 8B ?? 49 ?? ?? 89 ?? ?? 8B ?? ?? 89 ?? ??");
        if (ApplyResolutionScanResult)
        {
            spdlog::info("Custom Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ApplyResolutionScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ApplyResolutionMidHook{};
            ApplyResolutionMidHook = safetyhook::create_mid(ApplyResolutionScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (ctx.r8 && ctx.rdx)
                    {
                        // Internal resolution
                        *reinterpret_cast<short*>(ctx.r8) = (short)iCustomResX;
                        *reinterpret_cast<short*>(ctx.r8 + 0x2) = (short)iCustomResY;

                        // Window size
                        *reinterpret_cast<short*>(ctx.rdx) = (short)iCustomResX;
                        *reinterpret_cast<short*>(ctx.rdx + 0x2) = (short)iCustomResY;

                        spdlog::info("Custom Resolution: Applied custom resolution: Window: {}x{} | Internal: {}x{}", iCustomResX, iCustomResY, iCustomResX, iCustomResY);
                    }
                });
        }
        else if (!ApplyResolutionScanResult)
        {
            spdlog::error("Custom Resolution: Pattern scan failed.");
        }

        if (bBorderlessMode)
        {
            // Get window mode.
            uint8_t* WindowedModeScanResult = Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 48 ?? ?? F6 ?? 1B ?? 83 ?? 02") + 0x5;
            if (WindowedModeScanResult)
            {
                spdlog::info("Windowed Mode: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)WindowedModeScanResult - (uintptr_t)baseModule);

                static SafetyHookMid WindowedModeMidHook{};
                WindowedModeMidHook = safetyhook::create_mid(WindowedModeScanResult,
                    [](SafetyHookContext& ctx)
                    {
                        // Grab window mode
                        iWindowMode = (int)ctx.rax;
                    });
            }
            else if (!WindowedModeScanResult)
            {
                spdlog::error("Windowed Mode: Pattern scan failed.");
            }

            // Hook SetWindowLongA.
            SetWindowLongA_hook = safetyhook::create_inline(&SetWindowLongA, reinterpret_cast<void*>(SetWindowLongA_hooked));
        }
    }
}

void HUD()
{
    // HUD Width
    uint8_t* HUDWidthScanResult = Memory::PatternScan(baseModule, "0F B7 ?? ?? 66 0F ?? ?? 0F 5B ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? 75 ?? 48 ?? ?? E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ??") + 0xB;
    if (HUDWidthScanResult)
    {
        spdlog::info("HUD: HUD Width: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDWidthScanResult - (uintptr_t)baseModule);

        static SafetyHookMid HUDWidthMidHook{};
        HUDWidthMidHook = safetyhook::create_mid(HUDWidthScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm0.f32[0] = (float)iCustomResY * fNativeAspect;
                }
            });
    }
    else if (!HUDWidthScanResult)
    {
        spdlog::error("HUD: HUD Width: Pattern scan failed.");
    }

    // Menu Backgrounds
    uint8_t* MenuBackgroundsScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 44 ?? ?? ?? ?? 0F ?? ?? 0A 73 ??");
    if (MenuBackgroundsScanResult)
    {
        spdlog::info("HUD: Menu Backgrounds: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MenuBackgroundsScanResult - (uintptr_t)baseModule);

        static SafetyHookMid MenuBackgroundsMidHook{};
        MenuBackgroundsMidHook = safetyhook::create_mid(MenuBackgroundsScanResult,
            [](SafetyHookContext& ctx)
            {
                if (ctx.rdi + 0x80)
                {
                    // Check for 1280x800 background
                    if (*reinterpret_cast<float*>(ctx.rdi + 0x7C) == 1280.00f && *reinterpret_cast<float*>(ctx.rdi + 0x80) == 800.00f)
                    {
                        if (fAspectRatio > fNativeAspect)
                        {
                            *reinterpret_cast<float*>(ctx.rdi + 0x7C) = 720.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdi + 0x18) = -(((720.00f * fAspectRatio) - 1280.00f) / 2.00f);
                        }
                        else if (fAspectRatio < 1.60f)
                        {
                            *reinterpret_cast<float*>(ctx.rdi + 0x80) = 1280.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdi + 0x1C) = -(((1280.00f / fAspectRatio) - 720.00f) / 2.00f);
                        }
                    }
                }
            });
    }
    else if (!MenuBackgroundsScanResult)
    {
        spdlog::error("HUD: Menu Backgrounds: Pattern scan failed.");
    }

    // 2D Scissoring
    uint8_t* HUDScissorScanResult = Memory::PatternScan(baseModule, "0F 28 ?? F3 0F ?? ?? F3 41 ?? ?? ?? F3 0F ?? ?? 0F 28 ?? F3 0F ?? ?? F3 41 ?? ?? ??") - 0x3;
    if (HUDScissorScanResult)
    {
        spdlog::info("HUD: HUD Scissor: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDScissorScanResult - (uintptr_t)baseModule);

        static SafetyHookMid HUDScissorMidHook{};
        HUDScissorMidHook = safetyhook::create_mid(HUDScissorScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm2.f32[0] = fHUDWidth / 1280.00f;
                    ctx.xmm8.f32[0] += ((720.00f * fAspectRatio) - 1280.00f) / 2.00f;
                    ctx.xmm9.f32[0] += ((720.00f * fAspectRatio) - 1280.00f) / 2.00f;
                }
                else if (fAspectRatio < 1.60f)
                {
                    ctx.xmm6.f32[0] += ((1280.00f / fAspectRatio) - 720.00f) / 2.00f;
                    ctx.xmm7.f32[0] += ((1280.00f / fAspectRatio) - 720.00f) / 2.00f;
                }
            });
    }
    else if (!HUDScissorScanResult)
    {
        spdlog::error("HUD: HUD Scissor: Pattern scan failed.");
    }

    // Minimap Compass
    uint8_t* MinimapCompassScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ??  0F B7 ?? ?? 48 ?? ??");
    uint8_t* MinimapCompassNorthScanResult = Memory::PatternScan(baseModule, "48 ?? ?? E8 ?? ?? ?? ?? F3 0F ?? ?? ?? 0F 28 ?? ?? ?? ?? ?? ?? F3 44 ?? ?? ?? ??");
    if (MinimapCompassScanResult && MinimapCompassNorthScanResult)
    {
        spdlog::info("HUD: Minimap Compass: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MinimapCompassScanResult - (uintptr_t)baseModule);
        spdlog::info("HUD: Minimap Compass: North: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MinimapCompassScanResult - (uintptr_t)baseModule);

        static SafetyHookMid MinimapCompass1MidHook{};
        MinimapCompass1MidHook = safetyhook::create_mid(MinimapCompassScanResult + 0x36,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm0.f32[0] = fHUDWidth / 1280.00f;
                }
            });

        static SafetyHookMid MinimapCompass2MidHook{};
        MinimapCompass2MidHook = safetyhook::create_mid(MinimapCompassScanResult + 0x9B,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm0.f32[0] = fHUDWidth / 1280.00f;
                }
            });

        static SafetyHookMid MinimapCompass3MidHook{};
        MinimapCompass3MidHook = safetyhook::create_mid(MinimapCompassScanResult + 0x10D,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm7.f32[0] = fHUDWidth;
                }
            });

        static SafetyHookMid MinimapCompassNarrowMidHook{};
        MinimapCompassNarrowMidHook = safetyhook::create_mid(MinimapCompassScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio < 1.60f)
                {
                    ctx.xmm7.f32[0] = ((1280.00f / fAspectRatio) - 720.00f) / 2.00f;
                }
            });

        // North marker on compass
        static SafetyHookMid MinimapCompassNorthMidHook{};
        MinimapCompassNorthMidHook = safetyhook::create_mid(MinimapCompassNorthScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm6.f32[0] = -0.05f * fHUDWidth + 72.00f; // Not sure on how they calculated this, but this formula produces very similar results.
                }
            });
    }
    else if (!MinimapCompassScanResult)
    {
        spdlog::error("HUD: Minimap Compass: Pattern scan failed.");
    }

    // Fades
    uint8_t* FadesScanResult = Memory::PatternScan(baseModule, "0F 57 ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? 00 00 80 3F") + 0x7;
    if (FadesScanResult)
    {
        spdlog::info("HUD: Fades: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FadesScanResult - (uintptr_t)baseModule);

        static SafetyHookMid FadesMidHook{};
        FadesMidHook = safetyhook::create_mid(FadesScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    float fWidthOffset = ((720.00f * fAspectRatio) - 1280.00f) / 2.00f;

                    ctx.xmm1.f32[0] = 0.00f;
                    ctx.xmm2.f32[0] = 720.00f;

                    if (ctx.rcx + 0x6D0 && ctx.rcx + 6E0)
                    {
                        *reinterpret_cast<float*>(ctx.rcx + 0x6D0) = -fWidthOffset;
                        *reinterpret_cast<float*>(ctx.rcx + 0x6E0) = -fWidthOffset;
                    }
                }
            });

        static SafetyHookMid FadesSizeMidHook{};
        FadesSizeMidHook = safetyhook::create_mid(FadesScanResult + 0x70, // Big gap but this game ain't getting updates, so who cares?
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    float fWidthOffset = ((720.00f * fAspectRatio) - 1280.00f) / 2.00f;

                    if (ctx.rcx + 0x6F0 && ctx.rcx + 0x700)
                    {
                        *reinterpret_cast<float*>(ctx.rcx + 0x6F0) = (720.00f * fAspectRatio) - fWidthOffset;
                        *reinterpret_cast<float*>(ctx.rcx + 0x700) = (720.00f * fAspectRatio) - fWidthOffset;
                    }
                }
            });
    }
    else if (!FadesScanResult)
    {
        spdlog::error("HUD: Fades: Pattern scan failed.");
    }

    // Battle Crossfades
    uint8_t* BattleCrossfadesScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? F3 0F ?? ?? 66 0F ?? ?? F3 41 ?? ?? ?? 0F 5B ?? F3 0F ?? ?? ?? ?? F3 41 ?? ?? ??");
    if (BattleCrossfadesScanResult)
    {
        spdlog::info("HUD: Battle Crossfades: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)BattleCrossfadesScanResult - (uintptr_t)baseModule);

        static SafetyHookMid BattleCrossfadesMidHook{};
        BattleCrossfadesMidHook = safetyhook::create_mid(BattleCrossfadesScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm3.f32[0] *= fAspectMultiplier;
                }
                else if (fAspectRatio < 1.60f)
                {
                    ctx.xmm3.f32[0] /= fAspectMultiplier;
                }
            });
    }
    else if (!BattleCrossfadesScanResult)
    {
        spdlog::error("HUD: Battle Crossfades: Pattern scan failed.");
    }

    // Markers (e.g. target markers, main menu cursor)
    uint8_t* MarkersScanResult = Memory::PatternScan(baseModule, "0F 5B ?? F3 0F ?? ?? F3 0F ?? ?? 0F 28 ?? 0F 28 ?? F3 0F ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? ??") + 0x3;
    if (MarkersScanResult)
    {
        spdlog::info("HUD: Markers: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MarkersScanResult - (uintptr_t)baseModule);

        // >16:9
        static SafetyHookMid MarkersWidth1MidHook{};
        MarkersWidth1MidHook = safetyhook::create_mid(MarkersScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm0.f32[0] = fHUDWidth;
                }
            });

        static SafetyHookMid MarkersWidth2MidHook{};
        MarkersWidth2MidHook = safetyhook::create_mid(MarkersScanResult + 0x6C,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm1.f32[0] = fHUDWidth;
                }
            });

        static SafetyHookMid MarkersOffsetMidHook{};
        MarkersOffsetMidHook = safetyhook::create_mid(MarkersScanResult + 0x1C,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    ctx.xmm0.f32[0] -= ((720.00f * fAspectRatio) - 1280.00f) / 2.00f;
                }
            });

        // <16:9
        static SafetyHookMid MarkersNarrow1MidHook{};
        MarkersNarrow1MidHook = safetyhook::create_mid(MarkersScanResult + 0x41,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio < 1.60f)
                {
                    ctx.rax = ctx.rcx;
                }
            });

        static SafetyHookMid MarkersNarrow2MidHook{};
        MarkersNarrow2MidHook = safetyhook::create_mid(MarkersScanResult + 0x4D,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio < 1.60f)
                {
                    ctx.xmm2.f32[0] += 40.00f;
                    ctx.xmm2.f32[0] -= ((1280.00f / fAspectRatio) - 720.00f) / 2.00f; // 40.00f at 1920x1200 for example
                }
            });
    }
    else if (!MarkersScanResult)
    {
        spdlog::error("HUD: Markers: Pattern scan failed.");
    }

    // Allow battle markers to leave 16:9 boundary
    uint8_t* BattleMarkersScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? 0F ?? ?? 77 ?? 0F ?? ?? ?? ?? ?? ?? 77 ??");
    uint8_t* BattleMarkersEdgeFlipScanResult = Memory::PatternScan(baseModule, "7F ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 76 ?? F3 0F ?? ??");
    if (BattleMarkersScanResult && BattleMarkersEdgeFlipScanResult)
    {
        spdlog::info("HUD: Battle Markers Boundary: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)BattleMarkersScanResult - (uintptr_t)baseModule);
        spdlog::info("HUD: Battle Markers Edge Flip: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)BattleMarkersEdgeFlipScanResult - (uintptr_t)baseModule);

        // Left edge
        static SafetyHookMid BattleMarkersLeft1MidHook{};
        BattleMarkersLeft1MidHook = safetyhook::create_mid(BattleMarkersScanResult,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    // Set left side to 80 - the hud width offset.
                    ctx.xmm3.f32[0] = 80.00f - (((720.00f * fAspectRatio) - 1280.00f) / 2.00f); // -80
                }
            });

        static SafetyHookMid BattleMarkersLeft2MidHook{};
        BattleMarkersLeft2MidHook = safetyhook::create_mid(BattleMarkersScanResult + 0xBB,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    // Set left side to 80 - the hud width offset.
                    ctx.xmm1.f32[0] = 80.00f - (((720.00f * fAspectRatio) - 1280.00f) / 2.00f); // 80
                }
            });

        // Need to grab the address for the right edge value as it's used in a comiss. Luckily it isn't used anywhere else.
        BattleMarkerRightValue = Memory::GetAbsolute((uintptr_t)BattleMarkersScanResult + 0xE);

        // Need to grab address for the marker flip at the right edge of the screen.
        BattleMarkerFlipValue = Memory::GetAbsolute((uintptr_t)BattleMarkersEdgeFlipScanResult + 0x6);

        // Right edge
        static SafetyHookMid BattleMarkersRightMidHook{};
        BattleMarkersRightMidHook = safetyhook::create_mid(BattleMarkersScanResult + 0xCD,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio > fNativeAspect)
                {
                    // Need to leave the 80px margin
                    if (BattleMarkerRightValue && BattleMarkerFlipValue)
                    {
                        Memory::Write(BattleMarkerRightValue, 1280.00f + (((720.00f * fAspectRatio) - 1280.00f) / 2.00f));
                        Memory::Write(BattleMarkerFlipValue, 900.00f + (((720.00f * fAspectRatio) - 1280.00f) / 2.00f));
                    }
                    ctx.xmm0.f32[0] = 1200.00f + (((720.00f * fAspectRatio) - 1280.00f) / 2.00f);
                }
            });
    }
    else if (!BattleMarkersScanResult || !BattleMarkersEdgeFlipScanResult)
    {
        spdlog::error("HUD: Battle Markers Boundary: Pattern scan failed.");
    }

    // Movies
    uint8_t* MovieTextureScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? F3 0F ?? ?? F3 41 ?? ?? ?? 44 0F ?? ?? ?? ?? 0F 28 ??") + 0x5;
    if (MovieTextureScanResult)
    {
        spdlog::info("HUD: Movie: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieTextureScanResult - (uintptr_t)baseModule);

        static SafetyHookMid MovieTextureMidHook{};
        MovieTextureMidHook = safetyhook::create_mid(MovieTextureScanResult,
            [](SafetyHookContext& ctx)
            {
                if (ctx.rbx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        *reinterpret_cast<float*>(ctx.rbx) = -1.00f / fAspectMultiplier;
                        *reinterpret_cast<float*>(ctx.rbx + 0x1C) = -1.00f / fAspectMultiplier;
                        *reinterpret_cast<float*>(ctx.rbx + 0x38) = 1.00f / fAspectMultiplier;
                        *reinterpret_cast<float*>(ctx.rbx + 0x54) = 1.00f / fAspectMultiplier;
                    }
                }
            });

        static SafetyHookMid MovieTextureNarrowMidHook{};
        MovieTextureNarrowMidHook = safetyhook::create_mid(MovieTextureScanResult - 0x8C,
            [](SafetyHookContext& ctx)
            {
                if (fAspectRatio < 1.60f)
                {
                    ctx.xmm6.f32[0] = fAspectMultiplier;
                }
            });
    }
    else if (!MovieTextureScanResult)
    {
        spdlog::error("HUD: Movie: Pattern scan failed.");
    }
}

void FOV()
{
    if (bFixFOV)
    {
        // Field of View
        uint8_t* FOVScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? F3 44 ?? ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? F3 44 ?? ?? ?? ?? ?? ?? ?? F3 41 ?? ?? ??");
        if (FOVScanResult)
        {
            spdlog::info("FOV: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FOVScanResult - (uintptr_t)baseModule);

            static SafetyHookMid FOVMidHook{};
            FOVMidHook = safetyhook::create_mid(FOVScanResult,
                [](SafetyHookContext& ctx)
                {  
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm2.f32[0] *= 1.00f / fAspectMultiplier;
                    }
                });
        }
        else if (!FOVScanResult)
        {
            spdlog::error("FOV: Pattern scan failed.");
        }
    }
}

void Graphics()
{
    if (bFixShadowBug)
    {
        // Shadow Resolution Bug
        uint8_t* ShadowResolutionBugScanResult = Memory::PatternScan(baseModule, "41 ?? 00 08 00 00 41 ?? 00 10 00 00 0F ?? ?? ?? ?? ?? ?? ??");
        if (ShadowResolutionBugScanResult)
        {
            spdlog::info("Shadow Resolution Bug: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowResolutionBugScanResult - (uintptr_t)baseModule);

            // "Shadow Buffer" 4x option is bugged. It clamps the shadow resolution to 2048 instead of 4096. So 1x would be (1024,1024), 2x is (2048, 2048) and 4x is (2048,2048). Can you spot the issue?
            Memory::Write((uintptr_t)ShadowResolutionBugScanResult + 0x2, (int)4096);
            spdlog::info("Shadow Resolution Bug: Patched instruction.");
        }
        else if (!ShadowResolutionBugScanResult)
        {
            spdlog::error("Shadow Resolution Bug: Pattern scan failed.");
        }
    }
}

/*
void FPSCap()
{
    // Current frametime
    uint8_t* CurrentFrametimeScanResult = Memory::PatternScan(baseModule, "F2 0F ?? ?? 89 ?? ?? ?? ?? ?? ?? 48 8B ?? ?? ?? 48 83 ?? ?? 5F C3");
    if (CurrentFrametimeScanResult)
    {
        spdlog::info("CurrentFrametime: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentFrametimeScanResult - (uintptr_t)baseModule);

        static SafetyHookMid CurrentFrametimeMidHook{};
        CurrentFrametimeMidHook = safetyhook::create_mid(CurrentFrametimeScanResult,
            [](SafetyHookContext& ctx)
            {
                // TODO                
            });
    }
    else if (!CurrentFrametimeScanResult)
    {
        spdlog::error("CurrentFrametime: Pattern scan failed.");
    }

    // FPS Cap
    uint8_t* FPSCapScanResult = Memory::PatternScan(baseModule, "48 ?? ?? ?? ?? ?? ?? 48 8D ?? ?? 00 00 00 00 48 89 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? FF 15 ?? ?? ?? ??");
    if (FPSCapScanResult)
    {
        spdlog::info("FPSCap: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FPSCapScanResult - (uintptr_t)baseModule);

        static SafetyHookMid FPSCapMidHook{};
        FPSCapMidHook = safetyhook::create_mid(FPSCapScanResult,
            [](SafetyHookContext& ctx)
            {
                // Set 240fps cap
                ctx.rax = 41666; // 166667
            });
    }
    else if (!FPSCapScanResult)
    {
        spdlog::error("FPSCap: Pattern scan failed.");
    }

    // Game Speed
    uint8_t* GameSpeedScanResult = Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? F3 0F ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? ?? ??");
    uint8_t* GameSpeedIntScanResult = Memory::PatternScan(baseModule, "B8 3C 00 00 00 F7 ?? ?? ?? ?? ?? 41 ?? ??") + 0x12;
    if (GameSpeedScanResult && GameSpeedIntScanResult)
    {
        spdlog::info("GameSpeed: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameSpeedScanResult - (uintptr_t)baseModule);
        uintptr_t GameSpeedFuncAddr = Memory::GetAbsolute((uintptr_t)GameSpeedScanResult + 0x1);
        spdlog::info("GameSpeed: Function address is {:s}+{:x}", sExeName.c_str(), GameSpeedFuncAddr - (uintptr_t)baseModule);
        spdlog::info("GameSpeed Int: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameSpeedIntScanResult - (uintptr_t)baseModule);

        static SafetyHookMid GameSpeedMidHook{};
        GameSpeedMidHook = safetyhook::create_mid(GameSpeedFuncAddr + 0x8,
            [](SafetyHookContext& ctx)
            {
                ctx.xmm0.f32[0] = 1.00f / fCurrentFrametime; // Current FPS
            });

        static SafetyHookMid GameSpeedIntMidHook{};
        GameSpeedIntMidHook = safetyhook::create_mid(GameSpeedIntScanResult,
            [](SafetyHookContext& ctx)
            {
                ctx.r8 = 1;
            });
    }
    else if (!GameSpeedScanResult || !GameSpeedIntScanResult)
    {
        spdlog::error("GameSpeed: Pattern scan failed.");
    }

    // Combat Speed
    uint8_t* CombatSpeedScanResult = Memory::PatternScan(baseModule, "0F ?? ?? F3 0F ?? ?? ?? 0F ?? ?? 48 ?? ?? ?? F2 ?? ?? ?? ?? ?? ??") + 0x3;
    if (CombatSpeedScanResult)
    {
        spdlog::info("CombatSpeed: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CombatSpeedScanResult - (uintptr_t)baseModule);

        static SafetyHookMid CombatSpeedMidHook{};
        CombatSpeedMidHook = safetyhook::create_mid(CombatSpeedScanResult,
            [](SafetyHookContext& ctx)
            {
                ctx.xmm1.f32[0] = 60.00f / (1.00f / fCurrentFrametime);
            });
    }
    else if (!CombatSpeedScanResult)
    {
        spdlog::error("CombatSpeed: Pattern scan failed.");
    }
}
*/

DWORD __stdcall Main(void*)
{
    Logging();
    ReadConfig();
    IntroSkip();
    Resolution();
    HUD();
    FOV();
    Graphics();
    return true; //end thread
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);

        if (mainHandle)
        {
            CloseHandle(mainHandle);
        }
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

