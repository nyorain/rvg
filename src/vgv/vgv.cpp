#include "vgv/vgv.hpp"
#include <vpp/vk.hpp>
#include <vpp/formats.hpp>
#include <vpp/imageOps.hpp>
#include <dlg/dlg.hpp>
#include <cstring>

#include <shaders/fill.vert.h>
#include <shaders/fill.frag.h>

namespace vgv {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data)
{
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

} // anon namespace

// Paint
constexpr auto paintUboSize = sizeof(float) * 4;
Paint::Paint(Context& ctx, const Color& xcolor) : color(xcolor) {
	ubo_ = ctx.device().bufferAllocator().alloc(true, paintUboSize,
		vk::BufferUsageBits::uniformBuffer);
	updateDevice();

	ds_ = {ctx.dsLayoutPaint(), ctx.dsPool()};
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Paint::bind(Context& ctx, vk::CommandBuffer cmdBuf) {
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 0, {ds_}, {});
}

void Paint::updateDevice() {
	auto map = ubo_.memoryMap();
	std::memcpy(map.ptr(), &color, sizeof(float) * 4);
}

// Context
Context::Context(vpp::Device& dev, vk::RenderPass rp, unsigned int subpass) :
	device_(dev)
{
	// TODO
	constexpr auto sampleCount = vk::SampleCountBits::e1;

	// sampler
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::linear;
	samplerInfo.minFilter = vk::Filter::linear;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::linear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.borderColor = vk::BorderColor::floatTransparentBlack;
	samplerInfo.unnormalizedCoordinates = false;
	sampler_ = {dev, samplerInfo};

	// layouts
	auto paintDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::fragment),
	};

	auto texDSB = {
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, -1, 1, &sampler_.vkHandle()),
	};

	dsLayoutPaint_ = {dev, paintDSB};
	dsLayoutTex_ = {dev, texDSB};
	pipeLayout_ = {dev, {dsLayoutPaint_, dsLayoutTex_}, {}};

	// pool
	vk::DescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = 20;
	poolSizes[0].type = vk::DescriptorType::uniformBuffer;

	poolSizes[1].descriptorCount = 20;
	poolSizes[1].type = vk::DescriptorType::combinedImageSampler;

	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo.maxSets = 20;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	dsPool_ = {dev, poolInfo};

	// dummies
	auto eii = vpp::ViewableImageCreateInfo::color(dev, {1, 1, 1}).value();
	eii.img.imageType = vk::ImageType::e2d;
	eii.view.viewType = vk::ImageViewType::e2d;
	emptyImage_ = {dev, eii};
	
	// TODO: fill the pixel (solid white!)
	vpp::changeLayout(emptyImage_.vkImage(),
		vk::ImageLayout::undefined, vk::PipelineStageBits::topOfPipe, {},
		vk::ImageLayout::general, vk::PipelineStageBits::fragmentShader,
		vk::AccessBits::shaderRead, 
		{vk::ImageAspectBits::color, 0, 1, 0, 1},
		dev.queueSubmitter());

	dummyTex_ = {dsLayoutTex_, dsPool_};
	vpp::DescriptorSetUpdate update(dummyTex_);
	auto layout = vk::ImageLayout::general;
	update.imageSampler({{{}, emptyImage_.vkImageView(), layout}});

	// pipeline
	auto fillVertex = vpp::ShaderModule(dev, fill_vert_data);
	auto fillFragment = vpp::ShaderModule(dev, fill_frag_data);

	vpp::ShaderProgram fillStages({
		{fillVertex, vk::ShaderStageBits::vertex},
		{fillFragment, vk::ShaderStageBits::fragment}
	});

	vk::GraphicsPipelineCreateInfo fillPipeInfo;

	fillPipeInfo.renderPass = rp;
	fillPipeInfo.subpass = subpass;
	fillPipeInfo.layout = pipeLayout_;
	fillPipeInfo.stageCount = fillStages.vkStageInfos().size();
	fillPipeInfo.pStages = fillStages.vkStageInfos().data();

	// vertex attribs: vec2 pos, vec2 uv
	vk::VertexInputAttributeDescription vertexAttribs[2] = {};
	vertexAttribs[0].format = vk::Format::r32g32Sfloat;

	vertexAttribs[1].format = vk::Format::r32g32Sfloat;
	vertexAttribs[1].offset = sizeof(float) * 2;
	vertexAttribs[1].location = 1;

	// both in the same buffer
	vk::VertexInputBindingDescription vertexBindings[1] = {};
	vertexBindings[0].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[0].stride = sizeof(float) * 4;
	// vertexBindings[0].stride = sizeof(float) * 2;

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.pVertexAttributeDescriptions = vertexAttribs;
	vertexInfo.vertexAttributeDescriptionCount = 2;
	// vertexInfo.vertexAttributeDescriptionCount = 1;
	vertexInfo.pVertexBindingDescriptions = vertexBindings;
	vertexInfo.vertexBindingDescriptionCount = 1;
	fillPipeInfo.pVertexInputState = &vertexInfo;

	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo.topology = vk::PrimitiveTopology::triangleFan;
	fillPipeInfo.pInputAssemblyState = &assemblyInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
	rasterizationInfo.polygonMode = vk::PolygonMode::fill;
	rasterizationInfo.cullMode = vk::CullModeBits::none;
	rasterizationInfo.frontFace = vk::FrontFace::counterClockwise;
	rasterizationInfo.depthClampEnable = false;
	rasterizationInfo.rasterizerDiscardEnable = false;
	rasterizationInfo.depthBiasEnable = false;
	rasterizationInfo.lineWidth = 1.f;
	fillPipeInfo.pRasterizationState = &rasterizationInfo;

	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo.rasterizationSamples = sampleCount;
	multisampleInfo.sampleShadingEnable = false;
	multisampleInfo.alphaToCoverageEnable = false;
	fillPipeInfo.pMultisampleState = &multisampleInfo;

	vk::PipelineColorBlendAttachmentState blendAttachment;
	blendAttachment.blendEnable = true;
	blendAttachment.alphaBlendOp = vk::BlendOp::add;
	blendAttachment.srcColorBlendFactor = vk::BlendFactor::srcAlpha;
	blendAttachment.dstColorBlendFactor = vk::BlendFactor::oneMinusSrcAlpha;
	blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::one;
	blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::zero;
	blendAttachment.colorWriteMask =
		vk::ColorComponentBits::r |
		vk::ColorComponentBits::g |
		vk::ColorComponentBits::b |
		vk::ColorComponentBits::a;

	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = 1;
	blendInfo.pAttachments = &blendAttachment;
	fillPipeInfo.pColorBlendState = &blendInfo;

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;
	fillPipeInfo.pViewportState = &viewportInfo;

	const auto dynStates = {
		vk::DynamicState::viewport,
		vk::DynamicState::scissor};

	vk::PipelineDynamicStateCreateInfo dynamicInfo;
	dynamicInfo.dynamicStateCount = dynStates.size();
	dynamicInfo.pDynamicStates = dynStates.begin();
	fillPipeInfo.pDynamicState = &dynamicInfo;

	auto pipes = vk::createGraphicsPipelines(dev, {}, {fillPipeInfo});
	fanPipe_ = {dev, pipes[0]};
}

