cmake_minimum_required(VERSION 3.28)

set(LLVM_PATH "/usr/lib/llvm-18")
set(CMAKE_CXX_COMPILER "${LLVM_PATH}/bin/clang++")
set(CMAKE_C_COMPILER "${LLVM_PATH}/bin/clang")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=${LLVM_PATH}/bin/ld.lld -stdlib=libc++ -Wl,-rpath,${LLVM_PATH}/lib/x86_64-unknown-linux-gnu/ -lc++abi")

project(cxx-async-io)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall
    -Wextra
    -pedantic
    -Werror
    -Wno-unused-function
    -Wno-unused-parameter
    -Wno-unused-private-field
    -Wno-missing-field-initializers
)

#set(CMAKE_CXX_CLANG_TIDY clang-tidy)

include(cmake/CPM.cmake)
CPMAddPackage("gh:catchorg/Catch2@3.8.0")
CPMAddPackage(
    NAME stdexec
    GITHUB_REPOSITORY NVIDIA/stdexec
    GIT_TAG main
)

add_executable(test-app
        1.1-sync.cc
        1.2-sync-rec.cc
        2.1-cps-nested-lambda.cc
        2.2-cps-no-alloc.cc
        2.3-cps-sync-res.cc
        3.1-coro.cc
        4.1-snr-custom.cc
        4.2-snr-basic.cc
        4.3-snr-any.cc
        4.4-snr.cc
)
target_link_libraries(test-app PRIVATE
    Catch2::Catch2WithMain
    STDEXEC::stdexec
)

add_custom_target(test COMMENT "Running tests..."
    COMMAND test-app
)
