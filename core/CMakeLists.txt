find_package(SQLite3 REQUIRED)

add_library(tuxrepair-core STATIC
    db_manager.cpp
    db_manager.h
    models.h
)

target_include_directories(tuxrepair-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# db_manager.h includes <sqlite3.h> and is exported PUBLIC above, so the link
# dependency must be PUBLIC too — otherwise consumers see the header but have
# no link dependency on the sqlite library (ABI / link mismatch).
target_link_libraries(tuxrepair-core
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        TuxRepair::Warnings
)

# Enforce the language standard at the target level, not just globally.
target_compile_features(tuxrepair-core PUBLIC cxx_std_23)
