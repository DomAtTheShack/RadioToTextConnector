cmake_minimum_required(VERSION 4.1)
# Radio to Text Connector. HAMPROOF_* options and the hamproof_* targets keep the
# project's old internal name
project(RadioToTextConnector)

set(CMAKE_CXX_STANDARD 20)
# No C++20 modules here. CMake scans for them anyway with Ninja, and GCC 16's scanner
# fails on some of our files, which broke the Windows CI build
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# -DHAMPROOF_STATIC=ON links the C++ runtime (and Hamlib on Linux) into the exe.
# Windows (MinGW) builds always link the runtime statically
option(HAMPROOF_STATIC "Link libraries statically where possible" OFF)

option(HAMPROOF_TESTS "Build the test program and register it with ctest" ON)

# Everything except the menus and the radio, shared by the program and the tests
add_library(hamproof_core STATIC
        backend.cpp backend.h
        settings.cpp settings.h
        modem.cpp modem.h
        packet.cpp packet.h
        fsk.cpp fsk.h
        testcases.cpp testcases.h)
target_include_directories(hamproof_core PUBLIC ${CMAKE_SOURCE_DIR})
# Lets the Test tools menu find tests/audio when run from the build folder
target_compile_definitions(hamproof_core PRIVATE HAMPROOF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Sessions read the keyboard and minimodem's output on their own threads
find_package(Threads REQUIRED)
target_link_libraries(hamproof_core PUBLIC Threads::Threads)

# backend.cpp picks the OS code; each OS needs its own audio library
if(WIN32)
    # ws2_32: sockets, for direwolf's KISS port
    target_link_libraries(hamproof_core PUBLIC winmm ws2_32)
elseif(APPLE)
    # Homebrew's minimodem uses PulseAudio, so the device list comes from there too
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPULSE REQUIRED IMPORTED_TARGET libpulse)
    target_link_libraries(hamproof_core PUBLIC PkgConfig::LIBPULSE)
else()
    find_package(ALSA REQUIRED)
    target_link_libraries(hamproof_core PUBLIC ALSA::ALSA)
endif()

add_executable(RadioToTextConnector main.cpp ansi.hpp options.h session.cpp session.h cat.cpp cat.h)
target_link_libraries(RadioToTextConnector PRIVATE hamproof_core)

# The test audio goes next to the program, where the Test tools menu looks first
add_custom_command(TARGET RadioToTextConnector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/audio
                $<TARGET_FILE_DIR:RadioToTextConnector>/test-audio)

# Small CAT check against a radio, the X6100 simulator or the dummy rig
add_executable(cat_test cat_test.cpp cat.cpp cat.h)

set(HAMPROOF_EXECUTABLES RadioToTextConnector cat_test)
if(HAMPROOF_TESTS)
    enable_testing()
    add_executable(hamproof_tests tests/hamproof_tests.cpp)
    target_link_libraries(hamproof_tests PRIVATE hamproof_core)
    list(APPEND HAMPROOF_EXECUTABLES hamproof_tests)
    add_test(NAME unit COMMAND hamproof_tests unit)
    add_test(NAME audio COMMAND hamproof_tests audio ${CMAKE_SOURCE_DIR}/tests/audio)
    set_tests_properties(audio PROPERTIES SKIP_RETURN_CODE 77)   # minimodem not installed
endif()

