# Image Hider
This tool allows you to encode binary data into
uncompressed TIFF files. If can encode any file
into a given image if the image is large enough.

## Build
The build automation tool `make` is used and a makefile is provided. The executable produced is named `hider`.

## Encoding
![Image with executable](https://github.com/mcnultyc/image-hider/blob/master/images/picnic_executable.tif)<br>
As an example of the ability of this tool to encode arbitrary files, I've encoded the executable for this program inside of the image above using the following command `./hider -e images/picnic.tif hider`. The TIFF file from above with the encoded executable [pinic_executable.tif](images/picnic_executable.tif) is located in the images directory.

## Decoding
A functional copy of the executable can then be recreated using the decode command line argument `./hider -e images/picnic.tif hider_decoded` and then updating file permissions with `chmod u+x hider_decoded`.
