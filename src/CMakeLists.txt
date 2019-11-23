set(PROJECT rtmp2rtsp)

find_package(PkgConfig)

pkg_check_modules(GLIB glib-2.0)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(SOUP libsoup-2.4)
pkg_check_modules(JSON json-glib-1.0)

include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${SOUP_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_executable(${PROJECT} rtsp.c http.c main.c)

target_link_libraries(${PROJECT}
    glib-2.0
    gobject-2.0
    gstreamer-1.0
    gstrtsp-1.0
    gstrtspserver-1.0
    soup-2.4
    json-glib-1.0)

install(TARGETS ${PROJECT} DESTINATION bin)
