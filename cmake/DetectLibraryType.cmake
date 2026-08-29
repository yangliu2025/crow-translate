# Detect library type of a library
# Returns one of `SHARED`, `STATIC`, or `UNKNOWN` into the variable passed as the first argument

function(detect_library_type LIBRARY_TYPE)
    cmake_parse_arguments(PARSE_ARGV 1 PARAMS "" "PATH" "")

    if(NOT DEFINED PARAMS_PATH)
        message(FATAL_ERROR "The `PATH` argument is required.")
    endif()

    if(DEFINED PARAMS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unparsed arguments for detect_library_type: " "${PARAMS_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT PARAMS_PATH)
        message(FATAL_ERROR "The `PATH` argument is empty.")
    endif()

    set(${LIBRARY_TYPE} UNKNOWN PARENT_SCOPE)
    string(LENGTH "${PARAMS_PATH}" PATH_LENGTH)

    string(LENGTH "${CMAKE_SHARED_LIBRARY_SUFFIX}" SHARED_SUFFIX_LENGTH)
    math(EXPR SHARED_INDEX "${PATH_LENGTH} - ${SHARED_SUFFIX_LENGTH}")
    string(SUBSTRING "${PARAMS_PATH}" "${SHARED_INDEX}" -1 SHARED_SUFFIX)

    string(LENGTH "${CMAKE_STATIC_LIBRARY_SUFFIX}" STATIC_SUFFIX_LENGTH)
    math(EXPR STATIC_INDEX "${PATH_LENGTH} - ${STATIC_SUFFIX_LENGTH}")
    string(SUBSTRING "${PARAMS_PATH}" "${STATIC_INDEX}" -1 STATIC_SUFFIX)

    if(SHARED_SUFFIX STREQUAL CMAKE_SHARED_LIBRARY_SUFFIX)
        set(${LIBRARY_TYPE} SHARED PARENT_SCOPE)
    elseif(STATIC_SUFFIX STREQUAL CMAKE_STATIC_LIBRARY_SUFFIX)
        set(${LIBRARY_TYPE} STATIC PARENT_SCOPE)
    endif()
endfunction()
