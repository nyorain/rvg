#include <vgv/vgv.hpp>
#include <vgv/path.hpp>

#include <vpp/vk.hpp>
#include <vpp/formats.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/queue.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>
#include <nytl/vecOps.hpp>
#include <nytl/utf.hpp>
#include <cstring>

#include <vgv/nk_font/font.h>

#include <shaders/fill.vert.h>
#include <shaders/fill.frag.h>

// TODO(performance): optionally create (static) Polygons in deviceLocal memory.
// TODO(performance): cache points vec in *Shape::update
// TODO: something like default font(atlas) in context instead of dummy
//   texture?

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
	samplerInfo.magFilter = vk::Filter::linear;
	samplerInfo.minFilter = vk::Filter::linear;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.25f;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::nearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.borderColor = vk::BorderColor::floatOpaqueWhite;
	texSampler_ = {dev, samplerInfo};

	samplerInfo.magFilter = vk::Filter::nearest;
	samplerInfo.minFilter = vk::Filter::nearest;
	fontSampler_ = {dev, samplerInfo};

	// layouts
	auto transformDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
	};

	auto paintDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::fragment),
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, -1, 1, &texSampler_.vkHandle()),
	};

	auto fontAtlasDSB = {
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, -1, 1, &fontSampler_.vkHandle()),
	};

	dsLayoutTransform_ = {dev, transformDSB};
	dsLayoutPaint_ = {dev, paintDSB};
	dsLayoutFontAtlas_ = {dev, fontAtlasDSB};

	pipeLayout_ = {dev, {
		dsLayoutTransform_,
		dsLayoutPaint_,
		dsLayoutFontAtlas_
	}, {{vk::ShaderStageBits::fragment, 0, 4}}};

	// pool
	vk::DescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = 100;
	poolSizes[0].type = vk::DescriptorType::uniformBuffer;

	poolSizes[1].descriptorCount = 20;
	poolSizes[1].type = vk::DescriptorType::combinedImageSampler;

	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo.maxSets = 100;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	dsPool_ = {dev, poolInfo};

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

	// vertex attribs: vec2 pos, vec2 uv, vec4u8 color
	std::array<vk::VertexInputAttributeDescription, 3> vertexAttribs = {};
	vertexAttribs[0].format = vk::Format::r32g32Sfloat;

	vertexAttribs[1].format = vk::Format::r32g32Sfloat;
	vertexAttribs[1].location = 1;
	vertexAttribs[1].binding = 1;

	// NOTE: we would naturally use r8g8b8a8Unorm here and then simply
	// forward 'out_color = in_color' in the vertex shader but it
	// seems the amd vulkan driver on windows has a bug there currently
	// so we use this as a temporary workaround.
	// e: It's probably not a driver bug, not sure what though. Works on
	// linux (amdgpu radv), since the workaround has the same issue
	// vertexAttribs[2].format = vk::Format::r32Uint;
	
	vertexAttribs[2].format = vk::Format::r8g8b8a8Unorm;
	// vertexAttribs[2].format = vk::Format::r8g8b8a8Uint;
	vertexAttribs[2].location = 2;
	vertexAttribs[2].binding = 2;

	// position and uv are in different buffers
	// this allows polygons that don't use any uv-coords to simply
	// reuse the position buffer which will result in better performance
	// (due to caching) and waste less memory
	std::array<vk::VertexInputBindingDescription, 3> vertexBindings = {};
	vertexBindings[0].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[0].stride = sizeof(float) * 2; // position
	vertexBindings[0].binding = 0;

	vertexBindings[1].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[1].stride = sizeof(float) * 2; // uv
	vertexBindings[1].binding = 1;

	vertexBindings[2].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[2].stride = sizeof(u8) * 4; // color
	vertexBindings[2].binding = 2;

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.pVertexAttributeDescriptions = vertexAttribs.data();
	vertexInfo.vertexAttributeDescriptionCount = vertexAttribs.size();
	vertexInfo.pVertexBindingDescriptions = vertexBindings.data();
	vertexInfo.vertexBindingDescriptionCount = vertexBindings.size();
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

	// stripPipe
	auto stripPipeInfo = fanPipeInfo;
	stripPipeInfo.flags = vk::PipelineCreateBits::derivative;
	stripPipeInfo.basePipelineIndex = 0;

	auto stripAssemblyInfo = assemblyInfo;
	stripAssemblyInfo.topology = vk::PrimitiveTopology::triangleStrip;
	stripPipeInfo.pInputAssemblyState = &stripAssemblyInfo;

	auto pipes = vk::createGraphicsPipelines(dev, {},
		{fanPipeInfo, stripPipeInfo});
	fanPipe_ = {dev, pipes[0]};
	stripPipe_ = {dev, pipes[1]};

	// dummies
	constexpr std::uint8_t bytes[] = {0xFF, 0xFF, 0xFF, 0xFF};
	emptyImage_ = createTexture(dev, 1, 1,
		reinterpret_cast<const std::byte*>(bytes),
		TextureType::rgba32);

	dummyTex_ = {dsLayoutFontAtlas_, dsPool_};
	vpp::DescriptorSetUpdate update(dummyTex_);
	auto layout = vk::ImageLayout::general;
	update.imageSampler({{{}, emptyImage_.vkImageView(), layout}});

	identityTransform_ = {*this};
	pointColorPaint_ = {*this, ::vgv::pointColorPaint()};
}

