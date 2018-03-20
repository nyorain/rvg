#include <vgv/vgv.hpp>
#include <vgv/path.hpp>

#include <vpp/vk.hpp>
#include <vpp/formats.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/queue.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>
#include <nytl/vecOps.hpp>
#include <cstring>

#include <vgv/nk_font/font.h>

#include <shaders/fill.vert.h>
#include <shaders/fill.frag.h>

namespace vgv {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

} // anon namespace

// Context
Context::Context(vpp::Device& dev, vk::RenderPass rp, unsigned int subpass) :
	device_(dev)
{
	constexpr auto sampleCount = vk::SampleCountBits::e1;

	// sampler
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::nearest;
	samplerInfo.minFilter = vk::Filter::nearest;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.25f;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::nearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.borderColor = vk::BorderColor::floatOpaqueWhite;
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

	auto transformDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
	};

	dsLayoutPaint_ = {dev, paintDSB};
	dsLayoutTex_ = {dev, texDSB};
	dsLayoutTransform_ = {dev, transformDSB};

	pipeLayout_ = {dev, {dsLayoutPaint_, dsLayoutTex_, dsLayoutTransform_}, {}};

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
	constexpr std::uint8_t bytes[] = {0xFF, 0xFF, 0xFF, 0xFF};
	emptyImage_ = createTexture(dev, 1, 1,
		reinterpret_cast<const std::byte*>(bytes),
		TextureType::rgba32);

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

	vk::GraphicsPipelineCreateInfo fanPipeInfo;

	fanPipeInfo.renderPass = rp;
	fanPipeInfo.subpass = subpass;
	fanPipeInfo.layout = pipeLayout_;
	fanPipeInfo.stageCount = fillStages.vkStageInfos().size();
	fanPipeInfo.pStages = fillStages.vkStageInfos().data();
	fanPipeInfo.flags = vk::PipelineCreateBits::allowDerivatives;

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
	fanPipeInfo.pVertexInputState = &vertexInfo;

	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo.topology = vk::PrimitiveTopology::triangleFan;
	fanPipeInfo.pInputAssemblyState = &assemblyInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
	rasterizationInfo.polygonMode = vk::PolygonMode::fill;
	rasterizationInfo.cullMode = vk::CullModeBits::none;
	rasterizationInfo.frontFace = vk::FrontFace::counterClockwise;
	rasterizationInfo.depthClampEnable = false;
	rasterizationInfo.rasterizerDiscardEnable = false;
	rasterizationInfo.depthBiasEnable = false;
	rasterizationInfo.lineWidth = 1.f;
	fanPipeInfo.pRasterizationState = &rasterizationInfo;

	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo.rasterizationSamples = sampleCount;
	multisampleInfo.sampleShadingEnable = false;
	multisampleInfo.alphaToCoverageEnable = false;
	fanPipeInfo.pMultisampleState = &multisampleInfo;

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
	fanPipeInfo.pColorBlendState = &blendInfo;

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;
	fanPipeInfo.pViewportState = &viewportInfo;

	const auto dynStates = {
		vk::DynamicState::viewport,
		vk::DynamicState::scissor};

	vk::PipelineDynamicStateCreateInfo dynamicInfo;
	dynamicInfo.dynamicStateCount = dynStates.size();
	dynamicInfo.pDynamicStates = dynStates.begin();
	fanPipeInfo.pDynamicState = &dynamicInfo;

	// listPipe
	auto listPipeInfo = fanPipeInfo;
	listPipeInfo.flags = vk::PipelineCreateBits::derivative;
	listPipeInfo.basePipelineIndex = 0;

	auto listAssemblyInfo = assemblyInfo;
	listAssemblyInfo.topology = vk::PrimitiveTopology::triangleList;
	listPipeInfo.pInputAssemblyState = &listAssemblyInfo;

	// stripPipe
	auto stripPipeInfo = listPipeInfo;
	auto stripAssemblyInfo = assemblyInfo;
	stripAssemblyInfo.topology = vk::PrimitiveTopology::triangleStrip;
	stripPipeInfo.pInputAssemblyState = &stripAssemblyInfo;

	auto pipes = vk::createGraphicsPipelines(dev, {},
		{fanPipeInfo, listPipeInfo, stripPipeInfo});
	fanPipe_ = {dev, pipes[0]};
	listPipe_ = {dev, pipes[1]};
	stripPipe_ = {dev, pipes[2]};
}

