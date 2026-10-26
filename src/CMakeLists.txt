# Layer targets, from the pure domain outwards. Dependencies point one way:
# core <- text/api <- server <- app.

add_library(exchange_core STATIC
    core/engine.cpp
    core/engine_queries.cpp
    core/order_book.cpp
    core/order_lifecycle.cpp
    core/order_validation.cpp
    core/trade_posting.cpp
)
target_include_directories(exchange_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(exchange_core PUBLIC engine_build_flags)

add_library(exchange_text STATIC
    text/fixed_point.cpp
    text/json.cpp
)
target_include_directories(exchange_text PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(exchange_text PUBLIC engine_build_flags)

add_library(exchange_net STATIC
    net/http.cpp
    net/socket.cpp
)
target_include_directories(exchange_net PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(exchange_net PUBLIC engine_build_flags)
if(WIN32)
    target_link_libraries(exchange_net PUBLIC ws2_32)
endif()

add_library(exchange_api STATIC
    api/api_orders.cpp
    api/api_queries.cpp
    api/handler.cpp
    api/metrics.cpp
    api/serialization.cpp
)
target_include_directories(exchange_api PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(exchange_api PUBLIC exchange_core exchange_text engine_build_flags)

add_library(exchange_server STATIC
    server/http_server.cpp
)
target_include_directories(exchange_server PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(exchange_server PUBLIC
    exchange_api
    exchange_net
    Threads::Threads
    spdlog::spdlog
)

add_executable(matching-engine app/main.cpp)
target_link_libraries(matching-engine PRIVATE exchange_server)
