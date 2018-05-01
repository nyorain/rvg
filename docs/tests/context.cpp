// Tests context and deviceObject creation, life/render-cycle and destruction.

#include <rvg/context.hpp>
#include "main.hpp"

TEST(creation) {
	rvg::ContextSettings settings;
	settings.renderPass = globals.rp;
	settings.subpass = 0u;

	rvg::Context ctx(*globals.device, settings);
}
