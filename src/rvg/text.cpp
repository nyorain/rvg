// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/util.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>
#include <vpp/vk.hpp>
#include <nytl/utf.hpp>
#include <rvg/nk_font/font.h>

namespace rvg {

constexpr auto vertIndex0 = 2;
constexpr auto vertIndex2 = 3;

Text::Text(Context& ctx, std::string_view xtext, const Font& f, Vec2f xpos) :
	Text(ctx, toUtf32(xtext), f, xpos) {
}

Text::Text(Context& ctx, std::u32string txt, const Font& f, Vec2f xpos) :
		DeviceObject(ctx), state_{std::move(txt), &f, xpos}, oldFont_(&f) {

	update();
	updateDevice();
}

void Text::update() {
	auto& font = state_.font;
	auto& text = state_.utf32;
	auto& position = state_.position;

	if(font != oldFont_) {
		if(&font->atlas() != &oldFont_->atlas()) {
			context().rerecord();
		}

		oldFont_ = font;
	}

	dlg_assert(font && font->nkFont());
	dlg_assert(posCache_.size() == uvCache_.size());

	posCache_.clear();
	uvCache_.clear();

	// good approximcation for usually-ascii
	posCache_.reserve(text.size());
	uvCache_.reserve(text.size());

	auto x = position.x;
	auto addVert = [&](const nk_font_glyph& glyph, unsigned i) {
		auto left = i == 0 || i == 3;
		auto top = i == 0 || i == 1;

		posCache_.push_back({
			x + (left ? glyph.x0 : glyph.x1),
			position.y + (top ? glyph.y0 : glyph.y1)});
		uvCache_.push_back({
			left ? glyph.u0 : glyph.u1,
			top ? glyph.v0 : glyph.v1});
	};

	for(auto c : text) {
		auto pglyph = nk_font_find_glyph(font->nkFont(), c);
		dlg_assert(pglyph);

		auto& glyph = *pglyph;

		// we render using a strip pipe. Those doubled points allow us to
		// jump to the next quad. Not less efficient than using a list pipe
		for(auto i : {1, 1, 0, 2, 3, 3}) {
			addVert(glyph, i);
		}

		x += glyph.xadvance;
	}

	context().registerUpdateDevice(this);
	dlg_assert(posCache_.size() == uvCache_.size());
}

bool Text::updateDevice() {
	bool rerecord = false;

	// now upload data to gpu
	dlg_assert(posCache_.size() == uvCache_.size());
	auto checkResize = [&](auto& buf, auto needed) {
		if(buf.size() == 0u || buf.size() < needed) {
			needed = 2 * needed + 1;
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
	}

	return rerecord;
}

void Text::draw(const DrawInstance& ini) const {
	dlg_assert(valid());

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), Context::fontBindSet,
		{state_.font->atlas().ds()}, {});

	static constexpr auto type = uint32_t(1);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &type);

	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto off = posBuf_.offset() + ioff;

	// use a dummy color buffer
	auto pBuf = posBuf_.buffer().vkHandle();
	auto uvBuf = uvBuf_.buffer().vkHandle();
	vk::cmdBindVertexBuffers(cmdb, 0, {pBuf, uvBuf, pBuf},
		{off, uvBuf_.offset(), off});
	vk::cmdDrawIndirect(cmdb, posBuf_.buffer(), posBuf_.offset(), 1, 0);
}

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

Rect2f Text::ithBounds(unsigned n) const {
	auto& text = utf32();
	auto& position = state_.position;

	if(posCache_.size() <= n * 6 || text.size() <= n) {
		throw std::out_of_range("Text::ithBounds");
	}

	auto start = posCache_[n * 6 + vertIndex0];
	auto end = posCache_[n * 6 + vertIndex2];

	auto pglyph = nk_font_find_glyph(state_.font->nkFont(), text[n]);
	dlg_assert(pglyph);
	auto r = Rect2f {start - position, {pglyph->xadvance, end.y - start.y}};

	return r;
}

void Text::State::utf8(std::string_view utf8) {
	utf32 = toUtf32(utf8);
}

std::string Text::State::utf8() const {
	return toUtf8(utf32);
}

float Text::width() const {
	if(utf32().empty()) {
		return 0.f;
	}

	auto first = ithBounds(0);
	auto last = ithBounds(utf32().length() - 1);
	return last.position.x + last.size.x - first.position.x;
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
