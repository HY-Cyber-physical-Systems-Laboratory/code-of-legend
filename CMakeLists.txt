cmake_minimum_required(VERSION 3.20)
project(code-of-legend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG        v1.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(drogon)

FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
    GIT_SHALLOW    TRUE
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE drogon jwt-cpp::jwt-cpp argon2)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# ── Aggressive optimizations (Release) ──
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:
        -O3                    # max optimization
        -march=native          # all CPU instructions (AVX2, SSE4.2, etc.)
        -mtune=native          # tune for this exact CPU
        -funroll-loops         # loop unrolling
        -ffast-math            # aggressive float math
        -flto                  # link-time optimization
        -fomit-frame-pointer   # free up a register
        -fprefetch-loop-arrays # auto prefetch
        -ftree-vectorize       # auto SIMD vectorization
        -DNDEBUG               # disable asserts
    >
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-flto -s -static-libgcc -static-libstdc++>
)

# Static link OpenSSL, libpq, argon2, zlib, uuid, jsoncpp
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    -static-libgcc -static-libstdc++
    -Wl,-Bstatic
    ssl crypto pq argon2 jsoncpp uuid z
    -Wl,-Bdynamic
    pthread dl
)
