#include "vgv/vgv.hpp"
#include <vpp/vk.hpp>
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
Paint::Paint(Context& ctx, const Color& color) {
	ubo_ = ctx.device().bufferAllocator().alloc(true, paintUboSize, 
		vk::BufferUsageBits::uniformBuffer);

	{
		auto map = ubo_.memoryMap();
		std::memcpy(map.ptr(), &color, sizeof(float) * 4);
	}

	ds_ = {ctx.dsLayout(), ctx.dsPool()};
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Paint::bind(Context& ctx, vk::CommandBuffer cmdBuf) {
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 0, {ds_}, {});
}

// Context
Context::Context(vpp::Device& dev, vk::RenderPass rp, unsigned int subpass) {
	// TODO
	constexpr auto sampleCount = vk::SampleCountBits::e1;

	// layouts
	auto dsBindings = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::fragment),
	};

	dsLayout_ = {dev, dsBindings};
	pipeLayout_ = {dev, {dsLayout_}, {}};

	// pool
	vk::DescriptorPoolSize poolSizes[1] = {};
	poolSizes[0].descriptorCount = 20;
	poolSizes[0].type = vk::DescriptorType::uniformBuffer;

	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo.maxSets = 20;	
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = poolSizes;
	dsPool_ = {dev, poolInfo};
	
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

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.pVertexAttributeDescriptions = vertexAttribs;
	vertexInfo.vertexAttributeDescriptionCount = 2;
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

const vpp::Device& Context::device() const {
	return dsLayout_.device();
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
			fill_ = ctx.device().bufferAllocator().alloc(true, paintUboSize, 
				vk::BufferUsageBits::uniformBuffer);
			rerecord = true;
		}

		auto map = fill_.memoryMap();
		auto ptr = map.ptr();
		if(indirect_) {
			vk::DrawIndirectCommand cmd {};
			cmd.instanceCount = 1;
			cmd.vertexCount = points_.size();
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
		vk::cmdDraw(cmdb, points_.size(), 1, 0, 0);
	}
}

} // namespace vgv
