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
#include <variant>

struct FONScontext;

namespace rvg {

/// Holds a texture on the device to which multiple fonts can be uploaded.
class FontAtlas : public DeviceObject, public nytl::NonMovable {
public:
	FontAtlas(Context&);
	~FontAtlas();

	auto& ds() const { return ds_; }
	auto& texture() const { return texture_; }
	auto* stash() const { return ctx_; }

	// - usually not needed -
	bool updateDevice();
	void validate();
	void expand();

	void added(Text&);
	void removed(Text&);
	void moved(Text&, Text&) noexcept;

protected:
	FONScontext* ctx_;
	std::vector<Text*> texts_;
	vpp::TrDs ds_;
	Texture texture_;
	bool invalid_ {};
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
	struct Description {
		std::variant<StringParam, Span<const std::byte>> from;
		unsigned height {12};
	};

public:
	/// Loads the font from a given file.
	/// Throws on error (e.g. if the file does not exist/invalid font).
	/// If no FontAtlas is given, uses the contexts default one.
	Font(Context&, StringParam file);
	Font(FontAtlas&, StringParam file);

	/// Loads the font from a otf/ttf blob.
	/// Throws on error (invalid font data).
	/// If no FontAtlas is given, uses the contexts default one.
	Font(Context&, std::vector<std::byte> font);
	Font(FontAtlas&, std::vector<std::byte> font);

	float width(std::string_view text, unsigned height) const;
	float width(std::u32string_view text, unsigned height) const;

	auto& atlas() const { return *atlas_; }
	auto id() const { return id_; }

protected:
	FontAtlas* atlas_;
	int id_ {};
	std::vector<std::byte> blob_;
};

} // namespace rvg
