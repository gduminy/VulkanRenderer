# Add source to this project's executable.
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

file(GLOB_RECURSE HLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.hlsl"
    )

add_executable(vulkan_guide
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_pipeline.h
    vk_pipeline.cpp
    vk_mesh.h
    vk_mesh.cpp
    vk_textures.h
    vk_textures.cpp
    dx_engine.h
    dx_engine.cpp
    d3dx12.h
    Helper.h
    dx_mesh.h
    dx_mesh.cpp
    SimpleCamera.h
    SimpleCamera.cpp
    dx_texture.h
    dx_texture.cpp
    dx_initializers.h
    dx_initializers.cpp
    ${GLSL_SOURCE_FILES}
    ${HLSL_SOURCE_FILES})

set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image D3D12MemoryAllocator)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)
# DX12 libraries
target_link_libraries(vulkan_guide
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

add_dependencies(vulkan_guide Shaders)