cmake_minimum_required(VERSION 3.10)
project(ahoi-module VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(config.cmake)

set(AHOI_LIB ${PROJECT_NAME})

add_library(${AHOI_LIB})

add_subdirectory(include)
add_subdirectory(src)

if (WITH_LOGS)
    find_package(zlog QUIET)
    if (zlog_FOUND)
        target_link_libraries(${AHOI_LIB} PRIVATE zlog::zlog)
        target_compile_definitions(${AHOI_LIB} PRIVATE zlog_FOUND)
    endif ()
endif ()

if (WITH_IO)
    find_package(io-lib REQUIRED)
    target_link_libraries(${AHOI_LIB} PRIVATE io-lib::io-lib)
    target_compile_definitions(${AHOI_LIB} PRIVATE IO_ENABLED)
endif ()

install(TARGETS ${AHOI_LIB}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
        ${PROJECT_SOURCE_DIR}/include/ahoi-module.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

