#define DX12 1
#define VULKAN 0

#if VULKAN
#include <vk_engine.h>
#endif

#if DX12 
#include <dx_engine.h>
#endif


int main(int argc, char* argv[])
{
#if VULKAN
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	
#elif DX12
	DxEngine engine;

	engine.init(1280, 720);

	engine.run();

	engine.cleanup();
#endif
	return 0;
}
