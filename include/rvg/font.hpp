// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/paint.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/image.hpp>

#include <string>
#include <string_view>

namespace rvg {

/// Holds a texture on the device to which multiple fonts can be uploaded.
class FontAtlas {
public:
	FontAtlas(Context&);
	~FontAtlas();

	bool bake(Context&);

	auto& nkAtlas() const { return *atlas_; }
	auto& ds() const { return ds_; }
	auto& texture() const { return texture_; }

protected:
	std::unique_ptr<nk_font_atlas> atlas_;
	vpp::TrDs ds_;
	Texture texture_;
};

/// Represents information about one font in a font atlas.
class Font {
public:
	Font(FontAtlas&, const char* file, unsigned height);
	Font(FontAtlas&, struct nk_font* font);

	float width(std::string_view text) const;
	float width(std::u32string_view text) const;
	float height() const;

	auto* nkFont() const { return font_; }
	auto& atlas() const { return *atlas_; }

protected:
	FontAtlas* atlas_;
	struct nk_font* font_;
};

} // namespace rvg
