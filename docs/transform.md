All coordinates given e.g. as positions, sizes or points are interpreted in
world space. That space is purely of logical nature and its meaning interpreted
by the user.
You could define primitives like in plain vulkan; normalized in range [-1, 1] or
in window space [0, width] x [0, height] or directly in the logical space of
your 2d game world. The definition of this space happens by the user via the
transform state (rvg::Transform). There the user can set a transform matrix
which must map the given point into vulkan normalized coordinates, i.e.
the [-1, 1] range.

__NOTE: better to require the transform to end in window space?__

But there are also other quantities given in rvg for which the situation
is a little bit more complicated: text height, stroke width, fringe,
RectShape corner rounding (and probably some more). When your logical world
space is e.g. already the vulkan normalized space (and your transform matrix
therefore the identity) what is the expected behaviour when drawing a Text
object with height 12 in position (0, 0)? The height 12 corresponds to 12 pixels,
meaning the font glyphs used for this text heave pixel height 12.
So, 12 pixels? But that means that the Text object has to know about the current
transform (to even know what "pixels" are) and therefore all data has to
be reprocessed and reuploaded every time your transform changes (and there
arise other problems). We can do better. __NOTE: not so sure we can?__

maybe only pre-transform?
