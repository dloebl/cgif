## cgif tools
gifresize is a simple tool (experimental) for fast GIF resizing using cgif.

## running gifresize
```sh
cd cgif
# init and update libnsgif submodule
git submodule init
git submodule update
cd cgif_tools
make
# ./gifresize <input-gif> <output-gif> <new-width> <new-height>
```

## Notes
- GIF frames with more than 256 colors are currently not supported.
- User-defined transparency is not yet supported.
