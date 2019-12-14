# Image Hider
This tool allows you to encode binary data into
uncompressed TIFF files. If can encode any file
into a given image if the image is large enough.

## Build
A makefile is provided.

## Encoding
![Image with executable](https://github.com/mcnultyc/image-hider/blob/master/images/picnic_executable.tif)
As an example of the ability of this tool to encode arbitrary files, I've encoded the executable for this program inside of the image above using the following command `./hider -e images/picnic.tif hider`.

## Decoding
A functional copy of the executable can then be recreated using the decode command line argument `./hider -e images/picnic.tif hider_decoded` and then updating file permissions with `chmod u+x hider_decoded`.
