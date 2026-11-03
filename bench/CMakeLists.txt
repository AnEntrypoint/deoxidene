add_executable(deoxidene_bench main.cpp)
target_link_libraries(deoxidene_bench PRIVATE deoxidene::deoxidene)
deoxidene_apply_lto(deoxidene_bench)
