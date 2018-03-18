class Paint {
public:
	Paint(Color& color
	// void makeColor(Context&, const Color&);
	// void makeLinearGradient(Context&);
	// void makeBoxGradient(Context&);
	// void makeRadialGradient(Context&);
	// void makeTexture(Context&);

	void bind(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange ubo;
	vk::ImageView texture;
	vpp::DescriptorSet ds;
};

// next
class Subpath {
};

class Path {
public:
	Subpath& move(Vec2f to);
	void line(Vec2f to);
	void quad(Vec2f to, Vec2f control);
	void bezier(Vec2f to, Vec2f control1, Vec2f control2);
	void arc(Vec2f center, float begin, float end, float radius);

protected:
	std::vector<Subpath> subpaths_;
};

class StrokePath {
public:
	void draw(Context& ctx, vk::CommandBuffer cmdb);
	bool update(Path& path);
};

class FillPath {
public:
	void draw(Context& ctx, vk::CommandBuffer cmdb);
	bool update(Path& path);
};



// api usage test dummy
struct Style {
	Paint* fill;
	Paint* stroke;
};

class Widget {
public:
	void draw(Context& ctx, vk::CommandBuffer cmdb) {
		if(style_->fill) {
			style_->fill->bind(ctx, cmdb);
			fill_.draw(ctx, cmdb);
		}
	}

	bool updateDevice() {
		if(!update_) {
			return false;
		}

		auto ret = false;
		ret |= style_->stroke && stroke_.update(path_);
		ret |= (style_->fill) && fill_.update(path_);
		return ret;
	}

private:
	Path path_;
	StrokePath stroke_;
	FillPath fill_;
	Style* style_;
	bool update_;
};


    // Glyph metrics:
    // --------------
    //
    //                       xmin                     xmax
    //                        |                         |
    //                        |<-------- width -------->|
    //                        |                         |
    //              |         +-------------------------+----------------- ymax
    //              |         |    ggggggggg   ggggg    |     ^        ^
    //              |         |   g:::::::::ggg::::g    |     |        |
    //              |         |  g:::::::::::::::::g    |     |        |
    //              |         | g::::::ggggg::::::gg    |     |        |
    //              |         | g:::::g     g:::::g     |     |        |
    //    offsetX  -|-------->| g:::::g     g:::::g     |  offsetY     |
    //              |         | g:::::g     g:::::g     |     |        |
    //              |         | g::::::g    g:::::g     |     |        |
    //              |         | g:::::::ggggg:::::g     |     |        |
    //              |         |  g::::::::::::::::g     |     |      height
    //              |         |   gg::::::::::::::g     |     |        |
    //  baseline ---*---------|---- gggggggg::::::g-----*--------      |
    //            / |         |             g:::::g     |              |
    //     origin   |         | gggggg      g:::::g     |              |
    //              |         | g:::::gg   gg:::::g     |              |
    //              |         |  g::::::ggg:::::::g     |              |
    //              |         |   gg:::::::::::::g      |              |
    //              |         |     ggg::::::ggg        |              |
    //              |         |         gggggg          |              v
    //              |         +-------------------------+----------------- ymin
    //              |                                   |
    //              |------------- advanceX ----------->|<Paste>
