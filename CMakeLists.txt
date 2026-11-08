cmake_minimum_required(VERSION 3.24)

project(ArtisansOfSkyrim VERSION 1.8 LANGUAGES C CXX)

# The version is set only on the project() line above. C++ reads it from the
# generated Version.h; the Python tools read it from this file.
foreach(part MAJOR MINOR PATCH)
    set(AOS_VERSION_${part} "${PROJECT_VERSION_${part}}")
    if(AOS_VERSION_${part} STREQUAL "")
        set(AOS_VERSION_${part} 0)
    endif()
endforeach()
configure_file(
    include/ArtisansOfSkyrim/Version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ArtisansOfSkyrim/Version.h"
    @ONLY
)

option(AOS_BUILD_TESTS "Build engine-independent domain tests" ON)

include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "Build dependency tests" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Support Skyrim SE" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Support Skyrim AE" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Support Skyrim VR" FORCE)

FetchContent_Declare(
    openvr
    URL https://codeload.github.com/ValveSoftware/openvr/zip/60eb187801956ad277f1cae6680e3a410ee0873b
    URL_HASH SHA256=51b4deef78c10f52e3e41d85bab51a87a55cd4d87b4d6c65c8952c2cbfafb9ae
    SOURCE_SUBDIR headers
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    CommonLibSSE
    URL https://codeload.github.com/alandtse/CommonLibSSE-NG/zip/refs/tags/v7.2.0
    URL_HASH SHA256=377b447c4923c6f7c615494a793ace8d7b443fb899d4e15dbf95ec2712155ce8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(openvr CommonLibSSE)
target_include_directories(CommonLibSSE SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${openvr_SOURCE_DIR}/headers>"
)
if(MSVC)
    # /pathmap only takes effect together with /experimental:deterministic.
    target_compile_options(CommonLibSSE PRIVATE
        /experimental:deterministic
        "/pathmap:${commonlibsse_SOURCE_DIR}=CommonLibSSE"
        "/pathmap:${CMAKE_BINARY_DIR}=Build"
        "/pathmap:${VCPKG_INSTALLED_DIR}=Dependencies"
    )
endif()

find_package(minhook CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

set(NPCSS_HEADERS
    smithing_include/NPCSmithServices/CourierService.h
    smithing_include/NPCSmithServices/Config.h
    smithing_include/NPCSmithServices/Domain.h
    smithing_include/NPCSmithServices/DurationFormatter.h
    smithing_include/NPCSmithServices/EquipmentOrigin.h
    smithing_include/NPCSmithServices/FormResolver.h
    smithing_include/NPCSmithServices/InputHandler.h
    smithing_include/NPCSmithServices/InventoryService.h
    smithing_include/NPCSmithServices/MenuController.h
    smithing_include/NPCSmithServices/OrderDomain.h
    smithing_include/NPCSmithServices/OrderDialogueService.h
    smithing_include/NPCSmithServices/OrderService.h
    smithing_include/NPCSmithServices/PCH.h
    smithing_include/NPCSmithServices/RecipeIndex.h
    smithing_include/NPCSmithServices/SmithClassifier.h
)

set(AOS_PAPYRUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/papyrus/Source")
set(AOS_DISTRIBUTED_PAPYRUS_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data/Scripts/Source")
set(AOS_PAPYRUS_SOURCE_NAMES
    AOS_TIF_Alchemy.psc
    AOS_TIF_CollectOrders.psc
    AOS_TIF_Enchant.psc
    AOS_TIF_SmithCraft.psc
    AOS_TIF_SmithImprove.psc
    AOS_MCM.psc
    ArtisansOfSkyrim.psc
)
file(MAKE_DIRECTORY "${AOS_DISTRIBUTED_PAPYRUS_SOURCE_DIR}")
set(AOS_PAPYRUS_COMPARE_COMMANDS)
foreach(AOS_PAPYRUS_SOURCE_NAME IN LISTS AOS_PAPYRUS_SOURCE_NAMES)
    set(AOS_PAPYRUS_SOURCE
        "${AOS_PAPYRUS_SOURCE_DIR}/${AOS_PAPYRUS_SOURCE_NAME}")
    set(AOS_DISTRIBUTED_PAPYRUS_SOURCE
        "${AOS_DISTRIBUTED_PAPYRUS_SOURCE_DIR}/${AOS_PAPYRUS_SOURCE_NAME}")
    if(NOT EXISTS "${AOS_PAPYRUS_SOURCE}")
        message(FATAL_ERROR "Missing authoritative Papyrus source: ${AOS_PAPYRUS_SOURCE}")
    endif()
    configure_file(
        "${AOS_PAPYRUS_SOURCE}"
        "${AOS_DISTRIBUTED_PAPYRUS_SOURCE}"
        COPYONLY
    )
    list(APPEND AOS_PAPYRUS_COMPARE_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${AOS_PAPYRUS_SOURCE}"
            "${AOS_DISTRIBUTED_PAPYRUS_SOURCE}"
    )
endforeach()

add_custom_target(
    ArtisansPapyrusSources ALL
    ${AOS_PAPYRUS_COMPARE_COMMANDS}
    COMMENT "Validating distributed Papyrus sources"
    VERBATIM
)

set(NPCSS_SOURCES
    smithing_src/CourierService.cpp
    smithing_src/Config.cpp
    smithing_src/Domain.cpp
    smithing_src/EquipmentOrigin.cpp
    smithing_src/FormResolver.cpp
    smithing_src/InputHandler.cpp
    smithing_src/InventoryService.cpp
    smithing_src/MenuController.cpp
    smithing_src/OrderDomain.cpp
    smithing_src/OrderDialogueService.cpp
    smithing_src/OrderService.cpp
    smithing_src/RecipeIndex.cpp
    smithing_src/SmithClassifier.cpp
)

set(NPCES_HEADERS
    enchanting_include/NPCEnchantingServices/Config.h
    enchanting_include/NPCEnchantingServices/Domain.h
    enchanting_include/NPCEnchantingServices/EnchanterClassifier.h
    enchanting_include/NPCEnchantingServices/EnchantmentCatalog.h
    enchanting_include/NPCEnchantingServices/FormResolver.h
    enchanting_include/NPCEnchantingServices/InputHandler.h
    enchanting_include/NPCEnchantingServices/InventoryService.h
    enchanting_include/NPCEnchantingServices/MenuController.h
    enchanting_include/NPCEnchantingServices/PCH.h
)

set(NPCES_SOURCES
    enchanting_src/Config.cpp
    enchanting_src/Domain.cpp
    enchanting_src/EnchanterClassifier.cpp
    enchanting_src/EnchantmentCatalog.cpp
    enchanting_src/FormResolver.cpp
    enchanting_src/InputHandler.cpp
    enchanting_src/InventoryService.cpp
    enchanting_src/MenuController.cpp
)

set(NPCAS_HEADERS
    include/NPCAlchemyServices/AlchemistClassifier.h
    include/NPCAlchemyServices/AlchemyCatalog.h
    include/NPCAlchemyServices/Config.h
    include/NPCAlchemyServices/Domain.h
    include/NPCAlchemyServices/FormResolver.h
    include/NPCAlchemyServices/InputHandler.h
    include/NPCAlchemyServices/InventoryService.h
    include/NPCAlchemyServices/MenuController.h
    include/NPCAlchemyServices/PCH.h
    include/ArtisansOfSkyrim/MenuSort.h
    include/ArtisansOfSkyrim/ExclusiveEffectRules.h
    include/ArtisansOfSkyrim/ExclusiveEffects.h
    include/ArtisansOfSkyrim/PapyrusBridge.h
    include/ArtisansOfSkyrim/TransactionFeedback.h
    include/ArtisansOfSkyrim/BarterPricing.h
    include/ArtisansOfSkyrim/ServiceDiscount.h
    include/ArtisansOfSkyrim/UserSettings.h
    include/ArtisansOfSkyrim/PriceRounding.h
)

set(NPCAS_SOURCES
    alchemy_src/AlchemistClassifier.cpp
    alchemy_src/AlchemyCatalog.cpp
    alchemy_src/Config.cpp
    alchemy_src/Domain.cpp
    alchemy_src/FormResolver.cpp
    alchemy_src/InputHandler.cpp
    alchemy_src/InventoryService.cpp
    alchemy_src/MenuController.cpp
)

add_library(
    ArtisansOfSkyrim
    SHARED
        ${NPCSS_HEADERS}
        ${NPCSS_SOURCES}
        ${NPCES_HEADERS}
        ${NPCES_SOURCES}
        ${NPCAS_HEADERS}
        ${NPCAS_SOURCES}
        src/ServiceMenu.cpp
        src/Localization.cpp
        src/PapyrusBridge.cpp
        src/ExclusiveEffects.cpp
        src/ServiceDiscount.cpp
        src/UserSettings.cpp
        src/Plugin.cpp
)
add_dependencies(ArtisansOfSkyrim ArtisansPapyrusSources)

target_compile_features(ArtisansOfSkyrim PRIVATE cxx_std_23)
target_compile_definitions(ArtisansOfSkyrim PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(
    ArtisansOfSkyrim
    PRIVATE
        include
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        smithing_include
        enchanting_include
)
target_link_libraries(
    ArtisansOfSkyrim
    PRIVATE
        CommonLibSSE::CommonLibSSE
        minhook::minhook
        tomlplusplus::tomlplusplus
)
target_precompile_headers(
    ArtisansOfSkyrim
    PRIVATE include/NPCAlchemyServices/PCH.h
)

if(MSVC)
    target_compile_options(
        ArtisansOfSkyrim
        PRIVATE
            /permissive-
            /Zc:preprocessor
            /utf-8
            /W4
            /EHsc
            /MP
            /experimental:deterministic
            "/pathmap:${CMAKE_SOURCE_DIR}=."
            "/pathmap:${CMAKE_BINARY_DIR}=Build"
            "/pathmap:${VCPKG_INSTALLED_DIR}=Dependencies"
    )
endif()

set(AOS_PACKAGE_ROOT "${CMAKE_BINARY_DIR}/package")
add_custom_command(
    TARGET ArtisansOfSkyrim
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data/SKSE/Plugins"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:ArtisansOfSkyrim>"
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data/SKSE/Plugins/$<TARGET_FILE_NAME:ArtisansOfSkyrim>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${AOS_PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data"
        "${AOS_PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${AOS_PACKAGE_ROOT}/SKSE/Plugins"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:ArtisansOfSkyrim>"
        "${AOS_PACKAGE_ROOT}/SKSE/Plugins/$<TARGET_FILE_NAME:ArtisansOfSkyrim>"
    VERBATIM
)

if(AOS_BUILD_TESTS)
    enable_testing()

    add_executable(
        ArtisansSmithingDomainTests
        smithing_src/Domain.cpp
        smithing_src/OrderDomain.cpp
        tests/DomainTests.cpp
    )
    target_compile_features(ArtisansSmithingDomainTests PRIVATE cxx_std_20)
    target_include_directories(
        ArtisansSmithingDomainTests
        PRIVATE smithing_include include
    )
    target_link_libraries(
        ArtisansSmithingDomainTests
        PRIVATE tomlplusplus::tomlplusplus
    )
    add_test(
        NAME Artisans.SmithingDomain
        COMMAND ArtisansSmithingDomainTests
    )

    add_executable(
        ArtisansEnchantingDomainTests
        enchanting_src/Domain.cpp
        tests/EnchantingDomainTests.cpp
    )
    target_compile_features(ArtisansEnchantingDomainTests PRIVATE cxx_std_20)
    target_include_directories(
        ArtisansEnchantingDomainTests
        PRIVATE enchanting_include include
    )
    add_test(
        NAME Artisans.EnchantingDomain
        COMMAND ArtisansEnchantingDomainTests
    )

    add_executable(
        ArtisansAlchemyDomainTests
        alchemy_src/Domain.cpp
        tests/AlchemyDomainTests.cpp
    )
    target_compile_features(ArtisansAlchemyDomainTests PRIVATE cxx_std_20)
    target_include_directories(
        ArtisansAlchemyDomainTests
        PRIVATE include
    )
    add_test(
        NAME Artisans.AlchemyDomain
        COMMAND ArtisansAlchemyDomainTests
    )

    add_executable(ArtisansRuntimeCompatibilityTests tests/RuntimeCompatibilityTests.cpp)
    target_compile_features(ArtisansRuntimeCompatibilityTests PRIVATE cxx_std_20)
    target_include_directories(ArtisansRuntimeCompatibilityTests PRIVATE include)
    add_test(NAME Artisans.RuntimeCompatibility COMMAND ArtisansRuntimeCompatibilityTests)

    add_executable(
        ArtisansMenuSortTests
        tests/MenuSortTests.cpp
    )
    target_compile_features(ArtisansMenuSortTests PRIVATE cxx_std_20)
    target_include_directories(
        ArtisansMenuSortTests
        PRIVATE include
    )
    add_test(
        NAME Artisans.MenuSort
        COMMAND ArtisansMenuSortTests
    )

    add_executable(
        ArtisansExclusiveEffectRulesTests
        tests/ExclusiveEffectRulesTests.cpp
    )
    target_compile_features(ArtisansExclusiveEffectRulesTests PRIVATE cxx_std_20)
    target_include_directories(
        ArtisansExclusiveEffectRulesTests
        PRIVATE include
    )
    add_test(
        NAME Artisans.ExclusiveEffectRules
        COMMAND ArtisansExclusiveEffectRulesTests
    )

    foreach(AOS_TEST_TARGET IN ITEMS
        ArtisansRuntimeCompatibilityTests
        ArtisansSmithingDomainTests
        ArtisansEnchantingDomainTests
        ArtisansAlchemyDomainTests
        ArtisansMenuSortTests
        ArtisansExclusiveEffectRulesTests)
        target_compile_options(${AOS_TEST_TARGET} PRIVATE
            "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>"
            "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>"
        )
    endforeach()
endif()

if(MSVC)
    target_link_options(ArtisansOfSkyrim PRIVATE /INCREMENTAL:NO /DEBUG:NONE)
endif()
