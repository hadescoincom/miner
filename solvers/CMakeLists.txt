set(SOURCES
    hdsHash_III_solver.cpp  hdsHash_II_solver.cpp hdsSolvers.h
)

add_library(solvers ${SOURCES})
target_include_directories(solvers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kernels)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../hds)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../clHeaders)
