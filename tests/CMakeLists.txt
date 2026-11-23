cmake_minimum_required(VERSION 3.16)
project(IceAdventureRulesTests LANGUAGES CXX)
enable_testing()
set(project_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(source "${project_root}/Classes")

add_library(puzzle_rules STATIC
    "${source}/Puzzle.cpp" "${source}/LevelRules.cpp" "${source}/OriginalBoard.cpp"
    "${source}/Rewards.cpp" "${source}/OriginalLevel.cpp" "${source}/Campaign.cpp")
target_include_directories(puzzle_rules PUBLIC "${source}" PRIVATE "${project_root}/engine/external")
target_compile_features(puzzle_rules PUBLIC cxx_std_14)

foreach(test_name IN ITEMS puzzle_test original_test rewards_test)
    add_executable(${test_name} "${test_name}.cpp")
    target_link_libraries(${test_name} PRIVATE puzzle_rules)
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_name} PRIVATE -UNDEBUG)
    endif()
endforeach()
add_test(NAME custom_campaign COMMAND puzzle_test Resources/campaign.json)
add_test(NAME original_campaign COMMAND original_test)
add_test(NAME map_rewards COMMAND rewards_test)
set_tests_properties(custom_campaign original_campaign map_rewards PROPERTIES WORKING_DIRECTORY "${project_root}")

add_executable(index_original "../tools/index_original.cpp")
target_link_libraries(index_original PRIVATE puzzle_rules)
