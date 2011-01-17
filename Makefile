all	:	amp-rg
debug :	amp-rg-debug

CFLAGS = -Wall -std=gnu99 -pedantic -g -I/usr/include/ffmpeg
LINK = -lavutil -lavformat -lavcodec -lz -lavutil -lm
EXE = amp-rg

amp-rg	: main.o gain_analysis.o
	gcc $(CFLAGS) -o $(EXE) main.o gain_analysis.o $(LINK)

amp-rg-debug	:	main.o
	gcc $(CFLAGS) -g -DDEBUG -o $(EXE) main.o $(LINK)

clean:
	-rm $(EXE) *.o
