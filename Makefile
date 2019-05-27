hider: tif.o image_hider.o
	gcc -o hider tif.o image_hider.o

tif.o: tif.h tif.c
	gcc -c tif.c

image_hider.o: image_hider.c
	gcc -c image_hider.c

clean: 
	rm tif.o image_hider.o hider
