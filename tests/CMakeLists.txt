find_package(Qt6 6.4 REQUIRED COMPONENTS Core Test)

add_executable(test_gpuinfo
  test_gpuinfo.cpp
  ${CMAKE_SOURCE_DIR}/src/gpuinfo.cpp
)
target_include_directories(test_gpuinfo PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_gpuinfo PRIVATE Qt6::Core Qt6::Test)
if(WIN32)
  target_link_libraries(test_gpuinfo PRIVATE dxgi dxguid)
endif()
add_test(NAME gpuinfo COMMAND test_gpuinfo)

add_executable(test_mix_transition_order
  test_mix_transition_order.cpp
  ${CMAKE_SOURCE_DIR}/src/models/mixtransitionorder.cpp
)
target_include_directories(test_mix_transition_order PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mix_transition_order PRIVATE Qt6::Core Qt6::Test PkgConfig::mlt++)
add_test(NAME mix_transition_order COMMAND test_mix_transition_order)

add_executable(test_agent_rule_engine
  test_agent_rule_engine.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/agentruleengine.cpp
)
target_include_directories(test_agent_rule_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_agent_rule_engine PRIVATE Qt6::Core Qt6::Test)
add_test(NAME agent_rule_engine COMMAND test_agent_rule_engine)

add_executable(test_subtitle_ripple
  test_subtitle_ripple.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/subtitleutils.cpp
)
target_include_directories(test_subtitle_ripple PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_subtitle_ripple PRIVATE Qt6::Core Qt6::Test)
add_test(NAME subtitle_ripple COMMAND test_subtitle_ripple)

add_executable(test_agent_tool_registry
  test_agent_tool_registry.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/agenttoolregistry.cpp
)
target_include_directories(test_agent_tool_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_agent_tool_registry PRIVATE Qt6::Core Qt6::Test)
add_test(NAME agent_tool_registry COMMAND test_agent_tool_registry)

add_executable(test_local_llama_provider
  test_local_llama_provider.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/agenttoolregistry.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/localllamaprovider.cpp
)
target_include_directories(test_local_llama_provider PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_local_llama_provider PRIVATE Qt6::Core Qt6::Test)
add_test(NAME local_llama_provider COMMAND test_local_llama_provider)

add_executable(test_media_library test_media_library.cpp
  ${CMAKE_SOURCE_DIR}/src/agent/medialibrary.cpp)
target_include_directories(test_media_library PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_media_library PRIVATE Qt6::Core Qt6::Test)
add_test(NAME media_library COMMAND test_media_library)
