function(engine_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE exchange_core exchange_text)
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/src/core
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

engine_add_test(unit_test_matching unit/core/unit_test_matching.cpp)
engine_add_test(unit_test_queries unit/core/unit_test_queries.cpp)
engine_add_test(unit_test_tif unit/core/unit_test_tif.cpp)
engine_add_test(unit_test_stp unit/core/unit_test_stp.cpp)
engine_add_test(unit_test_engine_internals unit/core/unit_test_engine_internals.cpp)

engine_add_test(unit_test_fixed_point unit/text/unit_test_fixed_point.cpp)
engine_add_test(unit_test_json unit/text/unit_test_json.cpp)

engine_add_test(unit_test_http unit/net/unit_test_http.cpp)
target_link_libraries(unit_test_http PRIVATE exchange_net)
