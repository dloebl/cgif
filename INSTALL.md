# Installation
cgif is packaged for [Homebrew](https://formulae.brew.sh/formula/cgif) and several Linux distributions already:
  
[![Packaging status](https://repology.org/badge/vertical-allrepos/cgif.svg)](https://repology.org/project/cgif/versions)

## Homebrew
`$ brew install cgif`
   
## Fedora >= 35
`$ sudo yum install libcgif`

for development files

`$ sudo yum install libcgif-devel`
   
## Manual installation
### Requirements for compiling cgif:
- C compiler (e.g. gcc, clang)
- meson build system
### Steps
1. `$ git clone https://github.com/dloebl/cgif`
2. `$ cd cgif/`

set the prefix of your choice:

3. `$ meson setup --prefix=/usr build`

compile & install cgif as shared library:

4. `$ meson install -C build`
