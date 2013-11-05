#include "image.h"
#include "arg.h"
#include <unordered_set>
#include <set>
#include "histogram.h"
#include "font.h"
#include <iostream>

using namespace std;

int edgelen = 100;
static int si=0;

int verbose = 0;
const char* inname = "y.pgm";
const char* outname = "block";
int radius = 1;
char* intermediateFiles = NULL;
int maxPasses = 50;
int genHist = 1;
int startPass = 20;
int startSize = 10;
int endSize = 1000;

int ldebug = 0;


ArgDesc argdescs[] = {
		{ ArgDesc::Position, "", 0, ArgDesc::Pointer, &inname, ArgDesc::Optional },
		{ ArgDesc::Flag, "--radius", 0, ArgDesc::Int, &radius, ArgDesc::Optional },
		{ ArgDesc::Flag, "--start", 0, ArgDesc::Int, &startPass, ArgDesc::Optional },
		{ ArgDesc::Flag, "--passes", 0, ArgDesc::Int, &maxPasses, ArgDesc::Optional },
		{ ArgDesc::Flag, "--ss", 0, ArgDesc::Int, &startSize, ArgDesc::Optional },
		{ ArgDesc::Flag, "--es", 0, ArgDesc::Int, &endSize, ArgDesc::Optional },
		{ ArgDesc::Flag, "-v", 0, ArgDesc::Flag, &verbose, ArgDesc::Optional },
		{ ArgDesc::Flag, "-s", 0, ArgDesc::Flag, &si, ArgDesc::Optional },
		{ ArgDesc::Flag, "-i", 0, ArgDesc::Pointer, &intermediateFiles, ArgDesc::Optional },
		{ ArgDesc::Position, "", 1, ArgDesc::Pointer, &outname, ArgDesc::Optional },
		{ ArgDesc::END, "", 0, 0, 0, 0 }
};

char*
imageFilename(const char* temp, const char* ext)
{
	static int id=0;
	static char filename[256];
	sprintf(filename, "%04d-%s.%s", id++, temp, ext);
	printf("Generating: %s\n", filename);
	return filename;
}

//global variables
Font* a12;
Image<int>* tiled;
Image<unsigned char>* pImage;

bool isIntersect(Point boundingBox1, Point boundingBox2, Point testPoint, Point epsilonPoint) {

	Point p0 = boundingBox1;
	Point p1 = boundingBox2;
	Point p2 = testPoint;
	Point p3 = epsilonPoint;

    double s1_x = p1.x - p0.x;
    double s1_y = p1.y - p0.y;
    double s2_x = p3.x - p2.x;
    double s2_y = p3.y - p2.y;

    double s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y- p2.y)) /
(-s2_x * s1_y + s1_x * s2_y);
    double t = ( s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) /
(-s2_x * s1_y + s1_x * s2_y);

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
    	return true;
    }
    else
    	return false;
}



