message(STATUS "")
message(STATUS "Starting to configure CalibrationTools tests..")
message(STATUS "")
enable_testing()

set(UNIT_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CalibrationToolsTest.cpp)

#Configure Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.50 REQUIRED COMPONENTS unit_test_framework
                                            filesystem)

if(NOT TARGET Boost::unit_test_framework)
    add_library(Boost::unit_test_framework IMPORTED INTERFACE)
    set_property(TARGET Boost::unit_test_framework PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::unit_test_framework PROPERTY
        INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
endif()
#End of configuration of Boost

macro(package_add_test TESTNAME)
    string(REPLACE "Test" "" TEST_SOURCE ${TESTNAME}) #Remove Test from test name to get source to test
    add_executable(${TESTNAME}.x EXCLUDE_FROM_ALL ../calibrationProgram/${TEST_SOURCE} ${ARGN}) #Tests sources are in parent dir
    target_compile_options(${TESTNAME}.x PRIVATE -Wunused-parameter -Wall)
    target_link_libraries(${TESTNAME}.x JPetFramework::JPetFramework Boost::unit_test_framework)
    add_test(NAME ${TESTNAME}.x COMMAND ${TESTNAME}.x --log_level=error --log_format=XML --log_sink=${TESTNAME}.xml)
    set_target_properties(${TESTNAME}.x PROPERTIES FOLDER tests)
    add_dependencies(${TESTNAME}.x link_target_calibrationTools)
endmacro()


## Add custom target to create symlink from test dir to unitTestData
add_custom_target(link_target_calibrationTools ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../../unitTestData ${CMAKE_CURRENT_BINARY_DIR}/unitTestData)



foreach(test_source IN ITEMS ${UNIT_TEST_SOURCES})
    get_filename_component(test ${test_source} NAME_WE)
    package_add_test(${test} ${test_source} ../calibrationProgram/CalibrationTools.cpp)
    list(APPEND tests_names ${test}.x)
endforeach()

add_custom_target(tests_calibProg DEPENDS ${tests_names})
