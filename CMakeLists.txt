cmake_minimum_required(VERSION 3.10)
project(ahoi-module VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)

set(AHOI_LIB ${PROJECT_NAME})

find_package(io-lib REQUIRED)

add_library(${AHOI_LIB})

add_subdirectory(include)
add_subdirectory(src)

target_link_libraries(${AHOI_LIB} PRIVATE io-lib::io-lib)

install(TARGETS ${AHOI_LIB}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
        ${PROJECT_SOURCE_DIR}/include/ahoi-module.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

