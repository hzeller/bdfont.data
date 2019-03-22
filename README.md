Putting BDF into the `.data` segment
====================================

(Note, this a hacky tool I use for personal projects; expect changes.)

Simple tool to generate bitmap-fonts stored in C-structs from BDF fonts to be
used in C-Programs.

The focus is to generate a somewhat compact representation of fonts suited
for embedded systems with little flash memory.

Fonts can have any Unicode characters from the Basic Multilingual Plane (the
first 16 bit), so it is easy to include special characters such as μ or π.
Because of the vast choice, you have to specify _which_ of these you'd like
to include into the compiled code to keep data compact. It is perfectly
ok to have 'sparse' fonts, i.e. only contain the characters you need for
your program strings.

```
make -C src
# Invoking tool. Multiple same characters are only included once.
src/generate-compiled-font path/to/font.bdf myfontname "01234567890μHelloWorld"
```

This generates a new files `font-myfontname.{h,c}`. Copy this together with
`client-lib/font-support.{h,c}` into your project and compile there.
The `font-support` provides ways to access your generated font.

At this point, the generated code is pretty specific to represent fonts in
'stripes', needed for SSD1306 type of displays.
