cmake_minimum_required(VERSION 3.16)
project(LifeSimulation LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
endif()

# Enable OpenMP with specific handling for each platform
if(PLATFORM_MACOS)
    # OpenMP requires special handling on macOS
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-Xpreprocessor -fopenmp" OPENMP_FOUND)
    
    if(OPENMP_FOUND)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xpreprocessor -fopenmp")
        # Find Homebrew-installed libomp
        find_library(OpenMP_LIBRARY NAMES omp PATHS /opt/homebrew/lib /usr/local/lib)
        if(OpenMP_LIBRARY)
            message(STATUS "Found OpenMP library: ${OpenMP_LIBRARY}")
            set(OPENMP_LIBRARIES ${OpenMP_LIBRARY})
        else()
            find_library(OpenMP_LIBRARY omp 
            PATHS 
                /opt/homebrew/opt/libomp/lib
                /usr/local/opt/libomp/lib
                /opt/homebrew/Cellar/libomp/*/lib
                /usr/local/Cellar/libomp/*/lib
            NO_DEFAULT_PATH
            )
            if(OpenMP_LIBRARY)
                message(STATUS "Found OpenMP library: ${OpenMP_LIBRARY}")
                set(OPENMP_LIBRARIES ${OpenMP_LIBRARY})
            else()
                message(WARNING "OpenMP library not found. Install it with: brew install libomp")
            endif()
        endif()
    else()
        message(WARNING "OpenMP flags not supported by compiler")
    endif()
else()
    find_package(OpenMP)
    if(OpenMP_FOUND OR OpenMP_CXX_FOUND)
        set(OPENMP_LIBRARIES OpenMP::OpenMP_CXX)
        message(STATUS "Found OpenMP")
    else()
        message(WARNING "OpenMP not found. Parallel processing will be disabled.")
        add_compile_definitions(NO_OPENMP=1)
    endif()
endif()

# Set optimization flags based on platform
if(PLATFORM_MACOS)
    # Check for Apple Silicon (M-series)
    execute_process(
        COMMAND sysctl -n machdep.cpu.brand_string
        OUTPUT_VARIABLE CPU_BRAND
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(CPU_BRAND MATCHES "Apple M")
        # Apple Silicon optimizations
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mcpu=apple-m1 -mtune=apple-m1")
        add_compile_definitions(USE_APPLE_SILICON=1)
    else()
        # Intel Mac optimizations
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
    endif()
elseif(PLATFORM_WINDOWS)
    # Windows-specific optimizations
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /fp:fast /GL")
        # Favor speed over size
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ot")
    else()
        # MinGW or other GCC-like compiler on Windows
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
    endif()
else()
    # Linux/Unix optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
endif()

# Common optimizations across platforms (when supported)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -funroll-loops")
endif()

# Find required dependencies
if(PLATFORM_WINDOWS)
    # Windows-specific dependency handling
    set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL3/cmake" CACHE PATH "Path to SDL3 CMake config")
endif()

# Try to find SDL3
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found via find_package, checking for SDL3-static target")
    # Check if SDL is included as a submodule/subdirectory
    if(TARGET SDL3::SDL3-static OR TARGET SDL3::SDL3)
        message(STATUS "Using SDL3 target from submodule/subdirectory")
        set(SDL3_FOUND TRUE)
        set(SDL3_LIBRARIES SDL3::SDL3)
    else()
        message(FATAL_ERROR "SDL3 not found. Please install SDL3 or provide its location.")
    endif()
else()
    set(SDL3_LIBRARIES SDL3::SDL3)
endif()

# Glad library setup
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# ImGui sources
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
)

# ImGui backend sources for SDL3 + OpenGL3
set(IMGUI_BACKENDS_SOURCES
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# Resource handling
set(RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/shaders")
set(RESOURCE_DEST "shaders")

# Configure executable with platform-specific settings
if(PLATFORM_WINDOWS)
    add_executable(${PROJECT_NAME} WIN32
        src/main.cpp
        src/core/window.cpp
        src/core/fps_counter.cpp
        src/GUI/gui.cpp
        src/Graphics/shader.cpp
        src/Graphics/renderer.cpp
        src/Graphics/Particle.cpp
        src/Graphics/Simulation.cpp
        src/Graphics/ParticleSystem.cpp
        src/Common.cpp
        ${IMGUI_SOURCES}
        ${IMGUI_BACKENDS_SOURCES}
    )
    
    # Windows resource handling
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${RESOURCE_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${RESOURCE_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${RESOURCE_DEST}"
        COMMENT "Copying shader resources to output directory"
    )
elseif(PLATFORM_MACOS)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        src/main.cpp
        src/core/window.cpp
        src/core/fps_counter.cpp
        src/GUI/gui.cpp
        src/Graphics/shader.cpp
        src/Graphics/renderer.cpp
        src/Graphics/Particle.cpp
        src/Graphics/Simulation.cpp
        src/Graphics/ParticleSystem.cpp
        src/Common.cpp
        ${IMGUI_SOURCES}
        ${IMGUI_BACKENDS_SOURCES}
    )
    
    # Set macOS bundle properties
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        # Uncomment and configure these if needed:
        # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        # XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.yourcompany.lifesimulation"
    )
    
    # Add bundle resources
    set_source_files_properties(
        ${RESOURCE_DIR}
        PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources/shaders
    )
else()
    # Linux/Unix standard executable
    add_executable(${PROJECT_NAME}
        src/main.cpp
        src/core/window.cpp
        src/core/fps_counter.cpp
        src/GUI/gui.cpp
        src/Graphics/shader.cpp
        src/Graphics/renderer.cpp
        src/Graphics/Particle.cpp
        src/Graphics/Simulation.cpp
        src/Graphics/ParticleSystem.cpp
        src/Common.cpp
        ${IMGUI_SOURCES}
        ${IMGUI_BACKENDS_SOURCES}
    )
    
    # Linux resource handling
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${RESOURCE_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${RESOURCE_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${RESOURCE_DEST}"
        COMMENT "Copying shader resources to output directory"
    )
endif()

# Define any needed preprocessor macros
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    GLM_FORCE_SIMD_AVX2  # Enable SIMD optimizations in GLM
    # MAX_PARTICLES=1000000 # Define max particles globally
)

# Platform-specific preprocessor definitions
if(PLATFORM_WINDOWS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX  # Prevents Windows from defining min/max macros
        WIN32_LEAN_AND_MEAN
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/core
    external/glad/include
    external/glm
    external/imgui
    external/imgui/backends
)

# Link with required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL3_LIBRARIES}
    glad
)

# Platform-specific system monitoring libraries
if(PLATFORM_WINDOWS)
    # Windows Performance Data Helper (PDH) library
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pdh.lib
        psapi.lib
    )
