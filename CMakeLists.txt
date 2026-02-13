cmake_minimum_required(VERSION 3.22)

project(TodoListNative VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(PLUGIN_FORMATS AU VST3 Standalone)
elseif(WIN32)
  set(PLUGIN_FORMATS VST3 Standalone)
else()
  set(PLUGIN_FORMATS VST3 Standalone)
endif()

include(FetchContent)
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(TodoListNative
  COMPANY_NAME "shifshuf"
  BUNDLE_ID "com.shifshuf.todolistnative"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE SHIF
  PLUGIN_CODE TDL1
  FORMATS ${PLUGIN_FORMATS}
  PRODUCT_NAME "todo list"
)

target_sources(TodoListNative
  PRIVATE
    src/PluginProcessor.h
    src/PluginProcessor.cpp
    src/PluginEditor.h
    src/PluginEditor.cpp
)

target_compile_definitions(TodoListNative
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(TodoListNative
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(TodoListNative)
