// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/font.hpp>
#include <rvg/context.hpp>
#include <rvg/text.hpp>
#include <rvg/nk_font/font.h>
#include <vpp/vk.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <dlg/dlg.hpp>

// TODO: currently fonts cannot be removed from a font atlas.
// NOTE: the implementation can be improved since nk_font makes
// a few assumptions/was designed for cases that don't fully
// fit rvg. Ideally, we probably would want to maintain all glyph
// positions when a font atlas is rebaked. This means the glyphs then
// might not be 100% optimally layed out but we would not have to refresh
// (and previously store) all Text objects in Font.

namespace rvg {

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) : DeviceObject(ctx) {
	atlas_ = std::make_unique<nk_font_atlas>();
	nk_font_atlas_init_default(&nkAtlas());
	ds_ = {ctx.dsAllocator(), ctx.dsLayoutFontAtlas()};
}

FontAtlas::~FontAtlas() {
	nk_font_atlas_clear(&nkAtlas());
}

void FontAtlas::invalidate() {
	dlg_assert(valid());
	invalid_ = true;
}

void FontAtlas::ensureBaked() {
	if(!invalid_) {
		return;
	}

	int w, h;
	nk_font_atlas_begin(&nkAtlas());
	nk_font_atlas_bake(&nkAtlas(), &w, &h, NK_FONT_ATLAS_ALPHA8);

	context().registerUpdateDevice(this);
	invalid_ = false;
	for(auto& t : texts_) { // texts have to be rebaked (uv coords)
		dlg_assert(t);
		t->update();
	}
}

void FontAtlas::added(Text& t) {
	dlg_assert(std::find(texts_.begin(), texts_.end(), &t) == texts_.end());
	texts_.push_back(&t);
}

void FontAtlas::removed(Text& t) {
	auto it = std::find(texts_.begin(), texts_.end(), &t);
	dlg_assert(it != texts_.end());
	texts_.erase(it);
}

void FontAtlas::moved(Text& t, Text& tn) noexcept {
	auto it = std::find(texts_.begin(), texts_.end(), &t);
	dlg_assert(it != texts_.end());
	*it = &tn;
}

bool FontAtlas::updateDevice() {
	dlg_assert(valid());
	auto& ctx = context();
	bool rerecord = false;

	auto data = reinterpret_cast<const std::byte*>(atlas_->pixel);
	unsigned uw = atlas_->tex_width;
	unsigned uh = atlas_->tex_height;

	if(uw == 0 || uh == 0) {
		dlg_info("FontAtlas::updateDevice on empty atlas");
		return false;
	}

	dlg_assert(uw > 0 && uh > 0);
	auto dptr = reinterpret_cast<const std::byte*>(data);
	if(Vec2ui{uw, uh} != texture_.size()) {
		texture_ = {ctx, {uw, uh}, {dptr, uw * uh}, rvg::TextureType::a8};
		rerecord = true;

		vpp::DescriptorSetUpdate update(ds_);
		update.imageSampler({{{}, texture_.vkImageView(),
			vk::ImageLayout::general}});
	} else {
		texture_.update({dptr, dptr + uw * uh});
	}

	nk_font_atlas_end(&nkAtlas(), {}, nullptr);
	return rerecord;
}

// Font
Font::Font(Context& ctx, StringParam f, unsigned h) :
	Font(ctx.defaultAtlas(), f, h) {
}

Font::Font(FontAtlas& atlas, StringParam f, unsigned h) : atlas_(&atlas) {
	auto config = nk_font_config(h);
	auto data = reinterpret_cast<uint32_t*>(range_.data());
	config.range = data;
	font_ = nk_font_atlas_add_from_file(&atlas.nkAtlas(), f.c_str(), h,
		&config);
	if(!font_) {
		std::string err = "Could not load font ";
		err.append(f);
		throw std::runtime_error(err);
	}

	atlas.invalidate();
}

Font::Font(FontAtlas& atlas, Span<const std::byte> blob, unsigned h) :
		atlas_(&atlas) {

	auto config = nk_font_config(h);
	auto data = reinterpret_cast<uint32_t*>(range_.data());
	config.range = data;
	font_ = nk_font_atlas_add_from_memory(&atlas.nkAtlas(),
		const_cast<std::byte*>(blob.data()), blob.size(), h, &config);
	if(!font_) {
		auto err = "Failed to create font from memory";
		throw std::runtime_error(err);
	}

	atlas.invalidate();
}

Font::Font(Context& ctx, Span<const std::byte> blob, unsigned h) :
	Font(ctx.defaultAtlas(), blob, h) {
}

Font::Font(Context& ctx, Span<const Description> fonts) :
	Font(ctx.defaultAtlas(), fonts) {
}

