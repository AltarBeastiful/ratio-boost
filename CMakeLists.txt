project(ratio-boost C)

cmake_minimum_required(VERSION 2.6)

# Use the package PkgConfig to detect GTK+ headers/library files
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

if (GTK3_FOUND)
    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER})

    add_executable(ratioboost blex.c hash.c list.c urle.c gtk_main.c)
    target_link_libraries(ratioboost crypto curl ${GTK3_LIBRARIES})
endif (GTK3_FOUND)

add_executable(ratioboost-cli list.c blex.c hash.c urle.c main.c)
target_link_libraries(ratioboost-cli crypto curl)
