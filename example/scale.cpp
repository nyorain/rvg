// Example utility for creating window and vulkan swapchain setup
#include "render.hpp"

// the used rvg headers.
#include <rvg/context.hpp>
#include <rvg/shapes.hpp>
#include <rvg/paint.hpp>
#include <rvg/state.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>

// katachi to build our bezier curves or use svg
#include <katachi/path.hpp>
#include <katachi/svg.hpp>

// vpp to allow more high-level vulkan usage.
#include <vpp/handles.hpp>
#include <vpp/debugReport.hpp>
#include <vpp/formats.hpp>
#include <vpp/physicalDevice.hpp>

// some matrix/vector utilities
#include <nytl/vecOps.hpp>
#include <nytl/matOps.hpp>

// logging/debugging
#include <dlg/dlg.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <chrono>
#include <array>

// where the resources are located.
static const std::string baseResPath = "../";

using namespace nytl;
using namespace nytl::vec::operators;
using namespace nytl::vec::cw::operators;

constexpr auto appName = "rvg-example";
constexpr auto engineName = "vpp;rvg";
constexpr auto useValidation = true;
constexpr auto startMsaa = vk::SampleCountBits::e1;
constexpr auto layerName = "VK_LAYER_LUNARG_standard_validation";
constexpr auto printFrames = true;
constexpr auto vsync = true;
constexpr auto clearColor = std::array<float, 4>{{0.f, 0.f, 0.f, 1.f}};

class App {
public:
	App(rvg::Context& ctx);

	void draw(vk::CommandBuffer);
	void resize(Vec2ui size);

	void update(double) {}
	void clicked(Vec2f) {}
	void key(unsigned, bool);

private:
	rvg::Context& ctx_;
	rvg::Transform transform_;
	rvg::Font font_;
	rvg::Paint paint_;
	rvg::Paint paintGreen_;

	rvg::Text text_;
	rvg::RectShape rect_;
	rvg::CircleShape circle_;

	Vec2ui size_;
	Vec2f scale_ {1.f, 1.f};
	float angle_ {0.f};
};

template<typename T>
void scale(nytl::Mat4<T>& mat, nytl::Vec3<T> fac) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][i] *= fac[i];
	}
}

template<typename T>
void translate(nytl::Mat4<T>& mat, nytl::Vec3<T> move) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][3] += move[i];
	}
}

template<typename T = float>
nytl::Mat3<T> rotMat(double angle) {
	auto mat = nytl::identity<3, T>();

	auto c = std::cos(angle);
	auto s = std::sin(angle);

	mat[0][0] = c;
	mat[0][1] = -s;
	mat[1][0] = s;
	mat[1][1] = c;

	return mat;
}

App::App(rvg::Context& ctx) : ctx_(ctx),
		font_(ctx_, baseResPath + "example/OpenSans-Regular.ttf") {
	transform_ = {ctx};
	text_ = {ctx, {0.f, 50.f}, "Just some sample text", font_, 100.f};
	paint_ = {ctx, rvg::colorPaint({200u, 180u, 190u})};
	paintGreen_ = {ctx, rvg::colorPaint({140u, 250u, 180u})};

	rvg::DrawMode dm;
	dm.fill = true;
	dm.stroke = 4.f;
	dm.aaFill = true;
	dm.aaStroke = true;
	rect_ = {ctx, {5.f, 400.f}, {100.f, 300.f}, dm, {20.f, 10.f, 40.f, 0.f}};
	circle_ = {ctx, {300.f, 400.f}, {80.f, 140.f}, dm};
}

void App::resize(Vec2ui size) {
	auto mat = nytl::identity<4, float>();
	auto s = nytl::Vec {2.f / size.x, 2.f / size.y, 1};
	scale(mat, s);
	translate(mat, {-1, -1, 0});
	*transform_.change() = mat;
	size_ = size;
}

void App::draw(vk::CommandBuffer cb) {
	transform_.bind(cb);
	paint_.bind(cb);
	text_.draw(cb);
	rect_.fill(cb);
	circle_.fill(cb);

	paintGreen_.bind(cb);
	rect_.stroke(cb);
	circle_.stroke(cb);
}

