# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.4.1)


find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

find_library( m-lib
              m )

find_library( jnigraphics-lib
              jnigraphics )

add_library(ffmpeg_so SHARED IMPORTED)
set_target_properties(ffmpeg_so PROPERTIES IMPORTED_LOCATION ../../../../src/main/jnilibs/armeabi-v7a/libffmpeg.so)

add_library( video
             SHARED
             src/main/cpp/native-lib.cpp )

include_directories(src/main/jni/include)

target_link_libraries( video
                       ffmpeg_so
                       ${log-lib}
                       ${m-lib}
                       ${jnigraphics-lib})