Putting BDF into `.data` segemnt.
=================================

Simple tool to generate bitmap-fonts stored in C-structs from BDF fonts to be
used in C-Programs.

The focus is to generate a somewhat compact representation of fonts suited
for embedded systems.

To get the most compact representation, you give the tool only the characters
you need.

At this point, it represents fonts in stripes, needed for SSD1306 type of
displays.

```
make -C src
# Invoking tool
src/generate-compiled-font path/to/font.bdf myfontname "01234567890μπABC"
```

This generates a new files `font-myfontname.{h,c}`. Copy this together with
`client-lib/font-support.{h,c}` into your project and compile there. The
`font-support.h` header provides ways to access your generated font.
