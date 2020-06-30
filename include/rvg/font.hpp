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
#include <nytl/rect.hpp>

#include <string>
#include <string_view>
#include <variant>

struct FONScontext;

namespace rvg {

/// Holds a texture on the device to which multiple fonts can be uploaded.
/// See the Font class for FontAtlas restrictions and grouping.
class FontAtlas : public DeviceObject, public nytl::NonMovable {
public:
	FontAtlas(Context&);
	~FontAtlas();

	auto& ds() const { return ds_; }
	auto& texture() const { return texture_; }
	auto* stash() const { return ctx_; }

	// - usually not needed manually -
	bool updateDevice();
	void validate();
	void expand();
	nytl::Span<std::byte> addBlob(std::vector<std::byte>);

	void added(Text&);
	void removed(Text&);
	void moved(Text&, Text&) noexcept;

protected:
	FONScontext* ctx_;
	std::vector<Text*> texts_;
	vpp::TrDs ds_;
	Texture texture_;
	bool invalid_ {};
	std::vector<std::vector<std::byte>> blobs_;
};

// TODO: we should probably rather have something like
// struct FontRef {
// 	FontAtlas* atlas;
// 	int id;
// };
// And then various free functions for creating a font.
// Currently, it looks like Font is the object that contains
// everything about the font...

/// Represents information about one font in a font atlas.
/// Lightweight discription, all actual data is stored in the font
/// atlas. Therefore can be copied without overhead.
/// Fonts cannot be removed from a font atlas again; in this case the
/// whole font atlas has to be destroyed. One can only add fallbacks
/// fonts from the same font atlas. The more fonts are in a font atlas,
/// the slower an update is (e.g. a newly added glyph).
/// Therefore fonts should be grouped efficiently together in font atlases.
class Font {
public:
	struct Description {
		std::variant<StringParam, Span<const std::byte>> from;
		unsigned height {12};
	};

	struct Metrics {
		float lineHeight;
		float ascender;
		float descender;
	};

public:
	Font() = default;

	/// Loads the font from a given file.
	/// Throws on error (e.g. if the file does not exist/invalid font).
	/// The first overload uses the contexts default font atlas.
	Font(Context&, StringParam file);
	Font(FontAtlas&, StringParam file);

	/// Loads the font from a otf/ttf blob.
	/// Throws on error (invalid font data).
	/// The first overload uses the contexts default font atlas.
	Font(Context&, std::vector<std::byte> data);
	Font(FontAtlas&, std::vector<std::byte> data);

	/// Create a pure logical font for the given atlas (or default atlas).
	/// A pure logical font can only be used with fallback fonts.
	Font(Context&);
	Font(FontAtlas&);

	/// Adds the given font as fallback.
	/// Must be allocated on the same atlas.
	void fallback(const Font& f);

	float width(std::string_view text, unsigned height) const;
	nytl::Rect2f bounds(std::string_view text, unsigned height) const;
	Metrics metrics() const;

	bool valid() const { return atlas_ && id_ >= 0; }
	auto& atlas() const { return *atlas_; }
	auto id() const { return id_; }

protected:
	FontAtlas* atlas_ {};
	int id_ {};
};

} // namespace rvg
