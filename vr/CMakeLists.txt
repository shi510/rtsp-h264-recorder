set(the_library "vr")
set(vr_source_files)
set(vr_header_files)

file(GLOB_RECURSE files "*.*")
group_files("${files}")

add_subdirectory(recorder)
add_subdirectory(streamer)
add_subdirectory(utility)
add_subdirectory(video)

include_directories(${vr_include_dirs})
add_library(${the_library} ${LIB_TYPE} ${vr_source_files} ${vr_header_files})
if(UNIX AND NOT APPLE)
	# Unix but not apple specific library.
	target_link_libraries(${the_library} ${vr_lib_deps} stdc++fs pthread)
elseif(APPLE)
	# apple specific library.
	target_link_libraries(${the_library} ${vr_lib_deps} pthread)
else()
	message("No support platform.")
endif()

set_target_properties(${the_library} PROPERTIES FOLDER "vr_library")