// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>
#include <vk_mesh.h>
#include <unordered_map>

#include <glm/glm.hpp>

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void flush()
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)(); //call the function
		}

		deletors.clear();
	}
};

struct Material 
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject
{
	Mesh* mesh;
	Material* material;
	glm::mat4 transformMatrix;
};

class VulkanEngine {
public:

	DeletionQueue _mainDeletionQueue;

	//Init Vulkan
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	//swap chain
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	
	VkRenderPass _renderPass;

	std::vector<VkFramebuffer> _frameBuffers;

	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VmaAllocator _allocator;

	VkImageView _depthImageView;
	AllocatedImage _depthImage;

	VkFormat _depthFormat;

	std::vector<RenderObject> _renderable;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;

	int _selectedShader{ 0 };

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	Material* getMaterial(const std::string& name);

	Mesh* getMesh(const std::string& name);

	void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

private:
	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	void init_pipeline();

	void init_scene();

	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModuel);

	void load_meshes();

	void upload_mesh(Mesh& mesh);
};


