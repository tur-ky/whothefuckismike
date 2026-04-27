#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>

// Forward declarations for Voicemeeter Remote API function pointers
typedef long(__stdcall *TVBVMR_Login)(void);

typedef long(__stdcall *TVBVMR_Logout)(void);

typedef long(__stdcall *TVBVMR_GetParameterFloat)(char *szParamName, float *pValue);

typedef long(__stdcall *TVBVMR_SetParameterFloat)(char *szParamName, float Value);

typedef long(__stdcall *TVBVMR_IsParametersDirty)(void);

typedef long(__stdcall *TVBVMR_GetLevel)(long nType, long nChannel, float *pValue);

class VMWrapper
{
  public:
    VMWrapper();

    ~VMWrapper();

    bool Initialize();

    void Shutdown();

    // Set a parameter from the UI
    void SetParameter(const std::string &paramName, float value);

    // Get a parameter from the internal cache
    float GetParameter(const std::string &paramName);

    // Get real-time audio level (0=pre-fader input, 1=post-fader input, 2=post-mute input, 3=output)
    float GetLevel(int nType, int nChannel);

    // Register a callback for when parameters change (from the polling thread)
    void SetUpdateCallback(std::function<void(const std::string &, float)> callback);

  private:
    void PollingThread();

    void BuildTrackedParams();

    HMODULE hDLL;

    TVBVMR_Login iVMR_Login;

    TVBVMR_Logout iVMR_Logout;

    TVBVMR_IsParametersDirty iVMR_IsParametersDirty;

    TVBVMR_GetParameterFloat iVMR_GetParameterFloat;

    TVBVMR_SetParameterFloat iVMR_SetParameterFloat;

    TVBVMR_GetLevel iVMR_GetLevel;

    std::atomic<bool> isRunning;

    std::thread pollThread;

    std::mutex cacheMutex;

    std::unordered_map<std::string, float> paramCache;

    std::vector<std::string> trackedParams;

    std::function<void(const std::string &, float)> onUpdateCallback;
};
