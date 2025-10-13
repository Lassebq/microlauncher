cmake_minimum_required(VERSION 3.31.0)
project(MicroLauncher VERSION 1.0.0 LANGUAGES C CXX)

set(MICROSOFT_CLIENT_ID "95984717-05f1-4b52-8a66-064d0e1e5b55" CACHE STRING "Azure application client ID used for microsoft authentication")
set(MANIFEST_URL "" CACHE STRING "Version manifest URL")

set(SOURCES
  src/microlauncher.c
  src/microlauncher_msa.c
  src/microlauncher_gui.c
  src/microlauncher_instance.c
  src/microlauncher_account.c
  src/microlauncher_version_item.c
  src/microlauncher_java_runtime.c
  src/xdgutil.c
  src/util.c
  src/json_util.c
  src/gtk_util.c
  src/gobject_util.c
)
if(WIN32)
    list(APPEND SOURCES src/util.cpp)
endif()

find_package(PkgConfig REQUIRED)
set(DEPS
    gtk4 json-c gio-2.0 glib-2.0 gobject-2.0 libcurl libcrypto libzip
)
if(UNIX AND NOT APPLE)
    list(APPEND DEPS uuid)
endif()

pkg_check_modules(PKGCONF REQUIRED ${DEPS})
pkg_check_modules(LIBPCI libpci)

set(ICON_RES 48 64 128 256 512)
set(ICON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/assets/render_1024x1024.png)
set(ICONS_DIR ${CMAKE_CURRENT_BINARY_DIR}/icon)
if(APPLE)
    set(ICON_OUT ${CMAKE_CURRENT_BINARY_DIR}/icon.icns)
    set(ICONS_DIR ${CMAKE_CURRENT_BINARY_DIR}/icon.iconset)
elseif(WIN32)
    set(ICON_OUT ${CMAKE_CURRENT_BINARY_DIR}/icon.ico)
endif()

find_program(MAGICK NAMES "magick" "convert" "magick.exe" "convert.exe")
find_program(ICOTOOl NAMES "icotool" "icotool.exe")
if(APPLE)
    if(NOT MAGICK) # Fall back to Apple's scriptable image processing system
        find_program(SIPS sips)
    endif()
    find_program(ICONUTIL "iconutil")
endif()

file(MAKE_DIRECTORY ${ICONS_DIR})
foreach(res IN LISTS ICON_RES)
    list(APPEND ICON_PNGS "${ICONS_DIR}/icon_${res}x${res}.png")
    if(MAGICK)
        add_custom_command(
            OUTPUT "${ICONS_DIR}/icon_${res}x${res}.png"
            COMMAND ${MAGICK} ${ICON_SRC} -filter Catrom -resize "${res}x${res}" "${ICONS_DIR}/icon_${res}x${res}.png"
            DEPENDS ${ICON_SRC}
        )
    elseif(SIPS)
        add_custom_command(
            OUTPUT "${ICONS_DIR}/icon_${res}x${res}.png"
            COMMAND ${SIPS} -z ${res} ${res} ${ICON_SRC} --out "${ICONS_DIR}/icon_${res}x${res}.png"
            DEPENDS ${ICON_SRC}
        )
    endif()
    list(APPEND PNG_ICONS "${ICONS_DIR}/icon_${res}x${res}.png")

    install(FILES "${ICONS_DIR}/icon_${res}x${res}.png" DESTINATION "share/icons/hicolor/${res}x${res}/apps" RENAME "io.github.lassebq.microlauncher.png")
endforeach()
add_custom_target(icons ALL DEPENDS ${PNG_ICONS})

if(WIN32)
    add_custom_command(
        OUTPUT ${ICON_OUT}
        COMMAND ${ICOTOOl} -c ${ICON_PNGS} -o ${ICON_OUT}
        DEPENDS ${ICON_PNGS} 
    )
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" [[IDI_ICON1 ICON "icon.ico"]])
    list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" APPEND PROPERTY OBJECT_DEPENDS ${ICON_OUT})
