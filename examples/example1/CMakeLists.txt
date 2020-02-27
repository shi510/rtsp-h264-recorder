set(the_app example1)
file(GLOB example1_srcs "*.cc")
file(GLOB example1_hdrs "*.h")
add_executable(${the_app} ${example1_srcs} ${example1_hdrs})
include_directories(${vr_include_dirs})
target_link_libraries(${the_app} vr)
if(MSVC)
	# Windows specific library.
	# target_link_libraries(${the_app} ${OpenCV_LIBS} ${Protobuf_LIBS})
elseif(UNIX AND NOT APPLE)
	# Unix but not apple specific library.
	# target_link_libraries(${the_app} ${OpenCV_LIBS} ${Protobuf_LIBS})
elseif(APPLE)
	# apple specific library.
	# target_link_libraries(${the_app} ${OpenCV_LIBS} ${Protobuf_LIBS})
else()
	message("No support platform.")
endif()
set_target_properties(${the_app} PROPERTIES FOLDER "example1")