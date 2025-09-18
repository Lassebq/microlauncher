cmake_minimum_required(VERSION 3.31.0)
project(microlauncher VERSION 1.0.0 LANGUAGES C CXX)

set(MICROSOFT_CLIENT_ID "95984717-05f1-4b52-8a66-064d0e1e5b55" CACHE STRING "Azure application client ID used for microsoft authentication")
option(USE_MOJANG_MANIFEST "Uses official mojang manifest without tweaked client JSONs" OFF)

set(SOURCES
  src/microlauncher.c
  src/microlauncher_msa.c
  src/microlauncher_gui.c
  src/microlauncher_instance.c
  src/microlauncher_account.c
  src/microlauncher_version_item.c
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
    list(APPEND DEPS uuid libpci)
endif()

pkg_check_modules(PKGCONF REQUIRED ${DEPS})

set(ICON_RES 48 64 128 256 512)

foreach(res IN LISTS ICON_RES)
    list(APPEND ICON_PNGS "${CMAKE_CURRENT_SOURCE_DIR}/assets/scaled_${res}x${res}.png")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/scaled_${res}x${res}.png" DESTINATION "share/icons/hicolor/${res}x${res}/apps" RENAME "io.github.lassebq.microlauncher.png")
endforeach()

if(WIN32)
    # Just in case we're cross compiling and have native executable in build environment
    find_program(ICOTOOl "icotool")
    if(NOT ICOTOOl)
        find_program(ICOTOOl "icotool.exe")
    endif()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/icon.ico"
        COMMAND ${ICOTOOl} -c ${ICON_PNGS} -o "${CMAKE_CURRENT_BINARY_DIR}/icon.ico"
        DEPENDS ${ICON_PNGS} 
    )
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" [[IDI_ICON1 ICON "icon.ico"]])
    list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/icon.ico")
endif()

# Generate app icon from render_1024x1024.png in assets
if(APPLE)
    find_program(ICONUTIL "iconutil")

    set(ICON_SRC ${CMAKE_SOURCE_DIR}/assets/render_1024x1024.png)
    set(ICONSET_DIR ${CMAKE_BINARY_DIR}/app_icon.iconset)
    set(ICON_OUT ${CMAKE_BINARY_DIR}/app_icon.icns)
    
    add_custom_command(
        OUTPUT ${ICON_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONSET_DIR}
        COMMAND sips -z 16 16     ${ICON_SRC} --out ${ICONSET_DIR}/icon_16x16.png
        COMMAND sips -z 32 32     ${ICON_SRC} --out ${ICONSET_DIR}/icon_16x16@2x.png
        COMMAND sips -z 32 32     ${ICON_SRC} --out ${ICONSET_DIR}/icon_32x32.png
        COMMAND sips -z 64 64     ${ICON_SRC} --out ${ICONSET_DIR}/icon_32x32@2x.png
        COMMAND sips -z 128 128   ${ICON_SRC} --out ${ICONSET_DIR}/icon_128x128.png
        COMMAND sips -z 256 256   ${ICON_SRC} --out ${ICONSET_DIR}/icon_128x128@2x.png
        COMMAND sips -z 256 256   ${ICON_SRC} --out ${ICONSET_DIR}/icon_256x256.png
        COMMAND sips -z 512 512   ${ICON_SRC} --out ${ICONSET_DIR}/icon_256x256@2x.png
        COMMAND sips -z 512 512   ${ICON_SRC} --out ${ICONSET_DIR}/icon_512x512.png
        COMMAND ${CMAKE_COMMAND} -E copy ${ICON_SRC} ${ICONSET_DIR}/icon_512x512@2x.png
        COMMAND iconutil -c icns ${ICONSET_DIR} -o ${ICON_OUT}
        DEPENDS ${ICON_SRC}
        COMMENT "Generating macOS .icns icon"
    )

    add_custom_target(icons ALL DEPENDS ${ICON_OUT})
endif()

set_source_files_properties(${ICON_OUT} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

add_executable(microlauncher WIN32 MACOSX_BUNDLE ${ICON_OUT} ${SOURCES})
if(${CMAKE_C_COMPILER_ID} STREQUAL GNU)
    target_compile_options(microlauncher PUBLIC -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type)
endif()

set_target_properties(microlauncher PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in)

set_target_properties(microlauncher PROPERTIES 
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.lassebq.microlauncher"
    MACOSX_BUNDLE_BUNDLE_NAME "MicroLauncher"
    MACOSX_BUNDLE_ICON_FILE "app_icon"
)

target_compile_definitions(microlauncher PRIVATE
    MICROSOFT_CLIENT_ID=\"${MICROSOFT_CLIENT_ID}\"
    LAUNCHER_VERSION=\"${CMAKE_PROJECT_VERSION}\")
if(${USE_MOJANG_MANIFEST})
    target_compile_definitions(microlauncher PRIVATE USE_MOJANG_MANIFEST)
endif()
target_link_directories(microlauncher PRIVATE ${PKGCONF_LIBRARY_DIRS})
target_link_libraries(microlauncher PRIVATE ${PKGCONF_LIBRARIES} m)
target_include_directories(microlauncher PRIVATE ${PKGCONF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/io.github.lassebq.microlauncher.desktop" DESTINATION share/applications)

install(TARGETS microlauncher BUNDLE DESTINATION . TYPE RUNTIME)
if(WIN32)
    install(CODE "set(CMAKE_SYSTEM_LIBRARY_PATH \"${CMAKE_SYSTEM_LIBRARY_PATH}\")")
    
    install(CODE [[
        set(OBJDUMP_BIN ${CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND})
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "./objdump_unix2dos.sh")

        file(WRITE "objdump_unix2dos.sh" "${OBJDUMP_BIN} $@ | unix2dos")
        file(CHMOD "objdump_unix2dos.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

        file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR deps_resolved
        UNRESOLVED_DEPENDENCIES_VAR deps_unresolved
        LIBRARIES $<TARGET_FILE:microlauncher>
        DIRECTORIES ${CMAKE_SYSTEM_LIBRARY_PATH}
        PRE_EXCLUDE_REGEXES "api-ms-*" "ext-ms-*"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )
        # message(STATUS "Resolved deps: ${deps_resolved}")
        # message(STATUS "Unresolved deps: ${deps_unresolved}")
        foreach(dep ${deps_resolved})
        file(INSTALL ${dep} DESTINATION bin)
        endforeach()
    ]])
    
    install(DIRECTORY ${CMAKE_FIND_ROOT_PATH}/share/glib-2.0/schemas DESTINATION share/glib-2.0)

endif()