//white= 255;
//black = 0;
int main( int argc, char* argv[] )
{

	int outw = 4000;
	int outh = 5000;
	int leftMargin = 4;

	Image<int>* output = new Image<int>(outw+leftMargin, outh);
	output->fill(0x0ffffff);

	ArgDesc::procargs(argc, argv, argdescs);
	printf("%s: %s -> %s  radius=%d\n", ArgDesc::progname, inname, outname, radius);

	a12 = new Font("output12.fnt");

	//////////////////////////////////////////////////////////////////////////
	// Read gray-value image
	//////////////////////////////////////////////////////////////////////////
	pImage = Image<unsigned char>::readPGM(inname);
	if( ! pImage )  {
		printf( "Reading image failed!\n");
		exit(-1);
	}

	// init to white and black
	Image<int>* regions = new Image<int>(pImage->getWidth(), pImage->getHeight());
	Image<int>* regionTest = new Image<int>(pImage->getWidth(), pImage->getHeight());

	for (int y=0; y<pImage->getHeight(); y++) {
		for (int x=0; x<pImage->getWidth(); x++) {
			if (pImage->isNearWhite(pImage->getPixel(x, y))) {
				regions->setPixel(x, y, 0);
			} else {
				regions->setPixel(x, y, 1);
			}
		}
	}


	// zap singleton pixels
	for (int y=0; y<regions->getHeight(); y++) {
		for (int x=0; x<regions->getWidth(); x++) {
			if (regions->getPixel(x, y) == 1) {
				// see if surrounded only by white
				int count = 0;
				for (int i=0; i<8; i++) {
					int xx, yy;
					if (regions->getNeighbor(x, y, i, xx, yy)) {
						// there was a ith neighbor and it is at xx,yy
						if (regions->getPixel(xx, yy) == 1) {
							count++;
							break;
						}
					}
				}
				if (count == 0) {
					if (ldebug) printf("Removed lone pixel at %d,%d\n", x, y);
					regions->setPixel(x, y, 0);
				}
			}
		}
	}
	for(int i=0 ; i<regions->getHeight() ; i++)
		for(int j=0 ; j<regions->getWidth() ; j++)
			//printf("%d", regions->getPixel(i,j));
			if(regions->getPixel(j,i) != 0)
				regions->setPixel(j,i,255);


	regions->PPMout("MyoutputFile"); //PPM is for RGB

	//*****************************************************************************************************************
	//read the file block-desc.txt, write every block in a new image file

	//for now hardcoding one region

	//(545,208)
	/*  (545,4249)
        (1448,4249)
        (1448,629)
        (3071,629)
        (3071,3158)
        (3968,3158)
        (3968,208)
	 */
	//int trace[numPoints][2] = { (545,4249),(1448,4249),(1448,629),(3071,629),(3071,3158),(3968,3158),(3968,208)};
	//Point* BBpoint = new Point[numPoints];
	//testPoint->j = j;//regions->getPixel(j,i);//this won't work
	int intersectCount = 0;
	int numPoints = 8;
	Point* trace = new Point[numPoints];

//	trace[0].x = 545;
//	trace[0].y = 4249;
//
//	trace[1].x = 1448;
//	trace[1].y = 4249;
//
//	trace[2].x = 1448;
//	trace[2].y = 629;
//
//	trace[3].x = 3071;
//	trace[3].y = 629;
//
//	trace[4].x = 3071;
//	trace[4].y = 3158;
//
//	trace[5].x = 3968;
//	trace[5].y = 3158;
//
//	trace[6].x = 3968;
//	trace[6].y = 3158;
//
//	trace[7].x = 3968;
//	trace[7].y = 208;

	trace[0].y = 545;
	trace[0].x = 4249;

	trace[1].y = 1448;
	trace[1].x = 4249;

	trace[2].y = 1448;
	trace[2].x = 629;

	trace[3].y = 3071;
	trace[3].x = 629;

	trace[4].y = 3071;
	trace[4].x = 3158;

	trace[5].y = 3968;
	trace[5].x = 3158;

	trace[6].y = 3968;
	trace[6].x = 3158;

	trace[7].y = 3968;
	trace[7].x = 208;

	//Point* trace = { (545,4249),(1448,4249),(1448,629),(3071,629),(3071,3158),(3968,3158),(3968,208)};

	Point testPoint;
	Point epsilonPoint;//epsilonPoint remains constant for a particular bounding box

	//very expensive!!!
	//for every pixel in image
	for(int i=0 ; i<regions->getHeight() ; i++)
		for(int j=0 ; j<regions->getWidth() ; j++) {
			//how do I get access to every pixel co-ordinate (Point-x,y) in an image?

			//point to be tested
			testPoint.x = j;
			testPoint.y = i;

			//epsilonPoint :: Need to be set!!!!!
			epsilonPoint.x = 545-10;
			epsilonPoint.y = 4249+20;

			intersectCount = 0;
			bool intersected = false;

			for(int k = 0 ; k<numPoints ; k++ ) {
				//for each of the 8 joining lines
				intersected = false;
				if(k == (numPoints-1))
					intersected = isIntersect(trace[k],trace[0],testPoint,epsilonPoint);
				else
					intersected = isIntersect(trace[k],trace[k+1],testPoint,epsilonPoint);

				if( intersected == true) {
					intersectCount++;
				}
			}

			if((intersectCount !=0 || (intersectCount%2) != 0) && (regions->getPixel(j,i)) !=0  ) {//odd
				regionTest->setPixel(j,i,255);
			}
			else
				regionTest->setPixel(j,i,0);
		}

	regionTest->PPMout("MyoutputFileTest");
}



