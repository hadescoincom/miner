set(SOURCES
    hdsStratum.cpp hdsStratum.h
)

add_library(stratum ${SOURCES})
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../hds)