elseif(PLATFORM_LINUX)
    # Linux system monitoring libraries
    find_library(LIBPROCPS procps)
    if(LIBPROCPS)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPROCPS})
    endif()
endif()

# Platform-specific libraries
if(PLATFORM_MACOS)
    if(OPENMP_LIBRARIES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENMP_LIBRARIES})
    endif()
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework OpenGL"
        "-framework Accelerate"
        "-framework CoreFoundation"
        "-framework IOKit"
    )
elseif(PLATFORM_WINDOWS)
    if(OPENMP_FOUND OR OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENMP_LIBRARIES})
    endif()
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
        pdh.lib
        psapi.lib
    )
    
    # Configure startup project for Visual Studio 
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
else()
    # Linux/Unix
    if(OPENMP_FOUND OR OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENMP_LIBRARIES})
    endif()
    
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        ${LIBPROCPS}
    )
    
    # On some Linux systems, additional libraries might be needed
    find_library(MATH_LIBRARY m)
    if(MATH_LIBRARY)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${MATH_LIBRARY})
    endif()
endif()

# Additional development/debugging targets
if(PLATFORM_MACOS)
    add_custom_target(run_instrumented
        COMMAND xcrun instruments -t "Time Profiler" $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running with Instruments Time Profiler"
    )
    
    add_custom_target(run_metal_validation
        COMMAND METAL_DEVICE_WRAPPER_TYPE=1 $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running with Metal API validation"
    )
endif()

# Add a simple run target that works across platforms
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E env 
        $<IF:$<BOOL:${PLATFORM_MACOS}>,DYLD_LIBRARY_PATH,LD_LIBRARY_PATH>=$<TARGET_FILE_DIR:${PROJECT_NAME}>
        $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ${PROJECT_NAME}"
)