find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt_add_executable(tst_mockbackend
    tst_mockbackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MockBackend.cpp
)
target_include_directories(tst_mockbackend PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mockbackend PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mockbackend COMMAND tst_mockbackend)
