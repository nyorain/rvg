// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/font.hpp>
#include <rvg/context.hpp>
#include <rvg/text.hpp>
#include <vpp/vk.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <dlg/dlg.hpp>
#include <nytl/utf.hpp>

#define FONTSTASH_IMPLEMENTATION
#include <rvg/fontstash.h>

// TODO: currently fonts cannot be removed from a font atlas.

namespace rvg {

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) : DeviceObject(ctx) {
	FONSparams params {};
	params.flags = FONS_ZERO_TOPLEFT;
	params.width = 512;
	params.height = 512;

	ctx_ = fonsCreateInternal(&params);
	fonsSetAlign(ctx_, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
	ds_ = {ctx.dsAllocator(), ctx.dsLayoutFontAtlas()};
}

FontAtlas::~FontAtlas() {
	fonsDeleteInternal(ctx_);
}

void FontAtlas::validate() {
	int dirty[4];
	if(fonsValidateTexture(ctx_, dirty)) {
		// TODO: use dirty rect, don't update all in updateDevice
		context().registerUpdateDevice(this);
	}
}

void FontAtlas::expand() {
	constexpr auto maxSize = 4096;

	int w, h;
	fonsGetAtlasSize(ctx_, &w, &h);

	w = std::min(maxSize, w * 2);
	h = std::min(maxSize, h * 2);

	// TODO: instead of using reset and updating all texts we
	// could use ExpandAtlas. Try it and see if the result is much worse
	// (since rectpacking cannot be done again)
	fonsResetAtlas(ctx_, w, h);
	for(auto& t : texts_) {
		dlg_assert(t);
		t->update();
	}

	context().registerUpdateDevice(this);
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

	int w, h;
	auto data = fonsGetTextureData(ctx_, &w, &h);
	dlg_assert(w > 0 && h > 0);

	auto fs = Vec2ui {};
	fs.x = w;
	fs.y = h;

	auto dptr = reinterpret_cast<const std::byte*>(data);
	auto dsize = fs.x * fs.y;
	if(fs != texture_.size()) {
		texture_ = {ctx, fs, {dptr, dsize}, rvg::TextureType::a8};
		rerecord = true;

		vpp::DescriptorSetUpdate update(ds_);
		update.imageSampler({{{}, texture_.vkImageView(),
			vk::ImageLayout::shaderReadOnlyOptimal}});
	} else {
		// NOTE: important to not call texte_.update here since
		//   then the texture would register for updateDevice.
		//   But we currentlly are in the updateDevice phase, alling
		//   context registerUpdateDevice during that is not allowed
		// TODO: use dirty rect
		rerecord |= texture_.updateDevice({dptr, dptr + dsize});
	}

	return rerecord;
}

nytl::Span<std::byte> FontAtlas::addBlob(std::vector<std::byte> blob) {
	blobs_.emplace_back(std::move(blob));
	return blobs_.back();
}

// Font
Font::Font(Context& ctx, StringParam f) :
	Font(ctx.defaultAtlas(), f) {
}

Font::Font(FontAtlas& atlas, StringParam f) : atlas_(&atlas) {
	id_ = fonsAddFont(atlas.stash(), "", f.c_str());
	if(id_ == FONS_INVALID) {
		std::string err = "Could not load font ";
		err.append(f);
		throw std::runtime_error(err);
	}
}

Font::Font(FontAtlas& atlas, std::vector<std::byte> blob) : atlas_(&atlas) {
	auto data = atlas.addBlob(std::move(blob));
	auto ptr = reinterpret_cast<unsigned char*>(data.data());
	id_ = fonsAddFontMem(atlas.stash(), "", ptr, data.size(), 0);
	if(id_ == FONS_INVALID) {
		throw std::runtime_error("Font: Invalid blob given");
	}
}

Font::Font(Context& ctx, std::vector<std::byte> blob) :
	Font(ctx.defaultAtlas(), std::move(blob)) {
}

nytl::Rect2f Font::bounds(std::string_view text, unsigned height) const {
	dlg_assert(id_ != FONS_INVALID);
	float bounds[4];

	fonsSetFont(atlas().stash(), id_);
	fonsSetSize(atlas().stash(), height);
	fonsTextBounds(atlas().stash(), 0, 0, text.begin(), text.end(), bounds);
	return {{bounds[0], bounds[1]}, {bounds[2] - bounds[0], bounds[3] - bounds[1]}};
}

float Font::width(std::string_view text, unsigned height) const {
	return bounds(text, height).size.x;
}

void Font::fallback(const Font& f) {
	dlg_assert(id_ != FONS_INVALID && f.id() != FONS_INVALID);
	dlg_assert(&atlas() == &f.atlas());
	fonsAddFallbackFont(atlas().stash(), id_, f.id());
}

Font::Metrics Font::metrics() const {
	Metrics metrics;
	fonsSetFont(atlas().stash(), id_);
	fonsVertMetrics(atlas().stash(), &metrics.ascender,
		&metrics.descender, &metrics.lineHeight);
	return metrics;
}

} // namespac rvg
