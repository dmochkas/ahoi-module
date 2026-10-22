target_sources(${AHOI_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/ahoi_msg.c"
)

if (WITH_IO)
    target_sources(${AHOI_LIB} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/ahoi_io.c"
    )
endif ()