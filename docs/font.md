Requirements

- create fonts dynamically
- destroy fonts dynamically
- change the needed codepoint dynamically
	- extend them when a new char is needed
	- also allow to shrink it at any time again
- use variable number of font atlas objects

class Font {
public:
	const Glyph& glyph(uint32_t utf32);
	const Glyph* findGlyph(uint32_t utf32) const;
	void rerange(Span<const CharRange> ranges, bool shrink = true);
};

Maybe c api?

```c
struct fa_font_atlas {
};

void fa_font_atlas_init(struct fa_font_atlas*);
void fa_font_atlas_destroy(struct fa_font_atlas*);
... fa_font_atlas_get_data(...)

struct fa_font {
};


struct fa_glyph* fa_font_glyph(uint32_t utf32);
struct fa_glyph* fa_font_try_glyph(uint32_t utf32);
```
