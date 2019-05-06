# rvg transform state

rvg internally deals with anti aliasing, transforming paths into shapes
(e.g. rounded rect corners or circles) and font sizes. Therefore it has
to know about pixels and pixel sizes at some point. At the same time
rvg wants to allow efficient transforms, e.g. moving complicated shapes
and lots of text around without re-computing and re-uploading all the data
for these shapes. To implement both goals correctly, rvg exposes 2 types
of transforms: pre-transforms and post-transforms. If you only intend to
work in window space (and something that has at least the same *scale*) you
can entirely skip pre-transforms and just read the simple example; rvg 0.1
was released without the concepts of pre-transforms at all and usually
you won't need it. In this case, just skip the next part until we come
to post transforms, i.e. `rvg::Transform`.
But if you ever intend to use rvg in spaces that are
window-independent you might have to care about pre-transforms.
But from the beginning (the full story, including pre-transforms):

All input positions and sizes to rvg can be passed *in whatever space you
want*. Feel free to pass (0.1, 0.1) as size for a RectShape and 0.05 as
text font height to rvg if you want to work in normalized device coordinate
space. Alternatively you can obviously use rvg in your level/world space the
same way. **But in those cases you must use a pre-transform!**. Because after
the pre-transform step, rvg expects all coordinate values to be in a space,
where a unit of `1` equals 1 pixels. As outlined above, this is required
for correct font rendering, anti aliasing and curve tesselation. Usually
(and the case in rvg 0.1), this pre-transform is just the identity, therefore
the value you pass to rvg must be in window-sized space. Note that they
don't have to be exactly in window space, just in a space which has the
same scale as window space. That means you can freely apply rotations
and translations in post-transform. You usually want to use that since
changing the pre-transform must be done on a per-shape/per-text basis
and requires a full re-compute and re-upload of the shape, i.e. it's
expensive.

Then, after the pre-transform step, the data is passed to the gpu and there
the post-transform step is applied. The post-transform is simply a
shape-independent matrix on the gpu, managed via rvg::Transform, bound
at command buffer recording time. Changing the transform later on
is pretty cheap, it's just a single buffer update. You can have as many
Transform objects as you need. The post-transform matrix
(i.e. what you pass to rvg::Transform) takes coordinates as returned
by the pre-transform matrix and converts them to vulkan ndc space.
Since this matrix is supplied by the user, rvg never actively has to
care about window size. For a typical window-space to ndc matrix,
see the examples.

Using rvg::Transform for scaling (aside from the window-size-space
to ndc scaling) can still be useful e.g. for debugging or if you
explicitly want all shapes to stay *exactly* the same, even for
properties like antialiasing width or the used font size. But keep
in mind that it will result in unexpected behaviour for users.
Best is to just use rvg as it was originally intended: in window space.
