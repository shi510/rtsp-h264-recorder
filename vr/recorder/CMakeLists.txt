file(GLOB_RECURSE recorder_srcs "*.cc")
file(GLOB_RECURSE recorder_hdrs "*.h")
list(APPEND vr_source_files ${recorder_srcs})
list(APPEND vr_header_files ${recorder_hdrs})
set(vr_source_files ${vr_source_files} PARENT_SCOPE)
set(vr_header_files ${vr_header_files} PARENT_SCOPE)