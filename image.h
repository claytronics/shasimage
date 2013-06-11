#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <string.h>
#include <math.h>
#include <unordered_set>
#include <assert.h>
#include <vector>
#include "histogram.h"
#include "point.h"
#include "region.h"

template<class T>
T mymax(const T&a, const T&b) 
{
    if (a > b) return a;
    return b;
}

template<class T>
T mymin(const T&a, const T&b) 
{
    if (a < b) return a;
    return b;
}
typedef std::unordered_set<Point*,hash_PointP,eq_PointP>  RegionSet;


union RGB
{
	int m_Val;
	struct
	{
		unsigned char m_b;
		unsigned char m_g;
		unsigned char m_r;
		unsigned char m_a;
	};
	unsigned char byte[4];
	
	RGB () : m_r(0), m_g(0), m_b(0), m_a(255){}
	RGB (unsigned char gray) : m_r(gray), m_g(gray), m_b(gray), m_a(255){}
	RGB (unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) : m_r(r), m_g(g), m_b(b), m_a(a){}
	RGB (int val) : m_Val(val){}

	unsigned char getHue() const
	{
            const int v = mymax<unsigned char>( m_r, (mymax<unsigned char>(m_g, m_b)) );
		if (0 == v)
			return 0;

		const int rgbMin = mymin<unsigned char>( m_r, (mymin<unsigned char>(m_g, m_b)) );
		const int rgbDiff = (int)v - rgbMin;

		if (0 == rgbDiff)
			return 0;

		int h;

		if ( v == m_r )
		{
			h = (43 * ((int)m_g - (int)m_b)) / rgbDiff;
		}
		else if ( v == m_g )
		{
			h = 85 + (43 * ((int)m_b - (int)m_r)) / rgbDiff;
		}
		else  // ( v == m_b )
		{
			h = 171 + (43 * ((int)m_r - (int)m_g)) / rgbDiff;
		}

		return h;
	}

	unsigned char getSat() const
	{
		int max = m_r;
		int min = m_r;

		// find maximum and minimum RGB values
		if (m_g > max)
		{
			max = m_g;
		} else {
			min = m_g;
		}

		if (m_b > max)
		{
			max = m_b;
		} else if (m_b < min) {
			min = m_b;
		}

		if (0 == max)
			return 0;

		const int delta = max - min;

		return ( (255*delta) / max );
	}

	unsigned char getV() const
	{
		return ( mymax<unsigned char>( m_r, (mymax<unsigned char>(m_g, m_b)) ) );
	}

	bool operator==(const RGB & otherVal)
	{
		return ( (m_r == otherVal.m_r) && (m_g == otherVal.m_g)&& (m_b == otherVal.m_b) && (m_a == otherVal.m_a) );
	}
};

class Region;
class RegionList;

template <class Pixel>
class NearInfo {
 public:
    int deltax;                 /* direction of dest region in x */
    int deltay;                 /* direction of dest region in y */
    bool valid;                 /* whether we found one */
    Point from;                 /* start point from my region */
    Point to;                   /* dest point for target region */
    int distance;               /* distance in pixels to target */
    double reldist;             /* distance with blocker factor */
    Pixel id;                   /* id of target */
    int size;                   /* size if we know it of target */
};