DrawInstance Context::record(vk::CommandBuffer cmdb) {
	DrawInstance ret { *this, cmdb };
	identityTransform_.bind(ret);
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		pipeLayout(), fontBindSet, {dummyTex_}, {});
	return ret;
}

// Polygon
void Polygon::update(Span<const Vec2f> points, const DrawMode& mode) {
	fillCache_.clear();
	fillColorCache_.clear();
	strokeCache_.clear();
	strokeColorCache_.clear();

	if(mode.fill) {
		fillCache_.insert(fillCache_.end(), points.begin(), points.end());
		if(mode.color.fill) {
			dlg_assert(mode.color.points.size() == points.size());
			fillColorCache_.insert(fillColorCache_.end(),
				mode.color.points.begin(), mode.color.points.end());
		}
	}

	if(mode.stroke > 0.f) {
		if(mode.color.stroke) {
			dlg_assert(mode.color.points.size() == points.size());
			bakeColoredStroke(points, mode.color.points, {mode.stroke},
				strokeCache_, strokeColorCache_);
		} else {
			bakeStroke(points, {mode.stroke}, strokeCache_);
		}
	}
}

// Buffer layouts (IDC = IndirectDrawCommand):
//
// -- start (align: 4)
// IDC
// -- pos: sizeof(IDC) 
// position data (vec2f)
// -- pos: align(IDC + 2/3.f * (buf.size - sizeof(IDC)), 4u)
// color data [if any] (vec4u8)
// -- end
//
// We use the buffer this way to avoid the need for rerecording just
// because the point count changes. The align before the color
// data is probably not needed by spec but it seems there is a bug
// on the amd windows driver that requires it (or i oversaw something
// in the spec).

bool Polygon::updateDevice(const Context& ctx, bool hide) {
	bool rerecord = false;
	auto upload = [&](auto& cache, auto& ccache, auto cstate, auto& buf) {
		auto neededSize = sizeof(vk::DrawIndirectCommand);
		neededSize += sizeof(cache[0]) * cache.size();

		if(!ccache.empty()) {
			neededSize = vpp::align(neededSize, 4u);
			neededSize += sizeof(ccache[0]) * ccache.size();
		}

		if(ccache.empty() == (state_ & cstate)) {
			state_ ^= cstate;
			rerecord = true;
		}

		if(buf.size() < neededSize) {
			neededSize *= 2;
			buf = ctx.device().bufferAllocator().alloc(true, neededSize,
				vk::BufferUsageBits::vertexBuffer, 4u);
			rerecord = true;
		}

		auto map = buf.memoryMap();
		auto ptr = map.ptr();

		vk::DrawIndirectCommand cmd {};
		cmd.vertexCount = !hide * cache.size();
		cmd.instanceCount = 1;
		write(ptr, cmd);

		std::memcpy(ptr, cache.data(), cache.size() * sizeof(cache[0]));

		if(!ccache.empty()) {
			auto off = 2/3.f * (buf.size() - sizeof(vk::DrawIndirectCommand));
			off += sizeof(vk::DrawIndirectCommand);
			auto uoff = vpp::align(vk::DeviceSize(off), 4u);
			ptr = map.ptr() + uoff;
			std::memcpy(ptr, ccache.data(), ccache.size() * sizeof(ccache[0]));
		}
	};

	upload(fillCache_, fillColorCache_, State::fillColor, fillBuf_);
	upload(strokeCache_, strokeColorCache_, State::strokeColor, strokeBuf_);
	return rerecord;
}

