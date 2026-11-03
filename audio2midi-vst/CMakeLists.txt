cmake_minimum_required(VERSION 3.22)
project(Audio2MIDI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static C++ runtime on Windows so the plugin itself needs no Visual C++ redistributable.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# ONNX Runtime (prebuilt). Pass -DORT_ROOT=/path to use a local copy instead of downloading.
set(ORT_VERSION 1.20.1)
if(NOT DEFINED ORT_ROOT)
  if(WIN32)
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip")
  else()
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz")
  endif()
  FetchContent_Declare(onnxruntime URL ${ORT_URL})
  FetchContent_MakeAvailable(onnxruntime)
  set(ORT_ROOT "${onnxruntime_SOURCE_DIR}")
endif()

# The Basic Pitch note model (Apache 2.0), embedded into the plugin binary.
juce_add_binary_data(Audio2MIDIModel SOURCES assets/nmp.onnx)

juce_add_plugin(Audio2MIDI
  COMPANY_NAME "Hady Qassim"
  PLUGIN_MANUFACTURER_CODE Hdqs
  PLUGIN_CODE A2m1
  PRODUCT_NAME "Audio2MIDI"
  FORMATS VST3 Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx Tools
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Audio2MIDI PRIVATE
  Source/BasicPitchEngine.cpp
  Source/AudioToMidi.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_include_directories(Audio2MIDI PRIVATE "${ORT_ROOT}/include")

target_compile_definitions(Audio2MIDI PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_USE_OGGVORBIS=1)

target_link_libraries(Audio2MIDI
  PRIVATE
    Audio2MIDIModel
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags)

if(WIN32)
  # Loaded by full path at runtime (see Source/OrtLoader.h), so no import library is linked.
  target_compile_definitions(Audio2MIDI PUBLIC ORT_API_MANUAL_INIT)
  foreach(fmt VST3 Standalone)
    add_custom_command(TARGET Audio2MIDI_${fmt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${ORT_ROOT}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:Audio2MIDI_${fmt}>")
  endforeach()
else()
  target_link_libraries(Audio2MIDI PRIVATE "${ORT_ROOT}/lib/libonnxruntime.so")
  set_target_properties(Audio2MIDI PROPERTIES BUILD_RPATH "${ORT_ROOT}/lib")
endif()
