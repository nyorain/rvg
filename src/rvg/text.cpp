// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/util.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>
#include <vpp/vk.hpp>
#include <nytl/utf.hpp>
#include <rvg/fontstash.h>

namespace rvg {

// utility
namespace {

constexpr auto vertIndex0 = 2; // vertex index on the left
constexpr auto vertIndex2 = 3; // vertex index on the right

float quantatize(float value, float quantum) {
	return std::round(value / quantum) * quantum;
}

} // anon namespace

// Text
Text::Text(Context& ctx, Vec2f p, std::string t, Font& f, float h) :
		DeviceObject(ctx), state_{std::move(t), f, p, h} {

	f.atlas().added(*this);
	update();
	updateDevice();
}

// NOTE: remove them if possible (currently only here due to the
// font atlas referencing)
Text::Text(Text&& rhs) noexcept : DeviceObject(std::move(rhs)) {
	state_ = std::move(rhs.state_);
	deviceLocal_ = rhs.deviceLocal_;
	disable_ = rhs.disable_;
	posCache_ = std::move(rhs.posCache_);
	uvCache_ = std::move(rhs.uvCache_);
	posBuf_ = std::move(rhs.posBuf_);
	uvBuf_ = std::move(rhs.uvBuf_);
	oldAtlas_  = rhs.oldAtlas_;

	if(valid()) {
		font().atlas().moved(rhs, *this);
	}
}

Text& Text::operator=(Text&& rhs) noexcept {
	DeviceObject::operator=(std::move(rhs));
	state_ = std::move(rhs.state_);
	deviceLocal_ = rhs.deviceLocal_;
	disable_ = rhs.disable_;
	posCache_ = std::move(rhs.posCache_);
	uvCache_ = std::move(rhs.uvCache_);
	posBuf_ = std::move(rhs.posBuf_);
	uvBuf_ = std::move(rhs.uvBuf_);
	oldAtlas_  = rhs.oldAtlas_;

	if(valid()) {
		font().atlas().moved(rhs, *this);
	}

	return *this;
}

Text::~Text() {
	if(valid()) {
		dlg_assert(font().valid());
		font().atlas().removed(*this);
	}
}

void Text::update() {
	constexpr auto quantum = 0.5f;
	dlg_assert(valid() && font().valid());
	auto& font = state_.font;
	auto& text = state_.text;
	auto pos = state_.position;
	auto fscale = scale(state_.transform);
	auto fsize = quantatize(std::abs(state_.height) * fscale, quantum);
	dlg_assert(fsize > 0.f);
	dlg_assert(font.valid());

	if(state_.height < 0.f) {
		pos.y *= -1.f;
	}

	if(oldAtlas_ && &font.atlas() != oldAtlas_) {
		context().rerecord();
		oldAtlas_->removed(*this);
		font.atlas().added(*this);
		oldAtlas_ = &font.atlas();
	}

	posCache_.clear();
	uvCache_.clear();

	// good approximation for usually-ascii
	posCache_.reserve(text.size());
	uvCache_.reserve(text.size());

	auto addVert = [&](const FONSquad& q, unsigned i) {
		auto left = i == 0 || i == 3;
		auto top = i == 0 || i == 1;

		auto p = Vec2f{
			left ? q.x0 : q.x1,
			top ? q.y0 : q.y1};
		p *= 1 / fscale;
		if(state_.height < 0.f) {
			p.y *= -1.f;
		}
		p = multPos(state_.transform, p);
		posCache_.push_back(p);

		uvCache_.push_back({
			left ? q.s0 : q.s1,
			top ? q.t0 : q.t1});
	};

	auto* stash = font.atlas().stash();
	FONSquad q;
	FONStextIter iter;

	fonsSetSize(stash, fsize);
	fonsSetFont(stash, font.id());

	fonsTextIterInit(stash, &iter, fscale * pos.x, fscale * pos.y, text.data(),
		text.data() + text.size(), FONS_GLYPH_BITMAP_REQUIRED);

	auto prev = iter;
	while(fonsTextIterNext(stash, &iter, &q)) {
		if(iter.prevGlyphIndex == -1) {
			// this will recreate the font atlas, i.e. all work we have
			// done iterating up to this point is invalid.
			// Will automatically update all texts (this object as well)
			// so we can return afterwards.
			font.atlas().expand();
			return;
		}

		// we render using a strip pipe. Those doubled points allow us to
		// jump to the next quad. Not less efficient than using a list pipe
		for(auto i : {1, 1, 0, 2, 3, 3}) {
			addVert(q, i);
		}

		prev = iter;
	}

	font.atlas().validate();
	context().registerUpdateDevice(this);
	dlg_assert(posCache_.size() == uvCache_.size());
}

bool Text::updateDevice() {
	bool rerecord = false;

	// now upload data to gpu
	dlg_assert(posCache_.size() == uvCache_.size());
	auto checkResize = [&](auto& buf, auto needed) {
		if(buf.size() == 0u || buf.size() < needed) {
			needed = std::max<vk::DeviceSize>(2u * needed, 32u);
			auto usage = nytl::Flags{vk::BufferUsageBits::vertexBuffer};
			if(deviceLocal_) {
				usage |= vk::BufferUsageBits::transferDst;
			}

			auto memBits = deviceLocal_ ?
				context().device().deviceMemoryTypes() :
				context().device().hostMemoryTypes();
			buf = {context().bufferAllocator(), needed, usage, 4u, memBits};
			rerecord = true;
		}
	};

	auto posCacheSize = sizeof(Vec2f) * posCache_.size();
	checkResize(posBuf_, sizeof(vk::DrawIndirectCommand) + posCacheSize);
	checkResize(uvBuf_, sizeof(Vec2f) * uvCache_.size());

	// positionBuf contains the indirect draw command
	vk::DrawIndirectCommand cmd {};
	cmd.vertexCount = !disable_ * posCache_.size();
	cmd.instanceCount = 1;

	// upload140(*this, posBuf_, vpp::raw(cmd), vpp::raw(posCache_));
	upload140(*this, posBuf_, vpp::raw(cmd), vpp::raw(*posCache_.data(),
		posCache_.size()));

	if(!uvCache_.empty()) {
		// upload140(*this, uvBuf_, vpp::raw(uvCache_));
		upload140(*this, uvBuf_, vpp::raw(*uvCache_.data(),
			uvCache_.size()));
	} else {
		// write something for validation layers
		upload140(*this, uvBuf_, vpp::raw(cmd));
	}

	return rerecord;
}

void Text::draw(vk::CommandBuffer cb) const {
	dlg_assert(valid() && font().valid());

	vk::cmdBindPipeline(cb, vk::PipelineBindPoint::graphics,
		context().stripPipe());
	vk::cmdBindDescriptorSets(cb, vk::PipelineBindPoint::graphics,
		context().pipeLayout(), Context::fontBindSet,
		{font().atlas().ds()}, {});

	static constexpr auto type = uint32_t(1);
	vk::cmdPushConstants(cb, context().pipeLayout(),
		vk::ShaderStageBits::fragment, 0, 4, &type);

	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto off = posBuf_.offset() + ioff;

	// use a dummy color buffer
	auto pBuf = posBuf_.buffer().vkHandle();
	auto uvBuf = uvBuf_.buffer().vkHandle();
	vk::cmdBindVertexBuffers(cb, 0, {pBuf, uvBuf, pBuf},
		{off, uvBuf_.offset(), off});
	vk::cmdDrawIndirect(cb, posBuf_.buffer(), posBuf_.offset(), 1, 0);
}

// TODO: the given x is in logical space but posCache_ is in
// (pre-)transformed state at the moment.
unsigned Text::charAt(float x) const {
	x += state_.position.x;
	for(auto i = 0u; i < posCache_.size(); i += 6) {
		auto end = posCache_[i + vertIndex2].x;
		if(x < end) {
			return i / 6;
		}
	}

	return unsigned(posCache_.size() / 6);
}

// NOTE: although it would be desirable, we cannot derive the information of
// bounds from the stored position cache (we don't have advance/overlaps).
// Not 100% sure, there might be a way (with additional assumptions).
// We could cache this information in the update function but it's probably
// not worth it.
Rect2f Text::bounds() const {
	dlg_assert(valid() && state_.font.valid());
	return font().bounds(state_.text, state_.height);
}

Rect2f Text::ithBounds(unsigned n) const {
	dlg_assert(valid() && state_.font.valid());

	auto* stash = font().atlas().stash();
	FONSquad q;
	FONStextIter iter;

	fonsSetSize(stash, state_.height);
	fonsSetFont(stash, font().id());
	fonsTextIterInit(stash, &iter, position().x, position().y, text().data(),
		text().data() + text().size(), FONS_GLYPH_BITMAP_REQUIRED);

	while(fonsTextIterNext(stash, &iter, &q)) {
		dlg_assert(iter.prevGlyphIndex != -1);
		if(n == 0) {
			auto x = std::min(iter.x, q.x0);
			auto y = std::min(iter.y, q.y0);
			auto sx = std::max(iter.nextx, q.x1) - x;
			auto sy = std::max(iter.nexty, q.y1) - y;
			return {{x, y}, {sx, sy}};
		}
		--n;
	}

	dlg_warn("Invalid char given in Text::ithBounds");
	return {};
}

float Text::width() const {
	return bounds().size.x;
}

bool Text::disable(bool disable) {
	auto ret = disable_;
	disable_ = disable;
	context().registerUpdateDevice(this);
	return ret;
}

void Text::deviceLocal(bool set) {
	if(deviceLocal_ != set) {
		deviceLocal_ = set;

		if(posBuf_.size()) {
			auto needed = deviceLocal_ ?
				context().device().deviceMemoryTypes() :
				context().device().hostMemoryTypes();
			auto ptype = posBuf_.buffer().memoryEntry().memory()->type();
			auto uvtype = uvBuf_.buffer().memoryEntry().memory()->type();
			bool ud = false;
			if(!(needed & ptype)) {
				posBuf_ = {};
				ud = true;
			}

			if(!(needed & uvtype)) {
				uvBuf_ = {};
				ud = true;
			}

			if(ud) {
				updateDevice();
				context().rerecord();
			}
		}
	}
}

} // namespace rvg
