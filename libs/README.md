# libs/

These external libaries are compiled alongside usb-creator due to conflicts with the zlib bundled with QT.

If we try to link against pre-compiled versions of these libraries, we get terrible linking errors during cross-compilation.

I spent a few hours trying different solutions, it's not worth trying to get rid of this folder.

\- Justin 2023-07-27