template <class Pixel> class Image {
public:
  int width;
  int height;
  Pixel* pixels;

 Image(const int w, const int h, Pixel* p) : width(w), height(h), pixels(p) {}
 Image(const int w, const int h) : width(w), height(h) {
    pixels = new Pixel[width*height];
  }
 Image(const Image<Pixel>* i) : width(i->width), height(i->height) {
    pixels = new Pixel[width*height];
    for (int y=0; y<height; y++)
      for (int x=0; x<width; x++) 
	setPixel(x, y, i->getPixel(x, y));
  }
  ~Image() {
      if (pixels != NULL) delete[] pixels;
  }
  static Image* readPGM(const char* fileName);
  static Image* read(const char* fileName);
  static int verbose;

  unsigned int getHeight(void) const { return height; }
  unsigned int getWidth(void) const { return width; }
  Pixel getPixel(const unsigned int x, const unsigned int y) const {
      assert((y>=0)&&(y<height)&&(x>=0)&&(x<width));
      return pixels[y*width+x];
  }
  const Pixel black(void) const {
    return 0;
  }
  const Pixel white(void) const {
      return ((1<<(sizeof(Pixel)<<3))-1);
  }
  const bool isNearBlack(Pixel p) const {
      return (p <= (0+(1<<(sizeof(Pixel)>>2))));
  }
  const bool isNearWhite(Pixel p) const {
      return (p >= (white() - (1<<(sizeof(Pixel)>>2))));
  }

  void connectNearest(const unsigned int x, const unsigned int y, bool direction);

  void setPixel(const unsigned int x, const unsigned int y, const Pixel val) {
      assert((y>=0)&&(x>=0)&&(y<=height)&&(x<=width));
      pixels[y*width+x] = val;
  }
  void setCircle(const int x, const int y, const Pixel val, int radius);

  void copyRegion(int sx, int sy, int sw, int sh, Image* dest, int dx, int dy);

  bool PGMout(const char * fileName);
  bool PPMout(const char * fileName);

  void fill(Pixel color) {
    for (int y=0; y<height; y++) {
      fillLine(y, color);
    }
  }
  void fillLine(int line, Pixel color) {
    for (int x=0; x<width; x++) {
      setPixel(x, line, color);
    }
  }

  void fillColumn(int col, Pixel color) {
    for (int y=0; y<height; y++) {
      setPixel(col, y, color);
    }
  }

  void blobHist(void);
  Image* findBlobs(int size, int border);

  Image* block(int direction=3);
  int fillIn(int radius);

  Image* crop(unsigned int startx, unsigned int endx, unsigned int starty, unsigned int endy) {
    int outwidth = endx-startx;
    int outheight = endy-starty;
    Image* out = new Image<Pixel>(outwidth, outheight);
    for (int y=starty; y<endy; y++) {
      for (int x=startx; x<endx; x++) {
	out->setPixel(x-startx, y-starty, this->getPixel(x, y));
      }
    }
    return out;
  }
  bool getNeighbor(int x, int y, int n, int& xx, int& yy);
  void labelRegion(int x, int y, Pixel srcid, Pixel destid);
  Pixel firstDiffColor(int x, int y, Pixel c, int dx, int dy);
  Image* trim(int edge);

  void drawLine(int sx, int sy, int dx, int dy, Pixel color);
  void drawLine(int sx, int sy, int dx, int dy, Pixel color, int linew);
  void drawBox(Point ul, Point lr, Pixel color, int lw);
  void findConnectors(Region* list, int targetId, std::vector<Distance>& connectors);
  int findNearestRegions(Region* list, NearInfo<Pixel>* result, int maxdist, int maxsize, Image<unsigned char>* blocker, double bfactor, RegionList* rl);
  int findNearestRegionsByPerim(Region* list, NearInfo<Pixel>* result, int maxdist, int maxsize, Image<unsigned char>* blocker, double bfactor, RegionList* rl);
  bool findNearestRegion(int x, int y, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist);
  bool findNearestRegion(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<int>* blocker);
  bool findNearestRegion(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<unsigned char>* blocker, double bfactor, RegionList* rl=NULL, int maxsize=0);
  bool findNearestRegionX(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<unsigned char>* blocker);
  bool findNearestRegionY(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<unsigned char>* blocker);
  void getDistHelper(int& fromx, int& fromy, int dx, int dy, Pixel rid, int&tox, int&toy, int&dist);
  void fillInscribe(int x, int y);
  void fillInscribe(Region* list);
  RegionSet* getPointsOfRegion(int x, int y);
  RegionList* getRegions(void);
  Region* labelAndReturnRegion(int x, int y, Pixel srcid, Pixel destid);
  RegionList* createRegions(void);
  Histogram* collectEdgeHistogram(int direc, Pixel color, int runlen);
  int count8Neighbors(int x, int y, Pixel rid);
  void addLine2Region(int tx, int ty, int yx, int yy, int targetRegionId, RegionList* rl, int srcId);

 private:
  int countAndZapBlack(int x, int y);
  int copyBlackAndZap(int startx, int starty, int num, int size, Point* points);
  int getLastBadRegion(unsigned int *data, int start, int end, int incr);
};

char* imageFilename(const char* temp, const char* ext);


#endif

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