void Polygon::fill(const DrawInstance& ini) const {
	dlg_assert(fillBuf_.size() > 0);

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.fanPipe());

	static constexpr auto isText = uint32_t(0);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &isText);

	// position and dummy uv buffer
	auto& b = fillBuf_;
	auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
	vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer(), b.buffer()}, {off, off});

	if(state_ & State::fillColor) {
		auto o = 2/3.f * (b.size() - sizeof(vk::DrawIndirectCommand));
		o += sizeof(vk::DrawIndirectCommand);
		off = b.offset() + vpp::align(vk::DeviceSize(o), 4u);
	}

	// (potentially dummy) color buffer
	vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off});
	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);
}

void Polygon::stroke(const DrawInstance& ini) const {
	dlg_assert(strokeBuf_.size() > 0);

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());

	static constexpr auto isText = uint32_t(0);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &isText);

	// position and dummy uv buffer
	auto& b = strokeBuf_;
	auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
	vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer(), b.buffer()}, {off, off});

	if(state_ & State::strokeColor) {
		auto o = 2/3.f * (b.size() - sizeof(vk::DrawIndirectCommand));
		o += sizeof(vk::DrawIndirectCommand);
		off = b.offset() + vpp::align(vk::DeviceSize(o), 4u);
	}

	// (potentially dummy) color buffer
	vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off});
	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);
}

// Shape
Shape::Shape(const Context& ctx, std::vector<Vec2f> p, const DrawMode& d,
		bool h) : points(std::move(p)), draw(d), hide(h) {
	update();
	updateDevice(ctx);
}

void Shape::update() {
	polygon_.update(points, draw);
}

bool Shape::updateDevice(const Context& ctx) {
	return polygon_.updateDevice(ctx, hide);
}

void Shape::fill(const DrawInstance& di) const {
	return polygon_.fill(di);
}

void Shape::stroke(const DrawInstance& di) const {
	return polygon_.stroke(di);
}

// Rect
RectShape::RectShape(const Context& ctx, Vec2f p, Vec2f s,
		const DrawMode& d, bool h) : pos(p), size(s), draw(d), hide(h) {
	update();
	updateDevice(ctx);
}

void RectShape::update() {
	auto points = {
		pos,
		pos + Vec {size.x, 0.f},
		pos + size,
		pos + Vec {0.f, size.y},
		pos
	};
	polygon_.update(points, draw);
}

bool RectShape::updateDevice(const Context& ctx) {
	return polygon_.updateDevice(ctx, hide);
}

void RectShape::fill(const DrawInstance& di) const {
	return polygon_.fill(di);
}

void RectShape::stroke(const DrawInstance& di) const {
	return polygon_.stroke(di);
}

// CircleShape
CircleShape::CircleShape(const Context& ctx,
		Vec2f xcenter, Vec2f xradius, const DrawMode& xdraw,
		bool xhide, unsigned xpoints, float xstartAngle)
			: center(xcenter), radius(xradius), draw(xdraw), hide(xhide),
				points(xpoints), startAngle(xstartAngle) {

	update();
	updateDevice(ctx);
}

