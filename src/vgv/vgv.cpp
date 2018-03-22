#include <vgv/vgv.hpp>

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

// TODO(performance): optionally create (static) Polygons in deviceLocal memory.

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

	pipeLayout_ = {dev, {dsLayoutPaint_, dsLayoutTex_, dsLayoutTransform_},
		{{vk::ShaderStageBits::fragment, 0, sizeof(float)}}};

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
	vertexAttribs[1].location = 1;
	vertexAttribs[1].binding = 1;

	// position and uv are in different buffers
	// this allows polygons that don't use any uv-coords to simply
	// reuse the position buffer which will result in better performance
	// (due to caching) and waste less memory
	vk::VertexInputBindingDescription vertexBindings[2] = {};
	vertexBindings[0].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[0].stride = sizeof(float) * 2; // position
	vertexBindings[0].binding = 0;

	vertexBindings[1].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[1].stride = sizeof(float) * 2; // uv
	vertexBindings[1].binding = 1;

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.pVertexAttributeDescriptions = vertexAttribs;
	vertexInfo.vertexAttributeDescriptionCount = 2;
	vertexInfo.pVertexBindingDescriptions = vertexBindings;
	vertexInfo.vertexBindingDescriptionCount = 2;
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
	// TODO: use r8 format. Figure out why it did not work
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

float Font::width(StringParam text) const {
	dlg_assert(font_);
	auto& handle = font_->handle;
	return handle.width(handle.userdata, handle.height, text.c_str(),
		text.size());
}

float Font::height() const {
	dlg_assert(font_);
	return font_->handle.height;
}

// Text
Text::Text(Context&, std::string xtext, const Font& xfont, Vec2f xpos,
	bool indirect) : text(std::move(xtext)), font(&xfont), pos(xpos),
		indirect_(indirect) {
}

// TODO(performance): put the vertex baking outside of this function in an
// extra update function that just stores the results in the object.
// Or call it automatically everytime text is changed.
// Since those function could then be called while the gpu is still rendering.
// That also caches the vertices vector for fewer allocations.
bool Text::updateDevice(Context& ctx) {
	dlg_assert(font && font->nkFont());
	bool rerecord = false;

	// we first calculate all vertices since we cannot know the needed
	// size up front (because of utf-8 encoding).
	std::vector<Vec2f> positions;
	std::vector<Vec2f> uvs;

	// good approximcation for usually-ascii
	positions.reserve(text.size());
	uvs.reserve(text.size());

	auto x = pos.x;
	auto addVert = [&](const nk_font_glyph& glyph, unsigned i) {
		auto left = i == 0 || i == 3;
		auto top = i == 0 || i == 1;

		positions.push_back({
			x + (left ? glyph.x0 : glyph.x1),
			pos.y + (top ? glyph.y0 : glyph.y1)});
		uvs.push_back({
			left ? glyph.u0 : glyph.u1,
			top ? glyph.v0 : glyph.v1});
	};

	for(auto c : text) {
		auto pglyph = nk_font_find_glyph(font->nkFont(), c);
		if(!pglyph) {
			dlg_error("nk_font_find_glyph return null for {}", c);
			break;
		}

		auto& glyph = *pglyph;
		for(auto i : {0, 1, 2, 0, 2, 3}) {
			addVert(glyph, i);
		}

		x += glyph.xadvance;
	}

	// now upload data to gpu
	dlg_assert(positions.size() == uvs.size());
	auto byteSize = sizeof(Vec2f) * positions.size();
	auto neededSize = byteSize;
	if(indirect_) {
		neededSize += sizeof(vk::DrawIndirectCommand);
	}

	if(positionBuf_.size() < neededSize) {
		positionBuf_ = {};
		uvBuf_ = {};
		ctx.device().bufferAllocator().reserve(true,
			(neededSize + byteSize) * 2, vk::BufferUsageBits::vertexBuffer);
		positionBuf_ = ctx.device().bufferAllocator().alloc(true,
			neededSize * 2, vk::BufferUsageBits::vertexBuffer);
		uvBuf_ = ctx.device().bufferAllocator().alloc(true,
			byteSize * 2, vk::BufferUsageBits::vertexBuffer);
		rerecord = true;
	}

	auto map = positionBuf_.memoryMap();
	auto ptr = map.ptr();

	// the positionBuf contains the indirect draw command, if there is any
	if(indirect_) {
		vk::DrawIndirectCommand cmd {};
		cmd.vertexCount = positions.size();
		cmd.instanceCount = 1;
		write(ptr, cmd);
	} else {
		drawCount_ = positions.size();
	}

	std::memcpy(ptr, positions.data(), byteSize);

	map = uvBuf_.memoryMap();
	ptr = map.ptr();
	std::memcpy(ptr, uvs.data(), byteSize);

	return rerecord;
}

