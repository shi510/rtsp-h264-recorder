
if(BUILD_EXAMPLE)
	include(cmake/find_ffmpeg.cmake)
	if(FFMPEG_FOUND)
		list(APPEND vr_include_dirs ${FFMPEG_INCLUDE_DIR})
		list(APPEND vr_lib_deps ${FFMPEG_LIBRARIES})
	endif()
endif()
