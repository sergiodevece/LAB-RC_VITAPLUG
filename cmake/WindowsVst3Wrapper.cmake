# Keep the pinned JUCE tree intact. Only the Windows VST3 target compiles this copy.
function(labrc_replace_vst3_wrapper_once result source needle replacement)
    string(FIND "${source}" "${needle}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "Could not find the pinned JUCE VST3 wrapper code for the Windows overlay")
    endif()
    string(LENGTH "${needle}" count)
    math(EXPR end "${first} + ${count}")
    string(SUBSTRING "${source}" "${end}" -1 rest)
    string(FIND "${rest}" "${needle}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "Ambiguous pinned JUCE VST3 wrapper code for the Windows overlay")
    endif()
    string(REPLACE "${needle}" "${replacement}" modified "${source}")
    set(${result} "${modified}" PARENT_SCOPE)
endfunction()

function(labrc_configure_windows_vst3_wrapper)
    if(NOT WIN32)
        return()
    endif()

    set(wrapper_target juce_audio_plugin_client_VST3)
    set(wrapper_name juce_audio_plugin_client_VST3.cpp)
    set(original "${JUCE_ROOT}/modules/juce_audio_plugin_client/${wrapper_name}")
    file(SHA256 "${original}" actual_hash)
    if(NOT actual_hash STREQUAL "13780a72d21045e3f6fa0fce8fa5466ad2bacda9b41095b71141a3fb5dc4a054")
        message(FATAL_ERROR "Windows VST3 overlay requires the pinned JUCE 8.0.17 wrapper")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")

    get_target_property(sources "${wrapper_target}" INTERFACE_SOURCES)
    list(FIND sources "${original}" original_index)
    if(original_index EQUAL -1)
        message(FATAL_ERROR "Could not locate the pinned VST3 translation unit in the JUCE target")
    endif()

    file(READ "${original}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    # Report a rejected setup through the VST3 API before RoomMixer::prepare can throw.
    labrc_replace_vst3_wrapper_once(content "${content}" [=[
        if (canProcessSampleSize (newSetup.symbolicSampleSize) != kResultTrue)
            return kResultFalse;]=] [=[
        if (canProcessSampleSize (newSetup.symbolicSampleSize) != kResultTrue
            || ! std::isfinite (newSetup.sampleRate)
            || newSetup.sampleRate < 1.0 || newSetup.sampleRate > 768000.0)
            return kResultFalse;]=])
    # Label the existing sole preset when its processor name is empty.
    labrc_replace_vst3_wrapper_once(content "${content}" [=[
            toString128 (name, audioProcessor->getProgramName ((int) programIndex));]=] [=[
            const auto programName = audioProcessor->getProgramName ((int) programIndex);
            toString128 (name, programIndex == 0 && programName.isEmpty() ? juce::String ("Default") : programName);]=])

    set(generated "${CMAKE_CURRENT_BINARY_DIR}/labrc-windows-vst3/${wrapper_name}")
    file(GENERATE OUTPUT "${generated}" CONTENT "${content}")
    list(REMOVE_AT sources "${original_index}")
    list(INSERT sources "${original_index}" "${generated}")
    set_property(TARGET "${wrapper_target}" PROPERTY INTERFACE_SOURCES "${sources}")
endfunction()