// Polygon
Polygon::Polygon(Context&, bool indirect) : indirect_(indirect) {
}

bool Polygon::updateDevice(Context& ctx, DrawMode mode) {
	bool rerecord = false;
	if(points_.empty()) {
		return false;
	}

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
		// TODO: what about join/cap modes? width?
		constexpr auto width = 10.f;
		auto baked = bakeStroke(points_, width);
		strokeCount_ = baked.size();
		auto neededSize = sizeof(Vec2f) * points_.size() * 4;

		if(stroke_.size() < neededSize) {
			neededSize *= 2;
			stroke_ = ctx.device().bufferAllocator().alloc(true, neededSize,
				vk::BufferUsageBits::vertexBuffer);
			rerecord = true;
		}

		auto map = stroke_.memoryMap();
		auto ptr = map.ptr();
		if(indirect_) {
			vk::DrawIndirectCommand cmd {};
			cmd.instanceCount = 1;
			cmd.vertexCount = baked.size();
			write(ptr, cmd);
		}

		for(auto& p : baked) {
			write(ptr, p);
			write(ptr, Vec {0.f, 0.f}); // (unused) uv
		}
	}

	return rerecord;
}

void Polygon::stroke(Context& ctx, vk::CommandBuffer cmdb) {
	if(points_.empty()) {
		return;
	}

	dlg_assert(stroke_.size());
	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());
	if(indirect_) {
		auto offset = stroke_.offset() + sizeof(vk::DrawIndirectCommand);
		vk::cmdBindVertexBuffers(cmdb, 0, {stroke_.buffer()}, {offset});
		vk::cmdDrawIndirect(cmdb, stroke_.buffer(), stroke_.offset(), 1, 0);
	} else {
		vk::cmdBindVertexBuffers(cmdb, 0, {stroke_.buffer()}, {stroke_.offset()});
		vk::cmdDraw(cmdb, strokeCount_, 1, 0, 0);
	}
}

void Polygon::fill(Context& ctx, vk::CommandBuffer cmdb) {
	if(points_.empty()) {
		return;
	}

	dlg_assert(fill_.size());
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

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) {
	atlas_ = std::make_unique<nk_font_atlas>();
	nk_font_atlas_init_default(&nkAtlas());
	ds_ = {ctx.dsLayoutTex(), ctx.dsPool()};
}

FontAtlas::~FontAtlas() {
	nk_font_atlas_cleanup(&nkAtlas());
}

bool FontAtlas::bake(Context& ctx) {
	// TODO: use r8 format. Figure out why i did not work
	constexpr auto format = vk::Format::r8g8b8a8Uint;
	bool rerecord = false;

	int w, h;
	auto data = reinterpret_cast<const std::byte*>(
		nk_font_atlas_bake(&nkAtlas(), &w, &h, NK_FONT_ATLAS_RGBA32));

	if(w == 0 || h == 0) {
		dlg_info("FontAtlas::bake on empty atlas");
		return false;
	}

	dlg_assert(w > 0 && h > 0);
	auto uw = unsigned(w), uh = unsigned(h);
	if(uw > width_ || uh > height_) {
		// TODO: allocate more than needed? 2d problem here though,
		//   the rectpack algorithm does not only produce squares
		texture_ = vgv::createTexture(ctx.device(), w, h,
			reinterpret_cast<const std::byte*>(data),
			vgv::TextureType::rgba32);
		rerecord = true;

		vpp::DescriptorSetUpdate update(ds_);
		update.imageSampler({{{}, texture_.vkImageView(),
			vk::ImageLayout::general}});
	} else {
		auto& qs = ctx.device().queueSubmitter();
		auto cmdBuf = ctx.device().commandAllocator().get(qs.queue().family());
		vk::beginCommandBuffer(cmdBuf, {});
		auto buf = vpp::fillStaging(cmdBuf, texture_.image(), format,
			vk::ImageLayout::general, {uw, uh, 1}, {data, w * h * 4u},
			{vk::ImageAspectBits::color});
		vk::endCommandBuffer(cmdBuf);

		// TODO: performance! Return the work/wait for submission
		qs.wait(qs.add({{}, {}, {}, 1, &cmdBuf.vkHandle(), {}, {}}));
	}

	return rerecord;
}

