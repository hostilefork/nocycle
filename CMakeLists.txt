# This is an input file for the CMake makefile generator
# http://en.wikipedia.org/wiki/CMake

#
#  CMakeLists.txt - This is a centralization of the flags for conditional
#     compilation of the Nocycle library...mostly to do with debug
#     code.  Although the boost library is needed for much of the
#     testing, if you aren't doing a test build then it should not
#     be necessary to include any boost libraries (at least, not yet).
#
#  Copyright (c) 2009 HostileFork.com
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
#  See http://hostilefork.com/nocycle for documentation.
#

# If you want to run cmake in an interactive mode that gives you prompts
# for build options, use "ccmake" (or one of the graphical interfaces)
#
# https://cmake.org/cmake/help/latest/manual/ccmake.1.html
#
# Some resources for learning how to use CMake:
#
# http://www.drdobbs.com/cpp/the-cmake-build-manager/184405251
# http://www.elpauer.org/stuff/learning_cmake.pdf


### SETUP ###
### http://www.cmake.org/cmake/help/examples.html

# The name of our project is "NOCYCLE". CMakeLists files in this project can
# refer to the root source directory of the project as ${NOCYCLE_SOURCE_DIR}
# and to the root binary directory of the project as ${NOCYCLE_BINARY_DIR}.
#
cmake_minimum_required (VERSION 2.6)
project (nocycle)


### OPTIONS ###
### http://www.cmake.org/cmake/help/cmake2.6docs.html#command:option

# Several pieces of Nocycle have self testing code.  These tests may rely on
# the boost library.  If you would like to build a version of Nocycle without
# a dependency on boost, make sure all these are set to NO.
#
option (NSTATE_SELFTEST "Self-test Nstate library?" NO)
option (ORIENTEDGRAPH_SELFTEST "Self-test Oriented Graph?" NO)
option (DIRECTEDACYCLICGRAPH_SELFTEST "Self-test Directed Acyclic Graph?" NO)

# Experimental attempt to cache transitive closure, not for general use
#
option (
    DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
    "Use experimental transitive closure cache (doubles data structure size)"
    NO
)

if (DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY)
    #
    # If caching the transitive closure...
    # ...there is an "extra tristate" we get in the canreach graph when there
    # is a physical edge in the data graph.  We can use this to accelerate the
    # invalidation process, but for testing purposes it's nice to make sure the
    # algorithms aren't corrupting this implicitly when modifying other edges.
    #
    option (
        DIRECTEDACYCLICGRAPH_USER_TRISTATE
        "Expose spare per-node tristate in transitive closure to client code?"
        NO
    )

    # If caching the transitive closure...
    # ...and the extra tristate per edge is NOT visible to the user...
    # ...then we can use it to cache whether a vertex can still be reached even
    # if a physical link to it is removed from the graph.
    #
    if (NOT DIRECTEDACYCLICGRAPH_USER_TRISTATE)
        option (
            DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
            "Use per-node tristate in closure for reachability after removals?"
            NO
        )
    endif ()

    # If caching the transitive closure...
    # ...then we might want to perform heavy consistency checks on the
    # transitive closure sidestructure while running.
    #
    option (
        DIRECTEDACYCLICGRAPH_CONSISTENCY_CHECK
        "Heavy (slow!) consistency checks on transitive closure sidestructure?"
        NO
    )
endif (DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY)

option (
    TEST_AGAINST_BOOST
    "Test nocycle against reference implementation built on the boost library?"
    NO
)

if (TEST_AGAINST_BOOST)
    #
    # Though nocycle distinguishes between vertices that have no connections
    # and those which "don't exist", boost's default assumption is that all
    # nodes in its capacity "exist".  The only way to conceptually delete a
    # boost vertex from an adjacency_matrix is to remove all of its incoming
    # and outgoing connections.
    #
    # Though it is possible to inject a vertex property map to track existence,
    # the property map adds overhead and may not fairly represent boost's
    # performance in scenarios with large numbers of nodes when
    # existence tracking is not needed.
    #
    option (
        BOOSTIMPLEMENTATION_TRACK_EXISTENCE
        "Track existence for nodes in the Boost reference implementation?"
        NO
    )
endif (TEST_AGAINST_BOOST)


### CONFIG HEADER ###
### http://www.cmake.org/pipermail/cmake/2003-August/004256.html
### http://www.vtk.org/Wiki/CMake_HowToDoPlatformChecks

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/NocycleConfig.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/NocycleConfig.hpp)
include_directories (${CMAKE_CURRENT_BINARY_DIR})


