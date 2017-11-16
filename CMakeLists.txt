cmake_minimum_required(VERSION 2.8.11)

install(PROGRAMS bin/rtmp2rtspd DESTINATION bin)
install(FILES bin/daemonize DESTINATION bin)

add_subdirectory(src)