void Text::draw(Context& ctx, vk::CommandBuffer cmdb) {
	if(!positionBuf_.size()) {
		return;
	}

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.listPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 1, {font->atlas().ds()}, {});

	auto ioffset = indirect_ ? sizeof(vk::DrawIndirectCommand) : 0u;
	vk::cmdBindVertexBuffers(cmdb, 0, {positionBuf_.buffer(), uvBuf_.buffer()},
		{positionBuf_.offset() + ioffset, uvBuf_.offset()});
	if(indirect_) {
		vk::cmdDrawIndirect(cmdb, positionBuf_.buffer(),
			positionBuf_.offset(), 1, 0);
	} else {
		vk::cmdDraw(cmdb, drawCount_, 1, 0, 0);
	}
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

// Polygon
void Polygon::bakeStroke(const StrokeSettings& settings) {
	strokeWidth_ = settings.width;
	if(aaStroke_) {
		std::tie(strokeCache_, uvStrokeCache_) = bakeStrokeUv(points,
			settings.width, settings.cap, settings.join);
	} else {
		strokeCache_ = ::vgv::bakeStroke(points, settings.width,
			settings.cap, settings.join);
	}
}

bool Polygon::updateDevice(const Context& ctx, DrawMode mode) {
	bool rerecord = false;

	if(mode == DrawMode::fill || mode == DrawMode::fillStroke) {
		// TODO!
	}

	if(mode == DrawMode::stroke || mode == DrawMode::fillStroke) {
		auto ioff = indirect_ * sizeof(vk::DrawIndirectCommand);
		auto bsize = sizeof(Vec2f) * strokeCache_.size();
		auto neededSize = ioff + (1 + aaStroke_) * bsize;

		if(strokeBuf_.size() < neededSize) {
			neededSize *= 2;
			strokeBuf_ = ctx.device().bufferAllocator().alloc(true, neededSize,
				vk::BufferUsageBits::vertexBuffer);
			rerecord = true;
		}

		auto map = strokeBuf_.memoryMap();
		auto ptr = map.ptr();
		if(indirect_) {
			vk::DrawIndirectCommand cmd {};
			cmd.vertexCount = strokeCache_.size();
			cmd.instanceCount = 1;
			write(ptr, cmd);
		}

		std::memcpy(ptr, strokeCache_.data(),
			uvStrokeCache_.size() * sizeof(Vec2f));

		if(aaStroke_) {
			ptr += (strokeBuf_.size() - ioff) / 2;
			std::memcpy(ptr, uvStrokeCache_.data(),
				uvStrokeCache_.size() * sizeof(Vec2f));
		}
	}

	return rerecord;
}

void Polygon::stroke(const Context& ctx, vk::CommandBuffer cmdb) {
	if(points.empty()) {
		return;
	}

	dlg_assert(strokeBuf_.size());

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), 1, {ctx.dummyTex()}, {});

	auto ioff = indirect_ * sizeof(vk::DrawIndirectCommand);
	auto offset = strokeBuf_.offset() + ioff;
	auto offset2 = offset + aaStroke_ * (strokeBuf_.size() - ioff) / 2;

	auto sb = strokeBuf_.buffer().vkHandle();
	vk::cmdBindVertexBuffers(cmdb, 0, {sb, sb}, {offset, offset2});

	constexpr auto fringe = 5.f;
	float aa = aaStroke_ * (0.5 * strokeWidth_ + 0.5 * fringe) / fringe;
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, sizeof(float), &aa);

	if(indirect_) {
		vk::cmdDrawIndirect(cmdb, strokeBuf_.buffer(),
			strokeBuf_.offset(), 1, 0);
	} else {
		vk::cmdDraw(cmdb, strokeCache_.size(), 1, 0, 0);
	}
}

} // namespace vgv
