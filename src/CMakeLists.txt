set(PROJECT rtmp2rtsp)

project(${PROJECT})

include_directories(
    /usr/include/glib-2.0
    /usr/lib/glib-2.0/include
    /usr/include/gstreamer-1.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_executable(${PROJECT}
    rtmp2rtsp)

target_link_libraries(${PROJECT} glib-2.0 gobject-2.0 gstreamer-1.0 gstrtspserver-1.0)

install(TARGETS ${PROJECT} DESTINATION bin)
