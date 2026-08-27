# Project version codegen (#90, v0.2.4).
#
# The SDK versions itself from git tags via poetry-dynamic-versioning, with no
# hand-edited constants anywhere. User projects get the same deal: the version
# comes from `git describe` in the project directory and lands in
# out/generated/version.hpp, so firmware can log what it is and a support
# ticket can name an exact commit.
#
# A project with no git repository, or with a repository but no tags, is not an
# error -- it reports v0.0.0-untagged and carries on.

set(STM32_VERSION_HEADER "${STM32_GENERATED_DIR}/version.hpp")

set(_stm32_version_string "v0.0.0-untagged")
set(_stm32_version_major 0)
set(_stm32_version_minor 0)
set(_stm32_version_patch 0)
set(_stm32_version_tagged 0)
set(_stm32_version_dirty 0)

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _stm32_git_describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _stm32_git_rc
    )

    if(_stm32_git_rc EQUAL 0 AND _stm32_git_describe)
        # --always falls back to a bare hash when no tag is reachable; that is
        # not a version, so keep the untagged string and only record dirtiness.
        if(_stm32_git_describe MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
            set(_stm32_version_string "${_stm32_git_describe}")
            set(_stm32_version_major ${CMAKE_MATCH_1})
            set(_stm32_version_minor ${CMAKE_MATCH_2})
            set(_stm32_version_patch ${CMAKE_MATCH_3})
            set(_stm32_version_tagged 1)
        else()
            set(_stm32_version_string "v0.0.0-untagged+${_stm32_git_describe}")
        endif()

        if(_stm32_git_describe MATCHES "-dirty$")
            set(_stm32_version_dirty 1)
        endif()
    endif()

    # Re-run configure when HEAD moves or a tag is added. packed-refs covers
    # tags that git has packed away; both are guarded because a fresh clone may
    # have only one of them.
    foreach(_ref HEAD packed-refs)
        if(EXISTS "${CMAKE_SOURCE_DIR}/.git/${_ref}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${CMAKE_SOURCE_DIR}/.git/${_ref}")
        endif()
    endforeach()
endif()

set(STM32_PROJECT_VERSION "${_stm32_version_string}")

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/version.hpp.in
    ${STM32_VERSION_HEADER}
    @ONLY
)

message(STATUS "Project version: ${STM32_PROJECT_VERSION}")
