add_library(SSRDecompile)

target_include_directories(SSRDecompile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB sources CONFIGURE_DEPENDS "*.cpp" "*.h")
target_sources(SSRDecompile PRIVATE ${sources} ${headers})
target_compile_definitions(SSRDecompile PRIVATE ALL_DEM="${PROJECT_SOURCE_DIR}/App/all.dem")

target_link_libraries(SSRDecompile memory)