# Hamlib: the bundled Windows build, the one installed with `make install` on Linux,
# or Homebrew's on macOS
if(WIN32)
    set(HAMLIB_DIR ${CMAKE_SOURCE_DIR}/hamlib-w64-4.7.2)
    add_library(hamlib SHARED IMPORTED)
    set_target_properties(hamlib PROPERTIES
            IMPORTED_IMPLIB ${HAMLIB_DIR}/lib/gcc/libhamlib.dll.a
            IMPORTED_LOCATION ${HAMLIB_DIR}/bin/libhamlib-4.dll
            INTERFACE_INCLUDE_DIRECTORIES ${HAMLIB_DIR}/include)
    # The exe needs Hamlib's DLLs next to it to start
    file(GLOB HAMLIB_DLLS ${HAMLIB_DIR}/bin/*.dll)
    foreach(target RadioToTextConnector cat_test)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HAMLIB_DLLS} $<TARGET_FILE_DIR:${target}>)
    endforeach()
elseif(HAMPROOF_STATIC)
    # `make install` of Hamlib (and Homebrew's) puts libhamlib.a next to the shared library
    find_library(HAMLIB_STATIC_LIBRARY libhamlib.a REQUIRED)
    find_path(HAMLIB_INCLUDE_DIR hamlib/rig.h REQUIRED)
    add_library(hamlib STATIC IMPORTED)
    set_target_properties(hamlib PROPERTIES
            IMPORTED_LOCATION ${HAMLIB_STATIC_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${HAMLIB_INCLUDE_DIR})
    # A static Hamlib also needs the libraries it uses (libusb, libudev, ...)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HAMLIB_PC REQUIRED hamlib)
    list(REMOVE_ITEM HAMLIB_PC_STATIC_LIBRARIES hamlib)
    set_target_properties(hamlib PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${HAMLIB_PC_STATIC_LIBRARY_DIRS}"
            INTERFACE_LINK_LIBRARIES "${HAMLIB_PC_STATIC_LIBRARIES}")
else()
    find_library(HAMLIB_LIBRARY hamlib REQUIRED)
    find_path(HAMLIB_INCLUDE_DIR hamlib/rig.h REQUIRED)
    add_library(hamlib UNKNOWN IMPORTED)
    set_target_properties(hamlib PROPERTIES
            IMPORTED_LOCATION ${HAMLIB_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${HAMLIB_INCLUDE_DIR})
endif()

# direwolf for Windows. The official Windows download has no Hamlib, so it couldn't key the
# radio over CAT; this builds direwolf (plus atest and gen_packets, for audio files) from
# source against the bundled Hamlib and puts them next to the program, which looks there
# first. Linux and macOS use the installed direwolf, which has Hamlib already.
if(WIN32)
    option(HAMPROOF_DIREWOLF "Build direwolf with Hamlib and put it next to the program" ON)
    set(DIREWOLF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/direwolf CACHE PATH "direwolf source to build")
endif()
if(WIN32 AND HAMPROOF_DIREWOLF)
    if(NOT EXISTS ${DIREWOLF_SOURCE_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "No direwolf source in ${DIREWOLF_SOURCE_DIR}. Clone it with\n"
                "  git clone --branch 1.8.1 https://github.com/wb2osz/direwolf.git\n"
                "or set -DHAMPROOF_DIREWOLF=OFF to build without it.")
    endif()
    include(ExternalProject)
    set(DIREWOLF_BUILD_DIR ${CMAKE_BINARY_DIR}/direwolf-build)
    set(DIREWOLF_PROGRAMS direwolf.exe atest.exe gen_packets.exe)
    list(TRANSFORM DIREWOLF_PROGRAMS PREPEND ${DIREWOLF_BUILD_DIR}/src/ OUTPUT_VARIABLE DIREWOLF_OUTPUTS)
    ExternalProject_Add(direwolf_windows
            SOURCE_DIR ${DIREWOLF_SOURCE_DIR}
            BINARY_DIR ${DIREWOLF_BUILD_DIR}
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                -DHAMLIB_INCLUDE_DIR=${HAMLIB_DIR}/include
                -DHAMLIB_LIBRARY=${HAMLIB_DIR}/lib/gcc/libhamlib.dll.a
                # The MinGW runtime goes inside the exes, so they only need Hamlib's DLLs
                -DCMAKE_EXE_LINKER_FLAGS=-static
            BUILD_COMMAND ${CMAKE_COMMAND} --build ${DIREWOLF_BUILD_DIR} --target direwolf atest gen_packets
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${DIREWOLF_OUTPUTS})
    add_dependencies(RadioToTextConnector direwolf_windows)
    add_custom_command(TARGET RadioToTextConnector POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DIREWOLF_OUTPUTS}
                    $<TARGET_FILE_DIR:RadioToTextConnector>)
    # The audio test decodes the packet files with this atest
    if(HAMPROOF_TESTS)
        add_dependencies(hamproof_tests direwolf_windows)
        add_custom_command(TARGET hamproof_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DIREWOLF_OUTPUTS} ${HAMLIB_DLLS}
                        $<TARGET_FILE_DIR:hamproof_tests>)
    endif()
endif()

target_link_libraries(RadioToTextConnector PRIVATE hamlib)
target_link_libraries(cat_test PRIVATE hamlib)

if(MINGW)
    # Always link the MinGW runtime into the exe, so an older libwinpthread on PATH
    # (e.g. WSJT-X's) can't be loaded instead. Hamlib's DLLs are still copied next to it
    foreach(target ${HAMPROOF_EXECUTABLES})
        target_link_options(${target} PRIVATE -static)
    endforeach()
elseif(HAMPROOF_STATIC)
    foreach(target ${HAMPROOF_EXECUTABLES})
        if(APPLE)
            # libc++ is part of macOS and can't be linked statically; only Hamlib is
        else()
            # glibc and ALSA stay shared: a fully static glibc binary breaks
            # ALSA's plugin loading and name lookups
            target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
        endif()
    endforeach()
endif()
