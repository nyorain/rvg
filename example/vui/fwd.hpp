#pragma once

#include <nytl/fwd.hpp>

namespace vgv {

struct DrawInstance;
class Context;
class Scissor;
class Transform;
class Font;
class FontAtlas;
class Paint;

} // namespace vgv

namespace vui {

using namespace nytl;
using namespace vgv;

class Gui;
class Widget;
struct Styles;

class Button;
class Hint;
class DelayedHint;

} // namespace vui
