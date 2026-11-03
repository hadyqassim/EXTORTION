#pragma once
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#include <onnxruntime_cxx_api.h>

namespace a2m
{
// Windows ships an older, incompatible onnxruntime.dll in System32 that can win the DLL
// search and crash the plugin. On Windows we load OUR copy by full path (it sits next to
// the plugin binary) and hand the API table to the C++ wrapper (ORT_API_MANUAL_INIT).
// On other platforms ONNX Runtime is linked normally and there is nothing to do.
inline bool initOnnxRuntime (juce::String& error)
{
#if JUCE_WINDOWS
    const auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    const auto dll = dir.getChildFile ("onnxruntime.dll");

    if (! dll.existsAsFile())
    {
        error = "onnxruntime.dll not found next to the plugin: " + dll.getFullPathName();
        return false;
    }

    HMODULE mod = LoadLibraryExW (dll.getFullPathName().toWideCharPointer(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (mod == nullptr)
    {
        error = "Could not load onnxruntime.dll (error " + juce::String ((int) GetLastError()) + ").";
        return false;
    }

    using GetBaseFn = const OrtApiBase* (ORT_API_CALL*) ();
    auto getBase = (GetBaseFn) GetProcAddress (mod, "OrtGetApiBase");
    if (getBase == nullptr)
    {
        error = "onnxruntime.dll is missing OrtGetApiBase.";
        return false;
    }

    const OrtApi* api = getBase()->GetApi (ORT_API_VERSION);
    if (api == nullptr)
    {
        error = "onnxruntime.dll does not support the required API version.";
        return false;
    }

    Ort::InitApi (api);
#else
    juce::ignoreUnused (error);
#endif
    return true;
}
} // namespace a2m
