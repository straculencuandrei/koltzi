cmake_minimum_required(VERSION 3.20)
project(Koltzi VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(UNICODE _UNICODE NOMINMAX)

# Static CRT (/MT) for standalone portable single .exe without external runtimes
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Optimization and warning flags
if (MSVC)
    add_compile_options(
        /permissive-
        /W4
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gw>
        /EHsc
    )
    add_link_options(
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

# Fetch Zydis Fast Instruction Decoder
include(FetchContent)
FetchContent_Declare(
    zydis
    GIT_REPOSITORY https://github.com/zyantific/zydis.git
    GIT_TAG        v4.1.0
    GIT_SHALLOW    TRUE
)

set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TOOLS      OFF CACHE BOOL "" FORCE)
set(ZYDIS_FEATURE_ENCODER  OFF CACHE BOOL "" FORCE)
set(ZYDIS_FEATURE_SEGMENT  ON  CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(zydis)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
set(KOLTZI_SOURCES
    src/Main.cpp
    src/Common.h
    res/resource.h
    res/Koltzi.rc
    src/Core/TriageReport.h
    src/Core/PeReader.h
    src/Core/PeReader.cpp
    src/Core/CryptoVerifier.h
    src/Core/CryptoVerifier.cpp
    src/Core/InstructionScanner.h
    src/Core/InstructionScanner.cpp
    src/Core/StringScanner.h
    src/Core/StringScanner.cpp
    src/Core/ThreatAssessor.h
    src/Core/ThreatAssessor.cpp
    src/UI/AnimationTypes.h
    src/UI/FontManager.h
    src/UI/FontManager.cpp
    src/UI/GhostRenderer.h
    src/UI/GhostRenderer.cpp
    src/UI/SpeechBubble.h
    src/UI/SpeechBubble.cpp
    src/UI/GhostWindow.h
    src/UI/GhostWindow.cpp
    src/App/Application.h
    src/App/Application.cpp
)

# Application Executable
add_executable(Koltzi WIN32 ${KOLTZI_SOURCES})

target_link_libraries(Koltzi PRIVATE
    Zydis
    d2d1
    dwrite
    windowscodecs
    shlwapi
    comctl32
    shell32
    user32
    gdi32
    ole32
    comdlg32
    bcrypt
    wintrust
    crypt32
)

target_include_directories(Koltzi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Build tests executable
add_executable(KoltziTests
    tests/TestRunner.cpp
    src/Core/PeReader.cpp
    src/Core/CryptoVerifier.cpp
    src/Core/InstructionScanner.cpp
    src/Core/StringScanner.cpp
    src/Core/ThreatAssessor.cpp
)

target_include_directories(KoltziTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(KoltziTests PRIVATE
    Zydis
    shlwapi
    user32
    bcrypt
    wintrust
    crypt32
)

set_target_properties(Koltzi PROPERTIES
    OUTPUT_NAME "Koltzi"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