CircleShape::CircleShape(const Context& ctx,
		Vec2f xcenter, float xradius, const DrawMode& xdraw,
		bool xhide, unsigned xpoints, float xstartAngle)
			: CircleShape(ctx, xcenter, {xradius, xradius},
				xdraw, xhide, xpoints, xstartAngle) {
}

void CircleShape::update() {
	dlg_assertl(dlg_level_warn, points > 2);

	std::vector<Vec2f> pts;
	pts.reserve(points);

	auto a = startAngle;
	auto d = 2 * nytl::constants::pi / points;
	for(auto i = 0u; i < points + 1; ++i) {
		using namespace nytl::vec::cw::operators;
		pts.push_back(center + Vec {std::cos(a), std::sin(a)} * radius);
		a += d;
	}

	polygon_.update(pts, draw);
}

bool CircleShape::updateDevice(const Context& ctx) {
	return polygon_.updateDevice(ctx, hide);
}

void CircleShape::fill(const DrawInstance& di) const {
	return polygon_.fill(di);
}

void CircleShape::stroke(const DrawInstance& di) const {
	return polygon_.stroke(di);
}

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) {
	atlas_ = std::make_unique<nk_font_atlas>();
	nk_font_atlas_init_default(&nkAtlas());
	ds_ = {ctx.dsLayoutFontAtlas(), ctx.dsPool()};
}

FontAtlas::~FontAtlas() {
	nk_font_atlas_cleanup(&nkAtlas());
}

bool FontAtlas::bake(Context& ctx) {
	// TODO: use r8 format. Figure out why it did not work
	constexpr auto format = vk::Format::r8Uint;
	bool rerecord = false;

	int w, h;
	auto data = reinterpret_cast<const std::byte*>(
		nk_font_atlas_bake(&nkAtlas(), &w, &h, NK_FONT_ATLAS_ALPHA8));

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
			vgv::TextureType::a8);
		rerecord = true;

		vpp::DescriptorSetUpdate update(ds_);
		update.imageSampler({{{}, texture_.vkImageView(),
			vk::ImageLayout::general}});
	} else {
		auto& qs = ctx.device().queueSubmitter();
		auto cmdBuf = ctx.device().commandAllocator().get(qs.queue().family());
		vk::beginCommandBuffer(cmdBuf, {});
		auto buf = vpp::fillStaging(cmdBuf, texture_.image(), format,
			vk::ImageLayout::general, {uw, uh, 1}, {data, uw * uh},
			{vk::ImageAspectBits::color});
		vk::endCommandBuffer(cmdBuf);

		// TODO: performance! Return the work/wait for submission
		qs.wait(qs.add({{}, {}, {}, 1, &cmdBuf.vkHandle(), {}, {}}));
	}

	return rerecord;
}

// Font
Font::Font(FontAtlas& atlas, const char* file, unsigned h) : atlas_(&atlas) {
	font_ = nk_font_atlas_add_from_file(&atlas.nkAtlas(), file, h, nullptr);
	if(!font_) {
		std::string err = "Could not load font ";
		err.append(file);
		throw std::runtime_error(err);
	}
}

