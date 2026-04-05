cmake_minimum_required(VERSION 3.21)
project(SweetEditorQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

set(LINK_LIB)
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(MODULE_NAME ${CMAKE_PROJECT_NAME})
set(DEMO_NAME ${CMAKE_PROJECT_NAME}Demo)

file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cpp)
if (SRC_FILES)
    add_library(${MODULE_NAME} ${SRC_FILES})
else ()
    message(STATUS "No sources found under ${SRC_DIR}, configuring ${MODULE_NAME} as an interface target")
    add_library(${MODULE_NAME} INTERFACE)
endif ()

# add library function
function(add_platform_library dir filename type)
    if (${type} STREQUAL "STATIC")
        add_library(${dir} STATIC IMPORTED)
    elseif (${type} STREQUAL "SHARED")
        add_library(${dir} SHARED IMPORTED)
    else ()
        message(FATAL_ERROR "Error: add_platform_library type must be STATIC/SHARED")
    endif ()

    set(LIB_ROOT ${LIB_DIR}/${dir})
    if (NOT EXISTS "${LIB_ROOT}")
        string(REGEX REPLACE "_static$" "" LIB_ROOT_NAME "${dir}")
        string(REGEX REPLACE "_shared$" "" LIB_ROOT_NAME "${LIB_ROOT_NAME}")
        if (EXISTS "${LIB_DIR}/${LIB_ROOT_NAME}")
            set(LIB_ROOT ${LIB_DIR}/${LIB_ROOT_NAME})
        endif ()
    endif ()

    set(LIB_PATH)
    if (WIN32)
        if (MSVC)
            if(CMAKE_BUILD_TYPE STREQUAL "Release")
                set(LIB_PATH ${LIB_ROOT}/msvc-x64/release/${filename})
            else ()
                set(LIB_PATH ${LIB_ROOT}/msvc-x64/debug/${filename})
            endif ()
        else ()
            set(LIB_PATH ${LIB_ROOT}/mingw64/${filename})
        endif ()
    elseif (APPLE)
        set(_OSX_ARCH ${CMAKE_OSX_ARCHITECTURES})
        if (NOT _OSX_ARCH OR _OSX_ARCH STREQUAL "")
            # CMAKE_OSX_ARCHITECTURES fallback to empty if host arch matches target, so detect host arch here
            execute_process(COMMAND uname -m OUTPUT_VARIABLE _OSX_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif()
        set(LIB_PATH ${LIB_ROOT}/osx-${_OSX_ARCH}/${filename})
    elseif (UNIX AND NOT APPLE)
        set(LIB_PATH ${LIB_ROOT}/linux-${CMAKE_SYSTEM_PROCESSOR}/${filename})
    endif ()
    message(STATUS "add platform library: ${LIB_PATH}")
    set_target_properties(${dir} PROPERTIES IMPORTED_LOCATION ${LIB_PATH})

    set(INCLUDE_PATH ${LIB_ROOT}/include)
    if (EXISTS "${INCLUDE_PATH}" AND IS_DIRECTORY "${INCLUDE_PATH}")
        message(STATUS "add include directory: ${INCLUDE_PATH}")
        list(APPEND INCLUDE_DIRS "${INCLUDE_PATH}")
        list(REMOVE_DUPLICATES INCLUDE_DIRS)
        set(INCLUDE_DIRS ${INCLUDE_DIRS} PARENT_SCOPE)
    endif ()

    set(LINK_LIB ${LINK_LIB} ${dir} PARENT_SCOPE)
endfunction()

if (WIN32)
    if (MSVC)
        add_platform_library(sweeteditor_static sweeteditor_static.lib STATIC)
    else ()
        add_platform_library(sweeteditor_static libsweeteditor_static.a STATIC)
    endif ()
    set(LINK_LIB ${LINK_LIB} DbgHelp)
elseif (APPLE)
    add_platform_library(sweeteditor_static libsweeteditor_static.a STATIC)
elseif (UNIX AND NOT APPLE)
    add_platform_library(sweeteditor_static libsweeteditor_static.a STATIC)
endif ()

list(REMOVE_DUPLICATES INCLUDE_DIRS)
if (SRC_FILES)
    target_include_directories(${MODULE_NAME} PUBLIC ${INCLUDE_DIRS})

    target_link_libraries(${MODULE_NAME}
        PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets
        PRIVATE ${LINK_LIB}
    )
else ()
    target_include_directories(${MODULE_NAME} INTERFACE ${INCLUDE_DIRS})

    target_link_libraries(${MODULE_NAME}
        INTERFACE Qt6::Core Qt6::Gui Qt6::Widgets ${LINK_LIB}
    )
endif ()

add_executable(${DEMO_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/demo/main.cpp)

target_link_libraries(${DEMO_NAME} PRIVATE ${MODULE_NAME})

if (WIN32 AND TARGET Qt6::qmake)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                $<$<CONFIG:Debug>:--debug>
                $<$<NOT:$<CONFIG:Debug>>:--release>
                --dir "$<TARGET_FILE_DIR:${DEMO_NAME}>"
                --no-translations
                --verbose 0
                "$<TARGET_FILE:${DEMO_NAME}>"
            COMMENT "Deploying Qt runtime dependencies for ${DEMO_NAME}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
    else ()
        message(WARNING "windeployqt was not found. ${DEMO_NAME} may fail to start unless Qt bin is on PATH.")
    endif ()
endif ()
