# The pinned engine gates diagnostic state on DEBUG, but JUCE_LOG_ASSERTIONS
# also evaluates jassert in Release. Keep the state and assertion in sync.
set(_sunroom_te_patch "${CMAKE_CURRENT_LIST_DIR}/../patches/tracktion-release-assertions.patch")
set(_sunroom_te_root "${CMAKE_CURRENT_LIST_DIR}/../third_party/tracktion_engine")
execute_process(COMMAND git -C "${_sunroom_te_root}" apply --reverse --check "${_sunroom_te_patch}"
    RESULT_VARIABLE _sunroom_te_applied OUTPUT_QUIET ERROR_QUIET)
if(NOT _sunroom_te_applied EQUAL 0)
    execute_process(COMMAND git -C "${_sunroom_te_root}" apply --check "${_sunroom_te_patch}"
        RESULT_VARIABLE _sunroom_te_can_apply OUTPUT_QUIET ERROR_QUIET)
    if(NOT _sunroom_te_can_apply EQUAL 0)
        message(FATAL_ERROR "SUNROOM Tracktion assertion patch no longer matches the dependency. Review the upstream change.")
    endif()
    execute_process(COMMAND git -C "${_sunroom_te_root}" apply "${_sunroom_te_patch}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()
