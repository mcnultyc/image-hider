hider: hdr.o tif.o image_hider.o
	gcc -o hider hdr.o tif.o image_hider.o

hdr.o: hdr.h hdr.c
	gcc -c hdr.c

tif.o: tif.h tif.c
	gcc -c tif.c

image_hider.o: image_hider.c
	gcc -c image_hider.c

clean: 
	rm hdr.o tif.o image_hider.o hider
