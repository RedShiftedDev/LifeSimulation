# This file is part of the "Learn WebGPU for C++" book.
#   https://eliemichel.github.io/LearnWebGPU

include(FetchContent)

set(WEBGPU_BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN (it does not matter when using emcmake)")
set_property(CACHE WEBGPU_BACKEND PROPERTY STRINGS EMSCRIPTEN WGPU WGPU_STATIC DAWN)

# FetchContent's GIT_SHALLOW option is buggy and does not actually do a shallow
# clone. This macro takes care of it.
macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
	FetchContent_Declare(
		"${Name}"

		# Manual download mode instead:
		DOWNLOAD_COMMAND
			cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
			git init &&
			git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
			git reset --hard FETCH_HEAD
	)
endmacro()

if (NOT TARGET webgpu)
	string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)

	if (EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")

		FetchContent_DeclareShallowGit(
			webgpu-backend-emscripten
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        fa0b54d68841fb33188403b07959d403b24511de # emscripten-v3.1.61 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-emscripten)

	elseif (WEBGPU_BACKEND_U STREQUAL "WGPU")

		FetchContent_DeclareShallowGit(
			webgpu-backend-wgpu
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        54a60379a9d792848a2311856375ceef16db150e # wgpu-v0.19.4.1 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-wgpu)

	elseif (WEBGPU_BACKEND_U STREQUAL "WGPU_STATIC")

		FetchContent_DeclareShallowGit(
			webgpu-backend-wgpu-static
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        992fef64da25072ebe3844a73f7103105e7fd133 # wgpu-static-v0.19.4.1 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-wgpu-static)

	elseif (WEBGPU_BACKEND_U STREQUAL "DAWN")

		FetchContent_DeclareShallowGit(
			webgpu-backend-dawn
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        f49f0f3f6784a86a85944600d66f743e0c7eb4a9 # dawn-6536 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-dawn)

	else()

		message(FATAL_ERROR "Invalid value for WEBGPU_BACKEND: possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN, but '${WEBGPU_BACKEND_U}' was provided.")

	endif()
endif()
function(target_copy_webgpu_binaries target)
    # Construct binary path from correct subfolder (WEBGPU_PLATFORM is set globally)
    set(WEBGPU_BIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/_deps/webgpu-backend-wgpu-src/bin/${WEBGPU_PLATFORM}")

    # Copy the correct WebGPU dynamic library to the build output dir
    if(EXISTS "${WEBGPU_BIN_PATH}/libwgpu_native.so")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBGPU_BIN_PATH}/libwgpu_native.so"
            $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying libwgpu_native.so to output directory"
        )
    elseif(EXISTS "${WEBGPU_BIN_PATH}/wgpu_native.dll")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBGPU_BIN_PATH}/wgpu_native.dll"
            $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying wgpu_native.dll to output directory"
        )
    elseif(EXISTS "${WEBGPU_BIN_PATH}/libwgpu_native.dylib")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBGPU_BIN_PATH}/libwgpu_native.dylib"
            $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying libwgpu_native.dylib to output directory"
        )
    else()
        message(WARNING "WebGPU binary not found in ${WEBGPU_BIN_PATH}")
    endif()
endfunction()
