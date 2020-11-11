cmake_minimum_required(VERSION 3.10.1)

project(among-us-ste-edition)

# ${PROJECT_NAME}

#include(CheckCXXCompilerFlag)
#CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
#if(COMPILER_SUPPORTS_CXX11)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
#else()
#    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support.")
#endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

find_package(X11 REQUIRED)
message(STATUS "X11_FOUND = ${X11_FOUND}")
message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

if(UNIX AND NOT APPLE)
    find_package(PNG REQUIRED)
    message(STATUS "PNG_FOUND = ${PNG_FOUND}")
    message(STATUS "PNG_INCLUDE_DIR = ${PNG_INCLUDE_DIR}")
    message(STATUS "PNG_LIBRARIES = ${PNG_LIBRARIES}")

    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    message(STATUS "OpenGL_FOUND = ${OpenGL_FOUND}")
    message(STATUS "OpenGL_INCLUDE_DIR = ${OPENGL_INCLUDE_DIR}")
    message(STATUS "OpenGL_LIBRARIES = ${OPENGL_gl_LIBRARY}")

    #find_package(Boost REQUIRED)
    #message(STATUS "Boost_FOUND = ${Boost_FOUND}")
    #message(STATUS "Boost_INCLUDE_DIR = ${Boost_INCLUDE_DIRS}")
    #message(STATUS "Boost_LIBRARIES = ${Boost_LIBRARY_DIRS}")

    add_executable(server.o Server.cpp helper/helper.h helper/GameInstance.cpp)
    add_executable(client.o Client.cpp helper/helper.h helper/GameInstance.cpp helper/olcPixelGameEngine.h)
    add_executable(mapdesigner.o MapDesigner.cpp helper/olcPixelGameEngine.h)

    set_property(TARGET server.o PROPERTY CXX_STANDARD 17)
    set_property(TARGET client.o PROPERTY CXX_STANDARD 17)
    set_property(TARGET mapdesigner.o PROPERTY CXX_STANDARD 17)

    target_link_libraries(server.o PUBLIC Threads::Threads ${X11_LIBRARIES} ${PNG_LIBRARIES})
    target_include_directories(server.o PUBLIC asio-1.18.0)
    target_link_directories(server.o PUBLIC asio-1.18.0)

    target_link_libraries(client.o PUBLIC Threads::Threads OpenGL::GL ${X11_LIBRARIES} ${PNG_LIBRARIES})
    target_include_directories(client.o PUBLIC asio-1.18.0)
    target_link_directories(client.o PUBLIC asio-1.18.0)
    target_include_directories(client.o PUBLIC pgex)
    target_link_directories(client.o PUBLIC pgex)

    target_link_libraries(mapdesigner.o PUBLIC Threads::Threads OpenGL::GL ${X11_LIBRARIES} ${PNG_LIBRARIES})
endif()