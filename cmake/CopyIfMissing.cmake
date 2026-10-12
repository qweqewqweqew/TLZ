# Copy SRC to DST only when DST does not already exist.
if(NOT DEFINED SRC OR NOT DEFINED DST)
  message(FATAL_ERROR "CopyIfMissing.cmake requires -DSRC=... -DDST=...")
endif()
if(NOT EXISTS "${DST}")
  get_filename_component(_dst_dir "${DST}" DIRECTORY)
  file(MAKE_DIRECTORY "${_dst_dir}")
  file(COPY "${SRC}" DESTINATION "${_dst_dir}")
endif()