// Polygon
Polygon::Polygon(Context&, bool indirect) : indirect_(indirect) {
}

bool Polygon::updateDevice(Context& ctx, DrawMode mode) {
	bool rerecord = false;
	if(mode == DrawMode::fill || mode == DrawMode::both) {
		auto neededSize = sizeof(Vec2f) * points_.size();
		if(indirect_) {
			neededSize += sizeof(vk::DrawIndirectCommand);
		}

		if(fill_.size() < neededSize) {
			neededSize *= 2;
			fill_ = ctx.device().bufferAllocator().alloc(true, neededSize,
				vk::BufferUsageBits::vertexBuffer);
			rerecord = true;
		}

		auto map = fill_.memoryMap();
		auto ptr = map.ptr();
		if(indirect_) {
			vk::DrawIndirectCommand cmd {};
			cmd.instanceCount = 1;
			cmd.vertexCount = points_.size() / 2;
			write(ptr, cmd);
		}

		std::memcpy(ptr, points_.data(), points_.size() * sizeof(float) * 2);
	}

	if(mode == DrawMode::stroke || mode == DrawMode::both) {
		// TODO
	}

	return rerecord;
}

void Polygon::stroke(Context&, vk::CommandBuffer) {
	// TODO
}

void Polygon::fill(Context& ctx, vk::CommandBuffer cmdb) {
	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.fanPipe());
	if(indirect_) {
		auto offset = fill_.offset() + sizeof(vk::DrawIndirectCommand);
		vk::cmdBindVertexBuffers(cmdb, 0, {fill_.buffer()}, {offset});
		vk::cmdDrawIndirect(cmdb, fill_.buffer(), fill_.offset(), 1, 0);
	} else {
		vk::cmdBindVertexBuffers(cmdb, 0, {fill_.buffer()}, {fill_.offset()});
		vk::cmdDraw(cmdb, points_.size() / 2, 1, 0, 0);
	}
}

} // namespace vgv
