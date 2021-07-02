## CGIF, a GIF encoder written in C

A fast and lightweight GIF encoder that can create GIF animations and images. Summary of the main features:
- user-defined global or local color-palette with up to 256 colors (limit of the GIF format)
- size-optimizations for GIF animations:
  - option to set a pixel to transparent if it has identical color in the previous frame (transparency optimization)
  - do encoding just for the rectangular area that differs from the previous frame (width/height optimization)
- fast: a GIF with 256 colors and 1024x1024 pixels can be created in below 50 ms even on a minimalistic system
- MIT license (permissive)
- different options for GIF animations: static image, N repetitions, infinite repetitions
- code supplemented with comments and explanatory material (easy to extend and also suited for teaching purposes)
- additional source-code for verifying the encoder after making changes
- user-defined delay time from one frame to the next (can be set independently for each frame)
- source-code conforms to the C99 standard

## Examples
To get started, we suggest that you have a look at our code examples. ```cgif_example_video.c``` is an example that creates a GIF animation. ```cgif_example.c``` is an example for a static GIF image.

## Overview
To get an overview of the code, we recommend having a look at the header file ```cgif.h``` where types and functions are declared. The corresponding implementations can be found in ```cgif.c```. Here the most important types and functions:

```C
// These are the four struct types that contain all GIF data and parameters:
typedef GIFConfig               // global cofinguration parameters of the GIF
typedef FrameConfig             // local configuration parameters for a frame
typedef GIF                    // struct for the full GIF
typedef Frame                  // struct for a single frame

// The user needs only these three functions to create a GIF image:
GIF* cgif_newgif    (GIFConfig* pConfig);               // creates a new GIF
int  cgif_addframe  (GIF* pGIF, FrameConfig* pConfig);  // adds a frame to an existing GIF
int  cgif_close     (GIF* pGIF);                      // close the created file and free memory
```

With our encoder you can create animated or static GIFs, you can or cannot use certain optimizations, and so on. You can switch between all these different options easily using the two attributes ```attrFlags``` and ```genFlags``` in the configurations ```GIFConfig``` and ```FrameConfig```. These attributes are of type ```uint32_t``` and bundle yes/no-options with a bit-wise logic. So far only a few of the 32 bits are used leaving space to include further functionalities ensuring backward compatibility. We provide the following flag settings which can be combined by bit-wise or-operations:
```C
GIF_ATTR_IS_ANIMATED          // make an animated GIF (default is non-animated GIF)
GIF_ATTR_NO_GLOBAL_TABLE      // disable global color table (global color table is default)
FRAME_ATTR_USE_LOCAL_TABLE    // use a local color table for a frame (not used by default)
FRAME_GEN_USE_TRANSPARENCY    // use transparency optimization (size optimization)
FRAME_GEN_USE_DIFF_WINDOW     // do encoding just for the sub-window that has changed from the previous frame
```
If you didn't understand the point of ```attrFlags``` and ```genFlags``` and the flags, please don't worry. The example files ```cgif_example.c``` and ```cgif_example_video.c``` are all you need to get started and the used default settings for ```attrFlags``` and ```genFlags``` cover most cases quite well.

## Compiling the example
An example can be compiled and tested simply by:
```
$ cc -o cgif_example cgif_example_video.c cgif.c
$ ./cgif_example

```

## Validating the encoder
In the folder ```/tests```, we provide several testing routines that you can be run via the script ```performtests.sh```. To perform the tests you need to install the programs [ImageMagick](https://github.com/ImageMagick/ImageMagick), [gifsicle](https://github.com/kohler/gifsicle) and [tcc (tiny c compiler)](https://bellard.org/tcc/). 
With the provided tests you can validate that the encoder still generates correct GIF files after making changes on the encoder itself.

## Further explanations
The GIF format employs the [Lev-Zimpel-Welch (LZW)](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) algorithm for image compression. If you are interested in details of the GIF format, please have a look at the official GIF documentation (https://www.w3.org/Graphics/GIF/spec-gif89a.txt).

## License
Licensed under the MIT license (permissive).
For more details please see ```LICENSE```
