#pragma once

#include "fwd.hpp"
#include "container.hpp"

namespace vui {

/// Layout that just puts each widget in its own row.
class RowLayout : public LayoutWidget {

};

/// Layout that puts each widget in its own column.
class ColumnLayout : public LayoutWidget {

};

/// Layout that has a fixed AxB table/matrix in which it puts widgets.
/// Width of columns and heights of rows can be variable.
class TableLayout : public LayoutWidget {
};

} // namespace vui