void App::key(unsigned key, bool pressed) {
	if(!pressed) {
		return;
	}

	auto t = false;
	if(key == GLFW_KEY_I) {
		scale_ *= 1.1;
		t = true;
	} else if(key == GLFW_KEY_O) {
		scale_ *= 1.f / 1.1;
		t = true;
	} else if(key == GLFW_KEY_X) {
		scale_.x *= 1.f / 1.1;
		t = true;
	} else if(key == GLFW_KEY_Y) {
		scale_.y *= 1.f / 1.1;
		t = true;
	} else if(key == GLFW_KEY_Q) {
		angle_ += 0.1;
		t = true;
	} else if(key == GLFW_KEY_E) {
		angle_ -= 0.1;
		t = true;
	}

	if(t) {
		/*
		auto size = size_;
		auto mat = nytl::identity<4, float>();
		auto s = nytl::Vec {2.f / size.x, 2.f / size.y, 1};
		scale(mat, s);
		translate(mat, {-1, -1, 0});
		mat[0][0] *= scale_.x;
		mat[1][1] *= scale_.y;
		*transform_.change() = mat;
		*/

		// dlg_info("height: {}", scale_.y);
		// text_.change()->height = scale_.y;

		auto mat = nytl::identity<3, float>();
		mat[0][0] = scale_.x;
		mat[1][1] = scale_.y;
		mat = rotMat(angle_) * mat;

		text_.change()->transform = mat;
		rect_.change()->transform = mat;
		circle_.change()->transform = mat;
	}
}

