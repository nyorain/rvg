// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/paint.hpp>
#include <rvg/deviceObject.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/image.hpp>
#include <nytl/stringParam.hpp>

#include <string>
#include <string_view>

namespace rvg {

/// Holds a texture on the device to which multiple fonts can be uploaded.
class FontAtlas : public DeviceObject, public nytl::NonMovable {
public:
	FontAtlas(Context&);
	~FontAtlas();

	auto& nkAtlas() const { return *atlas_; }
	auto& ds() const { return ds_; }
	auto& texture() const { return texture_; }

	// - usually not needed -
	bool updateDevice();
	void invalidate();
	void ensureBaked();
	void added(Text&);
	void removed(Text&);
	void moved(Text&, Text&) noexcept;

protected:
	std::unique_ptr<nk_font_atlas> atlas_;
	std::vector<Text*> texts_;
	// std::vector<Font*> fonts_;
	vpp::TrDs ds_;
	Texture texture_;
	bool invalid_;
};

/// Represents information about one font in a font atlas.
/// Lightweight discription, all actual data is stored in the font
/// atlas. Can be copied.
/// Creating new Fonts should usually not be done at runtime since
/// it will require the FontAtlas to be recreated which is
/// somewhat expensive. So the best way to go is to create all
/// needed fonts up front. If this is not possible, it is a good
/// idea to store dyanmically loaded fonts in one or more separate
/// FontAtlas objects.
class Font {
public:
	/// Loads the font from a given file.
	/// Throws on error (e.g. if the file does not exist).
	/// If no FontAtlas is given, uses the contexts default one.
	Font(Context&, StringParam file, unsigned height);
	Font(FontAtlas&, StringParam file, unsigned height);

	/// Loads the font from a otf/ttf blob.
	/// Throws on error.
	/// If no FontAtlas is given, uses the contexts default one.
	Font(Context&, Span<const std::byte> font, unsigned height);
	Font(FontAtlas&, Span<const std::byte> font, unsigned height);

	void addRange(uint32_t from, uint32_t to);

	float width(std::string_view text) const;
	float width(std::u32string_view text) const;
	float height() const;

	auto* nkFont() const { return font_; }
	auto& atlas() const { return *atlas_; }

	const nk_font_glyph& glyph(uint32_t utf32);
	const nk_font_glyph* findGlyph(uint32_t utf32) const;

protected:
	struct Range {
		uint32_t from;
		uint32_t to;
	};

	FontAtlas* atlas_;
	std::vector<Range> range_;
	nk_font* font_;
};

} // namespace rvg