Font::Font(FontAtlas& atlas, struct nk_font* font) :
	atlas_(&atlas), font_(font) {
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
constexpr auto vertIndex0 = 2;
constexpr auto vertIndex2 = 3;

Text::Text(const Context& ctx, std::string xtext, const Font& f, Vec2f xpos) :
	Text(ctx, toUtf32(xtext), f, xpos) {
}

Text::Text(const Context& ctx, std::u32string txt, const Font& f, Vec2f xpos) :
		text(std::move(txt)), font(&f), pos(xpos) {
	update();
	updateDevice(ctx);
}
void Text::update() {
	// TODO: should signal rerecord when font was changed to a font
	// in a different atlas. Rework font (atlas) binding.
	// Or (alternatively) make Font private and proviate an update overload
	// taking a Font&

	dlg_assert(font && font->nkFont());
	dlg_assert(posCache_.size() == uvCache_.size());

	posCache_.clear();
	uvCache_.clear();

	// good approximcation for usually-ascii
	posCache_.reserve(text.size());
	uvCache_.reserve(text.size());

	auto x = pos.x;
	auto addVert = [&](const nk_font_glyph& glyph, unsigned i) {
		auto left = i == 0 || i == 3;
		auto top = i == 0 || i == 1;

		posCache_.push_back({
			x + (left ? glyph.x0 : glyph.x1),
			pos.y + (top ? glyph.y0 : glyph.y1)});
		uvCache_.push_back({
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

		// we render using a strip pipe. Those doubled points allow us to
		// jump to the next quad. Not less efficient than using a list pipe
		for(auto i : {1, 1, 0, 2, 3, 3}) {
			addVert(glyph, i);
		}

		x += glyph.xadvance;
	}

	dlg_assert(posCache_.size() == uvCache_.size());
}

bool Text::updateDevice(const Context& ctx) {
	bool rerecord = false;

	// now upload data to gpu
	dlg_assert(posCache_.size() == uvCache_.size());
	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto byteSize = sizeof(Vec2f) * posCache_.size();
	auto neededSize = ioff + 2 * byteSize;

	if(buf_.size() < neededSize) {
		neededSize *= 2;
		buf_ = ctx.device().bufferAllocator().alloc(true,
			neededSize, vk::BufferUsageBits::vertexBuffer);
		rerecord = true;
	}

	auto map = buf_.memoryMap();
	auto ptr = map.ptr();

	// the positionBuf contains the indirect draw command, if there is any
	vk::DrawIndirectCommand cmd {};
	cmd.vertexCount = posCache_.size();
	cmd.instanceCount = 1;
	write(ptr, cmd);

	std::memcpy(ptr, posCache_.data(), byteSize);
	ptr = ptr + (buf_.size() - ioff) / 2;
	std::memcpy(ptr, uvCache_.data(), byteSize);

	return rerecord;
}

void Text::draw(const DrawInstance& ini) const {
	if(posCache_.empty()) {
		return;
	}

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), fontBindSet, {font->atlas().ds()}, {});

	static constexpr auto isText = uint32_t(1);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &isText);

	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto off = buf_.offset() + ioff;

	// dummy color buffer
	auto vkbuf = buf_.buffer().vkHandle();
	vk::cmdBindVertexBuffers(cmdb, 0, {vkbuf, vkbuf, vkbuf},
		{off, off + (buf_.size() - ioff) / 2, off});
	vk::cmdDrawIndirect(cmdb, buf_.buffer(), buf_.offset(), 1, 0);
}

Text::CharAt Text::charAt(float x) const {
	auto lastEnd = posCache_.empty() ? -1.f : posCache_[vertIndex0].x;
	x += pos.x;
	for(auto i = 0u; i < posCache_.size(); i += 6) {
		auto start = posCache_[i + vertIndex0].x;
		auto end = posCache_[i + vertIndex2].x;
		dlg_assert(end >= start);

		if(x < start) {
			auto nearest = (i == 0) ? posCache_[vertIndex0].x - pos.x : lastEnd;
			return {i / 6, -1.f, nearest};
		}

		if(x < end) {
			auto fac = (end - start) / (x - start);
			auto nearest = (fac > 0.5f ? end - pos.x : lastEnd);
			return {i / 6 + 1, fac, nearest};
		}

		lastEnd = end - pos.x;
	}

	return {unsigned(posCache_.size() / 6), -1.f, lastEnd};
}

Rect2f Text::ithBounds(unsigned n) const {
	if(posCache_.size() <= n * 6) {
		throw std::out_of_range("Text::ithBounds");
	}

	auto start = posCache_[n * 6 + vertIndex0];
	return {start - pos, posCache_[n * 6 + vertIndex2] - start};
}

// Transform
constexpr auto transformUboSize = sizeof(Mat4f);
Transform::Transform(Context& ctx) : Transform(ctx, identity<4, float>()) {
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

void Transform::bind(const DrawInstance& di) {
	dlg_assert(ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), transformBindSet, {ds_}, {});
}

} // namespace vgv
