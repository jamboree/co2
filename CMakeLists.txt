cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} )
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project(co2 VERSION 1.0.0 LANGUAGES CXX)

option(CO2_ENABLE_TESTING "Enable testing of the co2 library." OFF)

# Set the default CMAKE_BUILD_TYPE to Release.
# This should be done before the project command since the latter can set
# CMAKE_BUILD_TYPE itself (it does so for nmake).
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
endif()

add_library(${PROJECT_NAME}
    INTERFACE
)

find_package(Boost REQUIRED)

# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(${PROJECT_NAME}
    INTERFACE
        include
        ${Boost_INCLUDE_DIR}
)

# If we have compiler requirements for this library, list them here
# target_compile_features(${PROJECT_NAME}
#     PUBLIC
#         cxx_alias_templates
#         cxx_auto_type
#         cxx_decltype_auto
#         cxx_user_literals
#         cxx_rvalue_references
#         cxx_range_for
#         cxx_nullptr
#         cxx_noexcept
#         cxx_inline_namespaces
# )

# Below doesn't work for INTERFACE target :(
# set_target_properties(${PROJECT_NAME} PROPERTIES
#     CXX_STANDARD 14
# )

install(DIRECTORY include/ DESTINATION include)

if (CO2_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
