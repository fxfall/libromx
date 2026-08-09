add_executable(romx_phase1_tests test_phase1.c)
target_link_libraries(romx_phase1_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase1_tests PRIVATE c_std_99)
set_target_properties(romx_phase1_tests PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(romx_phase1_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase1_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_test(NAME romx.phase1 COMMAND romx_phase1_tests)

add_executable(romx_phase2_tests test_phase2.c)
target_link_libraries(romx_phase2_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase2_tests PRIVATE c_std_99)
set_target_properties(romx_phase2_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_phase2_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase2_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
add_test(NAME romx.phase2 COMMAND romx_phase2_tests)

add_executable(romx_phase3_tests test_phase3.c)
target_link_libraries(romx_phase3_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase3_tests PRIVATE c_std_99)
set_target_properties(romx_phase3_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_phase3_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase3_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
add_test(NAME romx.phase3 COMMAND romx_phase3_tests)

find_package(Threads REQUIRED)
add_executable(romx_phase4_tests test_phase4.c)
target_link_libraries(romx_phase4_tests PRIVATE ROMX::romx Threads::Threads)
target_compile_features(romx_phase4_tests PRIVATE c_std_99)
set_target_properties(romx_phase4_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_phase4_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase4_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
add_test(NAME romx.phase4 COMMAND romx_phase4_tests)

add_executable(romx_phase5_tests test_phase5.c)
target_link_libraries(romx_phase5_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase5_tests PRIVATE c_std_99)
set_target_properties(romx_phase5_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_phase5_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase5_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
add_test(NAME romx.phase5 COMMAND romx_phase5_tests)

add_executable(romx_conformance_tests test_conformance.c)
target_link_libraries(romx_conformance_tests PRIVATE ROMX::romx)
target_compile_features(romx_conformance_tests PRIVATE c_std_99)
set_target_properties(romx_conformance_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_conformance_tests PRIVATE /W4)
else()
    target_compile_options(romx_conformance_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
if(EXISTS "${ROMX_CONFORMANCE_FIXTURE_DIR}/minimal-valid.romx")
    add_test(NAME romx.conformance COMMAND romx_conformance_tests
        "${ROMX_CONFORMANCE_FIXTURE_DIR}")
endif()

add_executable(romx_writer_conformance_tests test_writer_conformance.c)
target_link_libraries(romx_writer_conformance_tests PRIVATE ROMX::romx)
target_compile_features(romx_writer_conformance_tests PRIVATE c_std_99)
set_target_properties(romx_writer_conformance_tests PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(romx_writer_conformance_tests PRIVATE /W4)
else()
    target_compile_options(romx_writer_conformance_tests PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
if(EXISTS "${ROMX_CONFORMANCE_FIXTURE_DIR}/writer/payload-only.romx")
    add_test(NAME romx.writer_conformance
        COMMAND romx_writer_conformance_tests
        "${ROMX_CONFORMANCE_FIXTURE_DIR}")
endif()

add_executable(romx_phase6_tests test_phase6.c)
target_link_libraries(romx_phase6_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase6_tests PRIVATE c_std_99)
set_target_properties(romx_phase6_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

add_executable(romx_phase7_tests test_phase7.c)
target_link_libraries(romx_phase7_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase7_tests PRIVATE c_std_99)
set_target_properties(romx_phase7_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

add_executable(romx_phase8_tests test_phase8.c)
target_link_libraries(romx_phase8_tests PRIVATE ROMX::romx Threads::Threads)
target_compile_features(romx_phase8_tests PRIVATE c_std_99)
set_target_properties(romx_phase8_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

enable_language(CXX)
add_executable(romx_phase8_cpp_tests test_phase8_cpp.cpp)
target_link_libraries(romx_phase8_cpp_tests PRIVATE ROMX::romx)
target_compile_features(romx_phase8_cpp_tests PRIVATE cxx_std_11)

foreach(target romx_phase6_tests romx_phase7_tests romx_phase8_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endforeach()

if(MSVC)
    target_compile_options(romx_phase8_cpp_tests PRIVATE /W4)
else()
    target_compile_options(romx_phase8_cpp_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(MSVC)
    # The test programs intentionally use the portable stdio API for fixture
    # files. Keep the CRT deprecation warning local to test targets instead of
    # weakening warnings for the library itself.
    foreach(target
        romx_phase1_tests romx_phase2_tests romx_phase3_tests
        romx_phase4_tests romx_phase5_tests romx_conformance_tests
        romx_writer_conformance_tests romx_phase6_tests romx_phase7_tests
        romx_phase8_tests romx_phase8_cpp_tests)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endforeach()
    target_compile_options(romx_phase8_cpp_tests PRIVATE /EHsc)
endif()

add_test(NAME romx.phase6 COMMAND romx_phase6_tests)
add_test(NAME romx.phase7 COMMAND romx_phase7_tests)
add_test(NAME romx.phase8 COMMAND romx_phase8_tests)
add_test(NAME romx.phase8_cpp COMMAND romx_phase8_cpp_tests)
