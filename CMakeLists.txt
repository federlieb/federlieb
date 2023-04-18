cmake_minimum_required(VERSION 3.9)

# set(CMAKE_TOOLCHAIN_FILE ${CMAKE_BINARY_DIR}/conan_toolchain.cmake)

project(federlieb
  DESCRIPTION  "C++23 bindings for SQLite"
  HOMEPAGE_URL "https://github.com/federlieb/federlieb/"
  LANGUAGES    "CXX"
)

find_package(fmt REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED)

include(CheckIPOSupported)
check_ipo_supported()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
  ${PROJECT_SOURCE_DIR}/src
)

include_directories(
  ${SQLite3_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${fmt_INCLUDE_DIRS}
)

if (CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    # -Wconversion
  )
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES RELEASE)
  if (CMAKE_COMPILER_IS_GNUCXX)

  add_compile_options(
      -Wno-unused-parameter
      -Wno-unused-variable
      -Wno-unused-function
      -Wno-unused-but-set-variable
      -Wno-unused-but-set-parameter
    )

  endif()
endif()

if (CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(
    -fconcepts-diagnostics-depth=4
    -fdiagnostics-color=always
  )
endif()

if (CMAKE_COMPILER_IS_GNUCXX)

  add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)

  add_compile_options(
    -fconcepts-diagnostics-depth=4
    -fdiagnostics-color=always
  )
endif()


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(
    -stdlib=libc++
  )
endif()

set(FEDERLIEB_SOURCES_LIST 
  src/federlieb/column.cxx
  src/federlieb/db.cxx
  src/federlieb/error.cxx
  src/federlieb/field.cxx
  src/federlieb/pragma.cxx
  src/federlieb/row.cxx
  src/federlieb/stmt.cxx
  src/federlieb/detail.cxx
  src/federlieb/value.cxx
  src/federlieb/as.cxx
  src/federlieb/vtab.cxx
  src/federlieb/context.cxx
  src/federlieb/json.cxx
)

add_library(
  federlieb_static STATIC
  ${FEDERLIEB_SOURCES_LIST}
)

add_library(
  federlieb_static_ext STATIC
  ${FEDERLIEB_SOURCES_LIST}
)

set_property(
  TARGET federlieb_static_ext
  PROPERTY POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(federlieb_static_ext
  PUBLIC FEDERLIEB_EXTENSION_API
)

set(FEDERLIEB_EXT_SOURCES_LIST 
  ext/vt_stmt.cxx
  ext/vt_dominator_tree.cxx
  ext/vt_strong_components.cxx
  ext/vt_weak_components.cxx
  ext/vt_transitive_closure.cxx
  ext/vt_partition_by.cxx
  ext/vt_nameless.cxx
  ext/vt_contraction.cxx
  ext/vt_dijkstra_shortest_paths.cxx
  ext/vt_json_each.cxx
  ext/vt_script.cxx
  ext/vt_dfa.cxx
  ext/vt_articulation_points.cxx
  ext/vt_biconnected_components.cxx
  ext/vt_miniucd.cxx
  ext/vt_colormap.cxx
  ext/vt_bfs.cxx
  ext/vt_stmt_source.cxx
  ext/vt_stat1.cxx

  ext/fx_toset.cxx
  ext/fx_kcrypto.cxx
  ext/fx_counter.cxx
  ext/fx_ordered_concat_agg.cxx
  ext/fx_utility.cxx
  ext/fx_statistics.cxx
  ext/fx_only_value.cxx
  ext/fx_bits.cxx
)

add_executable(main main.cxx
  ${FEDERLIEB_EXT_SOURCES_LIST}
)

target_link_libraries (main
  ${SQLite3_LIBRARIES}
  ${fmt_LIBRARIES}
  ${Boost_LIBRARIES}
  federlieb_static
)

if(CMAKE_BUILD_TYPE MATCHES RELEASE)

  if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
  endif()

  set_property(
    TARGET main
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE
  )

endif()

add_executable(mini mini.cxx)

target_link_libraries (mini
  ${SQLite3_LIBRARIES}
  ${fmt_LIBRARIES}
  ${Boost_LIBRARIES}
)

add_library(fl_extensions SHARED
  extensions.cxx
  ${FEDERLIEB_EXT_SOURCES_LIST}
)

set_target_properties(fl_extensions
  PROPERTIES
  PREFIX ""
  POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(fl_extensions
  PUBLIC FEDERLIEB_EXTENSION_API
)

target_link_libraries (fl_extensions
  ${Boost_LIBRARIES}
  federlieb_static_ext
)

target_link_options(fl_extensions PRIVATE -static-libgcc -static-libstdc++)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")

target_precompile_headers(federlieb_static PRIVATE src/federlieb/federlieb.hxx)
target_precompile_headers(federlieb_static_ext PRIVATE src/federlieb/federlieb.hxx)
target_precompile_headers(
  fl_extensions
  PRIVATE src/federlieb/federlieb.hxx
  # PRIVATE ext/vt_bgl.hxx
)
