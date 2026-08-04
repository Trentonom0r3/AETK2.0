remove old helpers we don't eed anymore, make "_val" helpers be the standard public facing api - all helpers we DO need go into private, and we keep the public API public and clean. We need to dive deep into how we simplify this. Couple change's I've made - i swapped all in_data suite calls for using ::context::get_basic_suite, to have a unifed API. 
I'd also like to move iteration into its own header, and remove it from ctx (no reason to have it in ctx). 

We can call add public methods to smart world (that take a dst smart world, color, and func as params), making the api as simple as 

```cpp
// You can do this in the effect's render function

using Color = aetk::core::color<pixel_range::tkuint8>;
auto input = ctx.checkout_pixels(0); // now you have a smart_world.
auto output = ctx.checkout_output();
Color color = Color(255.0, 128.0, 123.0, 155.0);
input.iterate<pixel_range::tkuint8>(output, [&](int32_t x, int32_t y, aetk::core::color<pixel_range::tkuint8> &c) {
    c.red   = (c.red   + cval.red) / 2.0;
    c.green = (c.green + cval.green) / 2.0;
    c.blue  = (c.blue  + cval.blue) / 2.0;
    c.alpha = 255.0;
});
```


unified API for checking things out in pre render as well, 

.checkout globally. 

Need some more ideas too. 
