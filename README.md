# SSG Video Player
  Multi process video player for windows.

## License
  SSG Video Player is released under the GPL version 2 license. See LICENSE for more information.

## Requirements
  * ffmpeg 2.1
  * SDL 1.2+
  
## Building on Linux
  This project is built with a cross compiling gcc toolchain in Linux, using the spank build system, available at https://github.com/noname22/spank.
	
    spank build
  or

    spank build release 

## Building with Docker
    docker build -t vidbuild docker
    docker run --rm -i -t -v $(pwd):/root/build/source vidbuild