// main
int main() {
	// - initialization -
	if(!::glfwInit()) {
		throw std::runtime_error("Failed to init glfw");
	}

	// vulkan init: instance
	uint32_t count;
	const char** extensions = ::glfwGetRequiredInstanceExtensions(&count);

	std::vector<const char*> iniExtensions {extensions, extensions + count};
	iniExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	vk::ApplicationInfo appInfo (appName, 1, engineName, 1, VK_API_VERSION_1_0);
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = &appInfo;

	instanceInfo.enabledExtensionCount = iniExtensions.size();
	instanceInfo.ppEnabledExtensionNames = iniExtensions.data();

	if(useValidation) {
		auto layers = {
			layerName,
			// "VK_LAYER_RENDERDOC_Capture",
		};

		instanceInfo.enabledLayerCount = layers.size();
		instanceInfo.ppEnabledLayerNames = layers.begin();
	}

	vpp::Instance instance {};
	try {
		instance = {instanceInfo};
		if(!instance.vkInstance()) {
			throw std::runtime_error("vkCreateInstance returned a nullptr");
		}
	} catch(const std::exception& error) {
		dlg_error("Vulkan instance creation failed: {}", error.what());
		dlg_error("\tYour system may not support vulkan");
		dlg_error("\tThis application requires vulkan to work");
		throw;
	}

	// debug callback
	std::unique_ptr<vpp::DebugCallback> debugCallback;
	if(useValidation) {
		debugCallback = std::make_unique<vpp::DebugCallback>(instance);
	}

	// init glfw window
	const auto size = nytl::Vec {1200u, 800u};
	::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = ::glfwCreateWindow(size.x, size.y, "rvg", NULL, NULL);
	if(!window) {
		throw std::runtime_error("Failed to create glfw window");
	}

	// avoiding reinterpret_cast due to aliasing warnings
	VkInstance vkini;
	auto handle = instance.vkHandle();
	std::memcpy(&vkini, &handle, sizeof(vkini));
	static_assert(sizeof(VkInstance) == sizeof(vk::Instance));

	VkSurfaceKHR vkSurf {};
	VkResult err = ::glfwCreateWindowSurface(vkini, window, NULL, &vkSurf);
	if(err) {
		auto str = std::string("Failed to create vulkan surface: ");
		str += vk::name(static_cast<vk::Result>(err));
		throw std::runtime_error(str);
	}

	vk::SurfaceKHR surface {};
	std::memcpy(&surface, &vkSurf, sizeof(surface));
	static_assert(sizeof(VkSurfaceKHR) == sizeof(vk::SurfaceKHR));

	// create device
	// enable some extra features
	float priorities[1] = {0.0};

	auto phdevs = vk::enumeratePhysicalDevices(instance);
	auto phdev = vpp::choose(phdevs, surface);

	auto queueFlags = vk::QueueBits::compute | vk::QueueBits::graphics;
	int queueFam = vpp::findQueueFamily(phdev, surface, queueFlags);

	vk::DeviceCreateInfo devInfo;
	vk::DeviceQueueCreateInfo queueInfo({}, queueFam, 1, priorities);

	auto exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	devInfo.pQueueCreateInfos = &queueInfo;
	devInfo.queueCreateInfoCount = 1u;
	devInfo.ppEnabledExtensionNames = exts.begin();
	devInfo.enabledExtensionCount = 1u;

	auto features = vk::PhysicalDeviceFeatures {};
	features.shaderClipDistance = true;
	devInfo.pEnabledFeatures = &features;

	auto device = vpp::Device(instance, phdev, devInfo);
	auto presentQueue = device.queue(queueFam);

	auto renderInfo = RendererCreateInfo {
		device, surface, size, *presentQueue,
		startMsaa, vsync, clearColor
	};

	// optional so we can manually destroy it before vulkan surface
	auto orenderer = std::optional<Renderer>(renderInfo);
	auto& renderer = *orenderer;

	// app
	rvg::Context ctx(device, {renderer.renderPass(), 0, true});
	App app(ctx);

	// render recoreding
	renderer.onRender += [&](vk::CommandBuffer cb){
		ctx.bindDefaults(cb);
		app.draw(cb);
	};

	ctx.updateDevice();
	renderer.invalidate();

	// connect window & renderer
	struct WinInfo {
		Renderer* renderer;
		App* app;
	} winInfo = {
		&renderer,
		&app,
	};

	::glfwSetWindowUserPointer(window, &winInfo);

	auto sizeCallback = [](auto* window, int width, int height) {
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		auto size = nytl::Vec {unsigned(width), unsigned(height)};
		winInfo.renderer->resize(size);
		winInfo.app->resize(size);
	};

	auto keyCallback = [](auto* window, int key, int, int action, int) {
		auto pressed = action != GLFW_RELEASE;
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		winInfo.app->key(key, pressed);
	};

	auto mouseCallback = [](auto* window, int button, int action, int) {
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		auto pressed = action == GLFW_PRESS;
		if(pressed && button == GLFW_MOUSE_BUTTON_LEFT) {
			double x,y;
			::glfwGetCursorPos(window, &x, &y);
			winInfo.app->clicked({float(x), float(y)});
		}
	};

	::glfwSetFramebufferSizeCallback(window, sizeCallback);
	::glfwSetKeyCallback(window, keyCallback);
	::glfwSetMouseButtonCallback(window, mouseCallback);

	// - main loop -
	using Clock = std::chrono::high_resolution_clock;
	using Secf = std::chrono::duration<float, std::ratio<1, 1>>;

	auto lastFrame = Clock::now();
	auto fpsCounter = 0u;
	auto secCounter = 0.f;

	while(!::glfwWindowShouldClose(window)) {
		auto now = Clock::now();
		auto diff = now - lastFrame;
		auto dt = std::chrono::duration_cast<Secf>(diff).count();
		lastFrame = now;

		app.update(dt);
		::glfwPollEvents();

		auto [rec, seph] = ctx.upload();

		if(rec) {
			dlg_info("Rerecording due to context");
			renderer.invalidate();
		}

		auto wait = {
			vpp::StageSemaphore {
				seph,
				vk::PipelineStageBits::allGraphics,
			}
		};

		vpp::RenderInfo info;
		if(seph) {
			info.wait = wait;
		}

		renderer.renderSync(info);

		if(printFrames) {
			++fpsCounter;
			secCounter += dt;
			if(secCounter >= 1.f) {
				dlg_info("{} fps", fpsCounter);
				secCounter = 0.f;
				fpsCounter = 0;
			}
		}
	}

	orenderer.reset();
	vkDestroySurfaceKHR(vkini, vkSurf, nullptr);
	::glfwDestroyWindow(window);
	::glfwTerminate();
}

