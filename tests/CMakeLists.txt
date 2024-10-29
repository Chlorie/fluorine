# Unit test

find_package(Catch2 CONFIG REQUIRED)
include(CTest)
include(Catch)

function (add_test_target FILE)
    string(REPLACE "/" "." FILE_DOTTED ${FILE})
    set(TGT "test.${FILE_DOTTED}")
    add_executable(${TGT} "${FILE}.cpp")
    set_target_properties(${TGT} PROPERTIES FOLDER tests)
    target_link_libraries(${TGT} PRIVATE
        project_options
        fluorine
        Catch2::Catch2WithMain
    )
    target_set_cxx_std(${TGT})
    target_set_output_dirs(${TGT})
    catch_discover_tests(${TGT})
endfunction ()

add_test_target("example")