### COMPILER FLAGS ###

macro (add_cxx_flags flags)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
endmacro ()

# Nocycle was initially C++98, but as it is just some old experimental code it
# might as well be a place to try usages of C++17 features.
#
# Note that this *should* work:
#
#     set(CMAKE_CXX_STANDARD 17)
#
# But for some reason it isn't working on the CMake with Travis CI.
#
# http://stackoverflow.com/questions/40877744/
#
add_cxx_flags("-std=c++17")

# Turn up the warnings very high.
#
#     http://stackoverflow.com/a/9862800/211160
#
# Currently not adding in `-Wshadow`, because @HostileFork likes naming
# constructor arguments the same thing as the variables they initialize.
#
# For compiler identification notes:
#
#     http://stackoverflow.com/a/10055571/211160

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    # GCC and regular Clang or AppleClang share a lot of compatible switches

    add_cxx_flags(-Wall)
    add_cxx_flags(-Wsign-conversion)
    add_cxx_flags(-Wextra)
    add_cxx_flags(-Wcast-align)
    add_cxx_flags(-Wctor-dtor-privacy)
    add_cxx_flags(-Wdisabled-optimization)
    add_cxx_flags(-Wformat=2)
    add_cxx_flags(-Winit-self)
    add_cxx_flags(-Wmissing-declarations)
    add_cxx_flags(-Wmissing-include-dirs)
    add_cxx_flags(-Woverloaded-virtual)
    add_cxx_flags(-Wredundant-decls)
    add_cxx_flags(-Wsign-promo)
    add_cxx_flags(-Wstrict-overflow=5)
    add_cxx_flags(-Wswitch-default)
    add_cxx_flags(-Wundef)
    add_cxx_flags(-Wno-unused)
    add_cxx_flags(-pedantic)

    # We encourage *everyone* who is building the project to use the `-Werror`
    # switch.  This converts messages that are considered "warnings" to the
    # status of being show-stopper errors.  Being rigorous about this helps
    # keep distracting warnings from accumulating in the build process over
    # time, drowning out important messages that should be heeded.
    #
    # However...this idealism isn't long-term compatible with telling the
    # compiler it can throw in as many errors as it can think of (via the
    # switches `-Wpedantic`, `-Wall`, and `-Wextra`).  Each time a new
    # compiler or new compiler version is used to build the project, it may
    # invent new and never before seen warnings that haven't been explicitly
    # disabled yet.
    #
    # Reports to help adjust the warnings to keep them at zero on relevant
    # platforms are preferred to turning off the switch.  But the switch is
    # available to use on the command line: `-DRIGOROUS=no`

    if(NOT DEFINED(RIGOROUS) OR RIGOROUS)
        add_cxx_flags(-Werror)
    endif()

endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

    # using regular Clang or AppleClang

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    # using GCC

    add_cxx_flags(-Wlogical-op)
    add_cxx_flags(-Wstrict-null-sentinel)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    # using Visual Studio C++ (note C++11 and beyond support on by default)

else()

    # !!! If you're not using GCC, Clang, or MSVC then you're pretty much on
    # your own.  Pull requests welcome, however.
    #
    # e.g. if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")...

endif()


### ADDRESS SANITIZER ###

if (SANITIZE MATCHES "yes")
    add_cxx_flags(-fno-omit-frame-pointer)
    add_cxx_flags(-fsanitize=address)

    add_link_debug_flags(-fno-omit-frame-pointer)
    add_link_debug_flags(-fsanitize=address)
endif ()


### BUILD STEPS ###
### http://public.kitware.com/cgi-bin/viewcvs.cgi/Tests/Tutorial/?root=CMake

# Make sure the compiler can find include files from our library.
#
include_directories (${NOCYCLE_SOURCE_DIR})

# Make sure the linker can find the Nocycle library once it is built.
#
link_directories (${NOCYCLE_BINARY_DIR}/nocycle)

# Note: "lib" prefix is added automatically, using lowercase convention
# (libnocycle) because that seems to be the way people do it
#
add_library (nocycle OrientedGraph.cpp DirectedAcyclicGraph.cpp)

if (TEST_AGAINST_BOOST)
    find_package (Boost 1.34 REQUIRED)
    include_directories (${Boost_INCLUDE_DIRS})

    add_executable (PerformanceTest PerformanceTest.cpp)
    # Link the executable to the libnocycle library
    target_link_libraries (PerformanceTest nocycle)
endif (TEST_AGAINST_BOOST)