elseif(APPLE)
    add_custom_command(
        OUTPUT ${ICON_OUT}
        COMMAND ${ICONUTIL} -c icns ${ICONS_DIR} -o ${ICON_OUT}
        DEPENDS ${ICON_PNGS} 
    )
    list(APPEND SOURCES "${ICON_OUT}")
endif()

# add_custom_target(icons ALL DEPENDS ${ICON_OUT})
if(APPLE)
    set_source_files_properties(${ICON_OUT} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

add_executable(microlauncher WIN32 MACOSX_BUNDLE ${SOURCES})
if(NOT LIBPCI_FOUND)
    target_compile_definitions(microlauncher PRIVATE DISABLE_GPU=1)
else()
    target_link_directories(microlauncher PRIVATE ${LIBPCI_LIBRARY_DIRS})
    target_link_libraries(microlauncher PRIVATE ${LIBPCI_LIBRARIES} m)
    target_include_directories(microlauncher PRIVATE ${LIBPCI_INCLUDE_DIRS})
endif()

if(APPLE)
    set_target_properties(microlauncher PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.lassebq.microlauncher"
        MACOSX_BUNDLE_BUNDLE_NAME "MicroLauncher"
        MACOSX_BUNDLE_ICON_FILE "icon"
    )
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL GNU)
    target_compile_options(microlauncher PUBLIC -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type)
endif()

target_compile_definitions(microlauncher PRIVATE
    MICROSOFT_CLIENT_ID=\"${MICROSOFT_CLIENT_ID}\"
    LAUNCHER_VERSION=\"${CMAKE_PROJECT_VERSION}\")
if(NOT MANIFEST_URL STREQUAL "")
    target_compile_definitions(microlauncher PRIVATE MANIFEST_URL=\"${MANIFEST_URL}\")
endif()
target_link_directories(microlauncher PRIVATE ${PKGCONF_LIBRARY_DIRS})
target_link_libraries(microlauncher PRIVATE ${PKGCONF_LIBRARIES} m)
target_include_directories(microlauncher PRIVATE ${PKGCONF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/io.github.lassebq.microlauncher.desktop" DESTINATION share/applications)

install(TARGETS microlauncher BUNDLE DESTINATION . TYPE RUNTIME)
if(WIN32)

    message(STATUS "${CMAKE_INSTALL_PREFIX}")
    foreach(path IN LISTS CMAKE_FIND_ROOT_PATH)
        if(EXISTS ${path}/share/glib-2.0/schemas)
            install(DIRECTORY ${path}/share/glib-2.0/schemas DESTINATION share/glib-2.0)
            break()
        endif()
    endforeach()
    install(CODE "set(CMAKE_SYSTEM_LIBRARY_PATH \"${CMAKE_SYSTEM_LIBRARY_PATH}\")")

    install(CODE [[
        message(STATUS ${CMAKE_SYSTEM_LIBRARY_PATH})
        file(GET_RUNTIME_DEPENDENCIES
            RESOLVED_DEPENDENCIES_VAR deps_resolved
            UNRESOLVED_DEPENDENCIES_VAR deps_unresolved
            LIBRARIES "$<TARGET_FILE:microlauncher>"
            DIRECTORIES ${CMAKE_SYSTEM_LIBRARY_PATH}
            PRE_EXCLUDE_REGEXES "api-ms-*" "ext-ms-*" "^opengl32\\.dll$"
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )
        message(STATUS "Resolved deps: ${deps_resolved}")
        message(STATUS "Unresolved deps: ${deps_unresolved}")
        foreach(dep ${deps_resolved})
            file(INSTALL ${dep} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
        endforeach()
    ]])
    set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
    find_library(dep NAMES Lerc NO_CACHE REQUIRED)
    install(PROGRAMS ${dep} DESTINATION bin)

endif()
