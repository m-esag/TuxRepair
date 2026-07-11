find_package(SQLite3 REQUIRED)

add_library(tuxrepair-core STATIC
    db_manager.cpp
    db_manager.h
    models.h
)

target_include_directories(tuxrepair-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tuxrepair-core PRIVATE SQLite::SQLite3)
