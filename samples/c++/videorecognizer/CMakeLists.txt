cmake_minimum_required(VERSION 3.16)
project(videorecognizer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

# Find nlohmann/json (header-only library)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Find cxxopts (header-only library)
find_package(cxxopts REQUIRED)

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/../../../c++)

# Add executable
add_executable(videorecognizer videorecognizer.cpp)

# Link libraries
target_link_libraries(videorecognizer 
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
)

# Link ultimateALPR SDK
# Note: You may need to adjust the path to the SDK library based on your installation
if(EXISTS "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}/libultimate_alpr-sdk.so")
    target_link_libraries(videorecognizer 
        "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}/libultimate_alpr-sdk.so"
    )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/x86_64/libultimate_alpr-sdk.so")
    target_link_libraries(videorecognizer 
        "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/x86_64/libultimate_alpr-sdk.so"
    )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/aarch64/libultimate_alpr-sdk.so")
    target_link_libraries(videorecognizer 
        "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/aarch64/libultimate_alpr-sdk.so"
    )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/armv7l/libultimate_alpr-sdk.so")
    target_link_libraries(videorecognizer 
        "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/armv7l/libultimate_alpr-sdk.so"
    )
else()
    message(WARNING "Could not find ultimateALPR SDK library. Please ensure it's installed and the path is correct.")
endif()

# Set RPATH for runtime library loading
set_target_properties(videorecognizer PROPERTIES
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/../../../binaries/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Copy assets to build directory for testing
file(COPY "${CMAKE_SOURCE_DIR}/../../../assets" DESTINATION "${CMAKE_BINARY_DIR}")

# Install target
install(TARGETS videorecognizer DESTINATION bin) 