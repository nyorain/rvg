// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/font.hpp>
#include <rvg/context.hpp>
#include <rvg/nk_font/font.h>
#include <vpp/vk.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <dlg/dlg.hpp>

namespace rvg {

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) {
	atlas_ = std::make_unique<nk_font_atlas>();
	nk_font_atlas_init_default(&nkAtlas());
	ds_ = {ctx.dsAllocator(), ctx.dsLayoutFontAtlas()};
}

FontAtlas::~FontAtlas() {
	nk_font_atlas_clear(&nkAtlas());
}

bool FontAtlas::bake(Context& ctx) {
	bool rerecord = false;

	int w, h;
	nk_font_atlas_begin(&nkAtlas());
	auto data = reinterpret_cast<const std::byte*>(
		nk_font_atlas_bake(&nkAtlas(), &w, &h, NK_FONT_ATLAS_ALPHA8));

	if(w == 0 || h == 0) {
		dlg_info("FontAtlas::bake on empty atlas");
		return false;
	}

	dlg_assert(w > 0 && h > 0);
	auto uw = unsigned(w), uh = unsigned(h);
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

} // namespac rvg
