macro(stm32_resolve_family CHIP)
    string(TOUPPER ${CHIP} _CHIP_UPPER)
    string(SUBSTRING "${_CHIP_UPPER}" 5 2 _FAMILY_PREFIX)
    string(TOLOWER "${_FAMILY_PREFIX}" _FAMILY_LOWER)
    set(_FAMILY_ID "stm32${_FAMILY_LOWER}")

    # sdk/chips.json is the index of what the SDK supports (#98). It replaced
    # sixteen identical "not yet supported" family stubs, each of which spelled
    # out the supported-family list -- so adding a family used to mean editing
    # every one of them.
    set(_CHIPS_JSON_FILE "${_STM32_SDK_DIR}/chips.json")
    if(NOT EXISTS "${_CHIPS_JSON_FILE}")
        message(FATAL_ERROR "Chip index not found: ${_CHIPS_JSON_FILE}")
    endif()
    file(READ "${_CHIPS_JSON_FILE}" _CHIPS_JSON)

    string(JSON _FAMILIES GET "${_CHIPS_JSON}" families)
    string(JSON _FAMILY_COUNT LENGTH "${_FAMILIES}")

    set(_KNOWN_FAMILIES "")
    set(_SUPPORTED_FAMILIES "")
    math(EXPR _LAST "${_FAMILY_COUNT} - 1")
    foreach(_i RANGE ${_LAST})
        string(JSON _NAME MEMBER "${_FAMILIES}" ${_i})
        string(JSON _IS_SUPPORTED GET "${_FAMILIES}" ${_NAME} supported)
        list(APPEND _KNOWN_FAMILIES "${_NAME}")
        if(_IS_SUPPORTED)
            list(APPEND _SUPPORTED_FAMILIES "${_NAME}")
        endif()
    endforeach()

    if(NOT _FAMILY_ID IN_LIST _KNOWN_FAMILIES)
        string(REPLACE ";" ", " _KNOWN_TEXT "${_KNOWN_FAMILIES}")
        message(FATAL_ERROR
            "Unknown STM32 family '${_FAMILY_ID}' (chip: ${CHIP}).\n"
            "Known families: ${_KNOWN_TEXT}")
    endif()

    if(NOT _FAMILY_ID IN_LIST _SUPPORTED_FAMILIES)
        string(REPLACE ";" ", " _SUPPORTED_TEXT "${_SUPPORTED_FAMILIES}")
        message(FATAL_ERROR
            "The ${_FAMILY_PREFIX} family is not yet supported by stm32-sdk.\n"
            "Chip requested: ${CHIP}\n"
            "Currently supported: ${_SUPPORTED_TEXT}\n"
            "Contributions welcome: https://github.com/khosta77/stm32-sdk")
    endif()

    set(_FAMILY_FILE "${CMAKE_CURRENT_LIST_DIR}/families/${_FAMILY_ID}.cmake")
    if(NOT EXISTS "${_FAMILY_FILE}")
        message(FATAL_ERROR
            "chips.json marks ${_FAMILY_ID} as supported but its family file "
            "is missing.\nExpected: ${_FAMILY_FILE}")
    endif()

    include("${_FAMILY_FILE}")
    cmake_language(CALL "${_FAMILY_ID}_get_chip_info" "${CHIP}")
endmacro()