// Font
Font::Font(FontAtlas& atlas, const char* file, unsigned h) : atlas_(&atlas)
{
	font_ = nk_font_atlas_add_from_file(&atlas.nkAtlas(), file, h, nullptr);
	if(!font_) {
		std::string err = "Could not load font ";
		err.append(file);
		throw std::runtime_error(err);
	}
}

Font::Font(FontAtlas& atlas, struct nk_font* font) :
	atlas_(&atlas), font_(font)
{
}

unsigned Font::width(const char* text) {
	dlg_assert(font_);
	auto& handle = font_->handle;
	return handle.width(handle.userdata, handle.height, text,
		std::strlen(text));
}

// Text
Text::Text(Context&, const char* xtext, Font& xfont, Vec2f xpos) :
	text(xtext), font(&xfont), pos(xpos)
{
}

bool Text::updateDevice(Context& ctx) {
	dlg_assert(font && font->nkFont());

	bool rerecord = false;
	auto neededSize = sizeof(float) * 16 * std::strlen(text);
	if(verts_.size() < neededSize) {
		neededSize *= 2;
		verts_ = ctx.device().bufferAllocator().alloc(true, neededSize,
			vk::BufferUsageBits::vertexBuffer);
		rerecord = true;
	}

	auto map = verts_.memoryMap();
	auto ptr = map.ptr();

	auto x = pos.x;
	auto gen_point = [&](const nk_font_glyph& glyph, unsigned i) {
		float wx, wy, wu, wv;

		if(i == 0 || i == 3) {
			wu = glyph.u0;
			wx = x + glyph.x0;
		} else {
			wu = glyph.u1;
			wx = x + glyph.x0 + glyph.w;
		}

		if(i == 2 || i == 3) {
			wv = glyph.v0;
			wy = pos.y + glyph.y0;
		} else {
			wv = glyph.v1;
			wy = pos.y + glyph.y0 + glyph.h;
		}

		write(ptr, wx);
		write(ptr, wy);
		write(ptr, wu);
		write(ptr, wv);
	};

	for(auto it = text; *it; ++it) {
		auto pglyph = nk_font_find_glyph(font->nkFont(), *it);
		if(!pglyph) {
			dlg_warn("glyph '{}' not found in font", *it);
			break;
		}

		auto& glyph = *pglyph;

		gen_point(glyph, 0);
		gen_point(glyph, 1);
		gen_point(glyph, 2);

		gen_point(glyph, 0);
		gen_point(glyph, 2);
		gen_point(glyph, 3);
		x += glyph.xadvance;
	}

	return rerecord;
}

void Text::draw(Context& ctx, vk::CommandBuffer cmdb) {
	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.listPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 1, {font->atlas().ds()}, {});

	vk::cmdBindVertexBuffers(cmdb, 0, {verts_.buffer()}, {verts_.offset()});
	vk::cmdDraw(cmdb, std::strlen(text) * 6, 1, 0, 0);
}

// Paint
constexpr auto paintUboSize = sizeof(float) * 4;
Paint::Paint(Context& ctx, const Color& xcolor) : color(xcolor) {
	ubo_ = ctx.device().bufferAllocator().alloc(true, paintUboSize,
		vk::BufferUsageBits::uniformBuffer);
	ds_ = {ctx.dsLayoutPaint(), ctx.dsPool()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Paint::bind(Context& ctx, vk::CommandBuffer cmdBuf) {
	dlg_assert(ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 0, {ds_}, {});
}

void Paint::updateDevice() {
	dlg_assert(ubo_.size() && ds_);
	auto map = ubo_.memoryMap();
	std::memcpy(map.ptr(), &color, sizeof(float) * 4);
}

// Transform
constexpr auto transformUboSize = sizeof(Mat4f);
Transform::Transform(Context& ctx) : Transform(ctx, identity<4, float>())
{
}

Transform::Transform(Context& ctx, const Mat4f& m) : matrix(m) {
	ubo_ = ctx.device().bufferAllocator().alloc(true, transformUboSize,
		vk::BufferUsageBits::uniformBuffer);
	ds_ = {ctx.dsLayoutTransform(), ctx.dsPool()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Transform::updateDevice() {
	dlg_assert(ubo_.size() && ds_);
	auto map = ubo_.memoryMap();
	std::memcpy(map.ptr(), &matrix, sizeof(Mat4f));
}

void Transform::bind(Context& ctx, vk::CommandBuffer cmdBuf) {
	dlg_assert(ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 2, {ds_}, {});
}

} // namespace vgv
