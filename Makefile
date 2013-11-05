LDLIBS = -lm
CXXFLAGS = -std=c++11 -I../include -g -O0
#LDFLAGS = -pg
CC = $(CXX)

top:	 myOCR1 myOCR byregion id

all:	 myOCR1 myOCR byregion id

testset:	testset.cc
	g++ -g testset.cc  -lm -o testfont
testfont: testfont.o arg.o histogram.o font.o image.o 
makefont: makefont.o templates.o arg.o histogram.o font.o
byregion:	byregion.o arg.o histogram.o font.o templates.o htemp.o region.o perim.o
middle: middle.o arg.o templates.o histogram.o 
contig: contig.o arg.o templates.o histogram.o 
block:	block.o  arg.o templates.o histogram.o 
trim:	trim.o  arg.o templates.o histogram.o 
id:	id.o arg.o histogram.o font.o templates.o htemp.o region.o 
myOCR:	myOCR.o arg.o histogram.o font.o templates.o htemp.o region.o 
myOCR1:	myOCR1.o arg.o histogram.o font.o templates.o htemp.o region.o

region.o:	region.cc image.h region.h
testfont.o:	image.h font.h testfont.cc
makefont.o:	image.h font.h makefont.cc
font.o:	image.h font.h font.cc
byregion.o:	image.h histogram.h hash.h region.h 
gram.o:	histogram.cc histogram.h
middle.o:	middle.cc image.h 
templates.o:	templates.cc image.h image.cc region.h 
htemp.o:	htemp.cc hash.h hash.cc set.h 
contig.o:	contig.cc image.h
trim.o:	trim.cc image.h
block.o:	block.cc image.h
image.o:	image.cc image.h
arg.o:	arg.cc arg.h

clean:
	rm *.o trim block contig *.rpo



