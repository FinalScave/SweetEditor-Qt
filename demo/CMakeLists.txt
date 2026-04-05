cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if (WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(LINK_LIB)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DEMO_NAME SweetEditorQtDemo)

file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cpp)
add_executable(${DEMO_NAME} ${SRC_FILES})

target_link_libraries(${DEMO_NAME} PRIVATE SweetEditorQt)

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
