cmake_minimum_required(VERSION 2.8)


set(USE_DEBUG $ENV{USE_DEBUG})
if(USE_DEBUG)
    message(STATUS "Creating Makefiles for Debug Build")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -D_DEBUG_ -rdynamic -std=gnu99 -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -D_DEBUG_ -rdynamic -std=gnu++0x -D_GNU_SOURCE")
else()
    message(STATUS "Creating Makefiles for Production Build")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -rdynamic -std=gnu99 -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -rdynamic -std=gnu++0x -D_GNU_SOURCE")
endif(USE_DEBUG)

SET(al_data_struct_SRCS
   al_hash.c
   bst.c
   list.c
)

SET(test_exe_SRCS
    test.c
)
include_directories(
    /var/alertlogic/include
)

link_directories(
    /var/alertlogic/lib
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -export-dynamic -avoid-version")
add_library(al_data_struct SHARED ${al_data_struct_SRCS})
target_link_libraries(al_data_struct pthread rdb)

add_executable(test_lib ${test_exe_SRCS})
target_link_libraries(test_lib al_data_struct)

#target_link_libraries(al_data_struct #add dependant libraries here)
########### install files ###############
install(TARGETS al_data_struct DESTINATION /var/alertlogic/lib)
