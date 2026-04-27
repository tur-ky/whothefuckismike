#include "vm_wrapper.hpp"
#include <chrono>
#include <iostream>

VMWrapper::VMWrapper()
    : hDLL(nullptr), isRunning(false), iVMR_Login(nullptr), iVMR_Logout(nullptr), iVMR_IsParametersDirty(nullptr),
      iVMR_GetParameterFloat(nullptr), iVMR_SetParameterFloat(nullptr), iVMR_GetLevel(nullptr)
{
    BuildTrackedParams();
}

VMWrapper::~VMWrapper()
{
    Shutdown();
}

void VMWrapper::BuildTrackedParams()
{
    // According to engine_map.json
    std::vector<std::string> strips = {"Strip[0]", "Strip[3]", "Strip[4]"};

    std::vector<std::string> stripParams = {
        "Mute", "Solo", "Gain", "Color_x", "Color_y", "Comp", "Gate", "Pan_x", "Pan_y", "EQGain1", "EQGain2", "EQGain3",
        "A1",   "A2",   "A3",   "A4",      "A5",      "B1",   "B2",   "B3",    "Mono",  "MC",      "K"};

    for (const auto &strip : strips)
    {
        for (const auto &param : stripParams)
        {
            trackedParams.push_back(strip + "." + param);
        }
    }

    // Buses
    std::vector<std::string> buses = {"Bus[0]", "Bus[1]", "Bus[2]", "Bus[3]", "Bus[4]"};

    std::vector<std::string> busParams = {"Mute", "EQ.on", "Gain"};

    for (const auto &bus : buses)
    {
        for (const auto &param : busParams)
        {
            trackedParams.push_back(bus + "." + param);
        }
    }
}

bool VMWrapper::Initialize()
{
#ifdef _WIN64
    const char *dllPath = "C:\\Program Files (x86)\\VB\\Voicemeeter\\VoicemeeterRemote64.dll";

#else
    const char *dllPath = "C:\\Program Files (x86)\\VB\\Voicemeeter\\VoicemeeterRemote.dll";

#endif

    hDLL = LoadLibraryA(dllPath);

    if (!hDLL)
    {
        hDLL = LoadLibraryA("VoicemeeterRemote64.dll");
        // Fallback
        if (!hDLL)
            return false;
    }

    iVMR_Login = (TVBVMR_Login)GetProcAddress(hDLL, "VBVMR_Login");

    iVMR_Logout = (TVBVMR_Logout)GetProcAddress(hDLL, "VBVMR_Logout");

    iVMR_IsParametersDirty = (TVBVMR_IsParametersDirty)GetProcAddress(hDLL, "VBVMR_IsParametersDirty");

    iVMR_GetParameterFloat = (TVBVMR_GetParameterFloat)GetProcAddress(hDLL, "VBVMR_GetParameterFloat");

    iVMR_SetParameterFloat = (TVBVMR_SetParameterFloat)GetProcAddress(hDLL, "VBVMR_SetParameterFloat");

    iVMR_GetLevel = (TVBVMR_GetLevel)GetProcAddress(hDLL, "VBVMR_GetLevel");

    if (!iVMR_Login || !iVMR_Logout || !iVMR_IsParametersDirty || !iVMR_GetParameterFloat || !iVMR_SetParameterFloat)
    {
        FreeLibrary(hDLL);

        hDLL = nullptr;

        return false;
    }

    if (iVMR_Login() == 0)
    {
        // Initial populate cache
        for (const auto &pName : trackedParams)
        {
            float val = 0.0f;

            if (iVMR_GetParameterFloat((char *)pName.c_str(), &val) == 0)
            {
                paramCache[pName] = val;
            }
            else
            {
                paramCache[pName] = 0.0f;
            }
        }

        isRunning = true;

        pollThread = std::thread(&VMWrapper::PollingThread, this);

        return true;
    }

    return false;
}

void VMWrapper::Shutdown()
{
    if (isRunning)
    {
        isRunning = false;

        if (pollThread.joinable())
        {
            pollThread.join();
        }
        if (iVMR_Logout)
        {
            iVMR_Logout();
        }
    }
    if (hDLL)
    {
        FreeLibrary(hDLL);

        hDLL = nullptr;
    }
}

void VMWrapper::SetParameter(const std::string &paramName, float value)
{
    if (!isRunning || !iVMR_SetParameterFloat)
        return;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        if (paramCache[paramName] != value)
        {
            paramCache[paramName] = value;

            iVMR_SetParameterFloat((char *)paramName.c_str(), value);
        }
    }
}

float VMWrapper::GetParameter(const std::string &paramName)
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = paramCache.find(paramName);

    if (it != paramCache.end())
    {
        return it->second;
    }
    return 0.0f;
}

float VMWrapper::GetLevel(int nType, int nChannel)
{
    if (!isRunning || !iVMR_GetLevel)
        return 0.0f;

    float val = 0.0f;

    if (iVMR_GetLevel((long)nType, (long)nChannel, &val) == 0)
    {
        return val;
    }
    return 0.0f;
}

void VMWrapper::SetUpdateCallback(std::function<void(const std::string &, float)> callback)
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    onUpdateCallback = callback;
}

void VMWrapper::PollingThread()
{
    while (isRunning)
    {
        if (iVMR_IsParametersDirty() != 0)
        {
            for (const auto &pName : trackedParams)
            {
                float val = 0.0f;

                if (iVMR_GetParameterFloat((char *)pName.c_str(), &val) == 0)
                {
                    bool updated = false;

                    {
                        std::lock_guard<std::mutex> lock(cacheMutex);

                        if (paramCache[pName] != val)
                        {
                            paramCache[pName] = val;

                            updated = true;
                        }
                    }
                    if (updated)
                    {
                        std::function<void(const std::string &, float)> cb;

                        {
                            std::lock_guard<std::mutex> lock(cacheMutex);

                            cb = onUpdateCallback;
                        }
                        if (cb)
                        {
                            cb(pName, val);
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
