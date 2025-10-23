cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)

project(midi-markov-plugin VERSION 0.0.1)

add_subdirectory(./JUCE ./JUCE)                    

juce_add_plugin(midi-markov-plugin
    # VERSION ...                               
    # ICON_BIG ...                          
    # ICON_SMALL ...
    COMPANY_NAME Yee-King         
    BUNDLE_ID net.yeeking.midi-markov-plugin-v3   # <-- add this (no spaces)         
    IS_SYNTH FALSE                
    NEEDS_MIDI_INPUT TRUE            
    NEEDS_MIDI_OUTPUT TRUE             
    IS_MIDI_EFFECT TRUE         
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    
    COPY_PLUGIN_AFTER_BUILD TRUE        
    PLUGIN_MANUFACTURER_CODE Yeek              
    PLUGIN_CODE mmi0                            
                                             
    FORMATS AU VST3 Standalone                  
    PRODUCT_NAME "midi-markov-plugin-V3")     

# you can do this since we are not using torch any more
juce_generate_juce_header(midi-markov-plugin)


target_sources(midi-markov-plugin
    PRIVATE
    src/PluginEditor.cpp
    src/PluginProcessor.cpp
    src/ImproviserControlGUI.cpp
    src/NoteIndicatorComponent.cpp
    src/ChordDetector.cpp
    src/MarkovModelCPP/src/MarkovManager.cpp
    src/MarkovModelCPP/src/MarkovChain.cpp
    

   )


target_compile_definitions(midi-markov-plugin
    PUBLIC # 
        JUCE_ALSA=1
        JUCE_DIRECTSOUND=1
        JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1
        #JUCE_PLUGINHOST_LADSPA=1
        #JUCE_PLUGINHOST_LV2=1
        #JUCE_PLUGINHOST_VST3=1
        JUCE_USE_OGGVORBIS=1
        #JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(midi-markov-plugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


# (Optional) Also set the bundle identifier via CMake property:
set_target_properties(midi-markov-plugin PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER net.yeeking.midi-markov-plugin-v3
)