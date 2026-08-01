include_guard(DIRECTORY)

function(openstrata_default_build_type)
    get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endfunction()

function(openstrata_link_openusd)
    cmake_parse_arguments(ARG "" "TARGET;VISIBILITY" "COMPONENTS" ${ARGN})
    if(NOT ARG_VISIBILITY)
        set(ARG_VISIBILITY PRIVATE)
    endif()
    if(TARGET usd_ms)
        target_link_libraries(${ARG_TARGET} ${ARG_VISIBILITY} usd_ms)
    elseif(TARGET pxr::usd_ms)
        target_link_libraries(${ARG_TARGET} ${ARG_VISIBILITY} pxr::usd_ms)
    elseif(PXR_LIBRARIES)
        target_link_libraries(${ARG_TARGET} ${ARG_VISIBILITY} ${PXR_LIBRARIES})
    else()
        foreach(_component IN LISTS ARG_COMPONENTS)
            if(TARGET ${_component})
                target_link_libraries(${ARG_TARGET} ${ARG_VISIBILITY} ${_component})
            elseif(TARGET pxr::${_component})
                target_link_libraries(${ARG_TARGET} ${ARG_VISIBILITY} pxr::${_component})
            endif()
        endforeach()
    endif()
endfunction()

function(openstrata_configure_plugin)
    cmake_parse_arguments(ARG "" "TARGET;PLUG_INFO_INPUT;PLUG_INFO_OUTPUT" "" ${ARGN})
    set_target_properties(${ARG_TARGET} PROPERTIES
        PREFIX "lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${ARG_TARGET} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${_config} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY_${_config} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
            RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    endforeach()
    if(MSVC)
        target_compile_definitions(${ARG_TARGET} PRIVATE NOMINMAX)
        target_compile_options(${ARG_TARGET} PRIVATE /utf-8)
    endif()
    configure_file(${ARG_PLUG_INFO_INPUT} ${ARG_PLUG_INFO_OUTPUT} @ONLY)
endfunction()

function(openstrata_install_plugin_bundle)
    cmake_parse_arguments(ARG "" "TARGET;RESOURCE_DESTINATION" "MANIFESTS;RESOURCES" ${ARGN})
    include(GNUInstallDirs)
    install(TARGETS ${ARG_TARGET}
        RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES ${ARG_RESOURCES} DESTINATION "${ARG_RESOURCE_DESTINATION}")
    install(FILES ${ARG_MANIFESTS} DESTINATION ".")
endfunction()