Font::Font(FontAtlas& atlas, Span<const Description> fonts) : atlas_(&atlas) {
	// TODO: leave in valid state when throwing. Don't destroy font atlas
	if(fonts.empty()) {
		throw std::runtime_error("Font: empty font span given");
	}

	auto first = true;
	for(auto& f : fonts) {
		auto config = nk_font_config(f.height);
		auto data = reinterpret_cast<uint32_t*>(range_.data());
		config.range = data;
		config.merge_mode = !first;

		std::string err = "Failed to create font from ";
		struct nk_font* ff = nullptr;
		if(f.from.index() == 0) {
			auto file = std::get<0>(f.from);
			ff = nk_font_atlas_add_from_file(&atlas.nkAtlas(), file.c_str(),
				f.height, &config);
			err += "file '";
			err += file;
			err += "'";
		} else {
			auto blob = std::get<1>(f.from);
			ff = nk_font_atlas_add_from_memory(&atlas.nkAtlas(),
				const_cast<std::byte*>(blob.data()), blob.size(), f.height,
				&config);
			err += "given binary data";
		}

		if(!ff) {
			throw std::runtime_error(err);
		}

		if(first) {
			font_ = ff;
			first = false;
		}
	}

	atlas.invalidate();
}

float Font::width(std::string_view text) const {
	dlg_assert(font_);
	auto& handle = font_->handle;
	return handle.width(handle.userdata, handle.height, text.data(),
		text.size());
}

float Font::width(std::u32string_view text) const {
	dlg_assert(font_);
	float w = 0.f;
	for(auto c : text) {
		auto g = nk_font_find_glyph(font_, c);
		dlg_assert(g);
		w += g->xadvance;
	}

	return w;
}

float Font::height() const {
	dlg_assert(font_);
	return font_->handle.height;
}

bool Font::addRange(uint32_t from, uint32_t to) {
	if(from == 0) {
		if(to == 0) {
			dlg_warn("Font::addRange: called with 0, 0");
			return false;
		}

		// 0 is invalid
		from = 1;
	}

	// range is ordered
	auto inserted = false;
	auto added = false;
	for(auto i = 0u; i < range_.size() - 1; ++i) {
		auto& r = range_[i];
		if(r.to + 1 < from) { // when (r.to + 1 == from) we can merge them
			continue;
		}

		// we know (from <= r.to + 1)
		if(from < r.from && to + 1 < r.from) { // insert completely new range
			range_.insert(range_.begin() + i, {to, from});
			added = true;
		} else if(from < r.from || to > r.to) {
			r.from = std::min(from, r.from);;
			r.to = std::max(to, r.to);
			added = true;

			// fix overlapping ranges
			auto* next = (i < range_.size() - 2) ? &range_[i + 1] : nullptr;
			while(next && next->from <= r.to) {
				r.to = std::max(next->to, to);
				range_.erase(range_.begin() + i + 1);
				next = (i < range_.size() - 2) ? &range_[i + 1] : nullptr;
			}
		} // else: (from >= r.from && to <= r.to), range already included

		inserted = true;
		break;
	}

	// add new range
	if(!inserted) {
		range_.back() = {from, to};
		range_.emplace_back();
		added = true;
	}

	if(added) {
		dlg_trace("added: {} {}", from, to);
	}

	return added;
}

bool Font::ensureRange(std::u32string_view text) {
	auto ret = false;
	for(auto c : text) {
		ret |= addRange(c, c);
	}

	if(ret) {
		updateRanges();
	}

	return ret;
}

bool Font::ensureRange(uint32_t from, uint32_t to) {
	auto ret = addRange(from, to);
	if(ret) {
		updateRanges();
	}

	return ret;
}

void Font::updateRanges() {
	auto data = reinterpret_cast<uint32_t*>(range_.data());
	for(auto c = nkFont()->config;; c = c->n) {
		c->range = data;
		if(c->n == nkFont()->config) {
			break;
		}
	}

	atlas().invalidate();
}

const nk_font_glyph& Font::glyph(uint32_t utf32) {
	auto g = findGlyph(utf32);
	if((g && g != nkFont()->fallback) || utf32 == 0) {
		return *g;
	}

	dlg_info("Rebaking font atlas for glyph {}", utf32);
	if(!addRange(utf32, utf32) && g) {
		return *g;
	}

	g = findGlyph(utf32);
	dlg_assert(g);
	return *g;
}

const nk_font_glyph* Font::findGlyph(uint32_t utf32) const {
	atlas().ensureBaked();
	return nk_font_find_glyph(nkFont(), utf32);
}

} // namespac rvg
