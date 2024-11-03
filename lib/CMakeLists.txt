add_library(fluorine)
add_library(fluorine::fluorine ALIAS fluorine)

set_target_properties(fluorine PROPERTIES LINKER_LANGUAGE CXX)
target_set_output_dirs(fluorine)

function (target_add_sources TGT PREFIX)
    set(srcs ${ARGN})
    list(TRANSFORM srcs PREPEND ${PREFIX})
    target_sources(${TGT} PRIVATE ${srcs})
endfunction ()

target_add_sources(fluorine "include/fluorine/"
    # Header files here
    "core/board.h"
    "core/macros.h"
)

target_add_sources(fluorine "src/"
    # Source files here (relative to ./src/)
    "arena/random_player.cpp"
    "core/board.cpp"
    "core/game.cpp"
    "evaluation/endgame_solver.cpp"
    "evaluation/transposition_table.cpp"
    "utils/tui.cpp"
)

find_package(clu CONFIG REQUIRED)

target_set_options(fluorine PRIVATE)
target_set_warnings(fluorine PRIVATE)
target_set_cxx_std(fluorine)

if (FLUORINE_ENABLE_AVX2)
    if (MSVC)
        target_compile_options(fluorine PRIVATE /arch:AVX2)
    else ()
        target_compile_options(fluorine PRIVATE -mavx2)
    endif ()
endif ()

if (BUILD_SHARED_LIBS)
    target_compile_definitions(fluorine
        PRIVATE FLUORINE_EXPORT_SHARED
        PUBLIC FLUORINE_BUILD_SHARED)
endif ()

target_include_directories(fluorine
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(fluorine
PUBLIC
    clu::clu
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/fluorineConfigVersion.cmake"
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS fluorine
    EXPORT fluorineTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/fluorineConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/fluorineConfig.cmake"
    INSTALL_DESTINATION lib/cmake/fluorine
)

export(TARGETS fluorine NAMESPACE fluorine:: FILE fluorineConfig.cmake)
install(EXPORT fluorineTargets NAMESPACE fluorine:: DESTINATION lib/cmake/fluorine)
install(FILES "${PROJECT_BINARY_DIR}/fluorineConfigVersion.cmake"
                "${PROJECT_BINARY_DIR}/fluorineConfig.cmake"
        DESTINATION lib/cmake/fluorine)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/lib/include/ DESTINATION include)
