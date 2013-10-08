#include "image.h"
#include "arg.h"
#include <unordered_set>
#include <set>
#include "histogram.h"
#include "font.h"

template <class Pixel> extern Image<Pixel>* makePerimImage(Image<Pixel>* img, RegionList* allRegions);

int edgelen = 100;
int si=0;
static int startImageSaving = 0;
static int debugcrossing = 0;
Image<int>* dci = NULL;

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
int recttest = 0;

int ldebug = 0;


ArgDesc argdescs[] = {
    { ArgDesc::Position, "", 0, ArgDesc::Pointer, &inname, ArgDesc::Optional },
    { ArgDesc::Flag, "--radius", 0, ArgDesc::Int, &radius, ArgDesc::Optional },
    { ArgDesc::Flag, "--test", 0, ArgDesc::Flag, &recttest, ArgDesc::Optional },
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

template <class Pixel>
int
addDividerH(Image<Pixel>* dest, int start, int extra)
{
    int i;
    for (i=0; i<16; i++) dest->fillLine(start+i, 128);
    for (; i<16+extra; i++) dest->fillLine(start+i, 200);
    for (; i<32+extra; i++) dest->fillLine(start+i, 128);
    return 32+extra;
}

template <class Pixel>
int
addDividerV(Image<Pixel>* dest, int start, int extra)
{
    int i;
    for (i=0; i<16; i++) dest->fillColumn(start+i, 128);
    for (; i<16+extra; i++) dest->fillColumn(start+i, 200);
    for (; i<32+extra; i++) dest->fillColumn(start+i, 128);
    return 32+extra;
}

template <class Pixel>
int blackIn(Image<Pixel>* I, int x, int y)
{
    int maxp;
    int xm = (x == -1)?1:0;
    int ym = (y == -1)?1:0;
    maxp = I->getWidth() * xm + I->getHeight() * ym;
    int count = 0;
    for (int i=0; i<maxp; i++) {
        if (I->isNearBlack(I->getPixel(xm?i:x, ym?i:y))) count++;
    }
    return count;
}

// linesOrColumns == 0 -> get line dividers
//                == 1 -> get column dividers
template <class Pixel>
int findBlocks(Image<Pixel>* pImage, int*& lines, int*& regions, int linesOrColumns, int round)
{
    char filename[256];

    Image<unsigned char>* lined = pImage->block();
    sprintf(filename, "strip-%d.pgm", round);
    lined->PGMout(filename);

    // find horizontal dividers
    int h = lined->getHeight();
    int w = lined->getWidth();
    printf("fb:%d %dx%d\n", round, w, h);
    int maxunit;
    int maxperunit;
    int countinaRow;
    if (linesOrColumns == 0) {
        // get info by rows
        maxunit = h;
        maxperunit = w;
        countinaRow = 1;
        lines = new int[maxunit];
        for (int y=0; y<h; y++) {
            int count = 0;
            for (int x=0; x<w; x++) {
                if (lined->getPixel(x, y) == lined->black()) count++;
            }
            if (count == maxperunit) lines[y] = 1; else lines[y] = 0;
            //printf("%d: %d -> %d\n", y, count, lines[y]);
        }
    } else {
        // get info by columns
        maxunit = w;
        maxperunit = h;
        countinaRow = 0;
        lines = new int[maxunit];
        for (int x=0; x<w; x++) {
            int count = 0;
            for (int y=0; y<h; y++) {
                if (lined->getPixel(x, y) == lined->black()) count++;
            }
            if (count == maxperunit) lines[x] = 1; else lines[x] = 0;
            //printf("%d: %d -> %d\n", y, count, lines[y]);
        }
    }
    // count changes between dividers and not dividers
    int changes = 0;
    int last = lines[0];
    for (int y=1; y<maxunit; y++) {
        if (lines[y] == last) continue;
        changes++;
        last = lines[y];
    }
    printf("%d region changes\n", changes);
    regions = new int[changes+3];
    last = lines[0];
    int rstart = 0;
    int r = 0;
    for (int y=1; y<maxunit; y++) {
        if (lines[y] == last) continue;
        //printf("Start new region:%d y=%d  start=%d\n", r, y, rstart);
        regions[r++] = rstart;
        rstart = y;
        last = lines[y];
    }
    regions[r++] = rstart;
    regions[r] = maxunit+1;
    int dividers = 0;
    for (int i=0; i<r; i++) {
        if (lines[regions[i]] == 1) dividers++;
        if (1) {
            printf("fb:%d %4d: %4d (%4d) %s\n", round, regions[i], regions[i+1]-regions[i], 
                   blackIn(lined, countinaRow ? -1: regions[i], countinaRow ? regions[i]: -1),
                   (lines[regions[i]] == 1) ? "Divider" : "");
        }
    }
    printf("fb:%d There are %d dividers\n", round, dividers);
    int neww = w+(countinaRow ? 0 : (dividers*32));
    int newh = h+(countinaRow ? (dividers*32) : 0);
    Image<unsigned char>* divided = new Image<unsigned char>(neww, newh);
    divided->fill(divided->white());
    int offset = 0;
    for (int i=0; i<r; i++) {
        int copy = (lines[regions[i]] == 0);
        int hh = regions[i+1]-regions[i];
        if (countinaRow) {
            if (copy) {
                pImage->copyRegion(0, regions[i], w, hh, divided, 0, offset);
                offset += hh;
            } else {
                offset += addDividerH(divided, offset, hh);
            }
        } else {
            if (copy) {
                pImage->copyRegion(regions[i], 0, hh, h, divided, offset, 0);
                offset += hh;
            } else {
                offset += addDividerV(divided, offset, hh);
            }
        }
    }
    sprintf(filename, "divided-%d.pgm", round);
    divided->PGMout(filename);
    //delete divided;
    //delete lined;
    return r;
}

int pickRegion(int start, int end, int num, int* lines, int* regions, int* next)
{
    if (num != 1) {
        fprintf(stderr, "Not implemented other than 1 yet\n");
        exit(-1);
    }
    int sofar = 0;
    int chosen = -1;
    int prevdivider = 0;
    int newstart = -1;
    int direc = (start < end) ? 1 : -1;
    int lastDivider = -1;
    for (int i=start; i!=end; i+=direc) {
        int blank = (lines[regions[i]] == 1);
        int hh = regions[i+1]-regions[i];
        if (blank) {
            sofar+=hh;
            prevdivider = hh;
            lastDivider = i;
        } else {
            if (hh < prevdivider) {
                // probably noise
                prevdivider += hh;
            } else {
                // bigger than last divider
                if ((chosen != -1)&&(hh > (regions[chosen+direc]-regions[chosen]))) {
                    // we had a title and this one is bigger, so were are good to go
                    newstart = regions[i];
                    break;
                } else {
                    chosen = i;
                }
            }
            sofar += hh;
        }
    }
    if (lastDivider != -1) {
        if (direc > 0) *next = regions[lastDivider+1]; else *next = regions[lastDivider]-1;
    } else {
        fprintf(stderr, "Could not find a last divider\n");
        exit(-1);
    }
    return chosen;
}

template<class Pixel>
void writeStrip(Image<Pixel>* body, int width, int offset, const char* fname)
{
    int h = body->getHeight();
    Image<unsigned char>* img = new Image<unsigned char>(width, h);
    body->copyRegion(offset, 0, width, h, img, 0, 0);
    img->PGMout(fname);
    delete img;
}

class Rect {
public:
  int x;
  int y;
  int w;
  int h;
  int area;
  Rect(int _x, int _y, int _w, int _h, int _area) : x(_x), y(_y), w(_w), h(_h), area(_area) {}
};

struct RectCompare {
  bool operator() (Rect* const& lhs, Rect* const & rhs) const
  {return lhs->area<rhs->area;}
};

typedef std::set<Rect*,RectCompare> RectpSet;

void
addCandidate(int x, int y, int rw, int rh, int area, RectpSet* blocks)
{
  if (blocks->size() < 20) {
    blocks->insert(new Rect(x, y, rw, rh, area));
    return;
  }
  Rect* r;
  for (auto it=blocks->cbegin(); it != blocks->cend(); ++it) {
    r = *it;
    if (r->area < area) {
      Rect* s = new Rect(x, y, rw, rh, area);
      blocks->insert(s);
      blocks->erase(r);
      r = s;
      printf("min area: %7d %3d,%3d\n", r->area, x, y);
      break;
    }
    break;
  }
}

template<class Pixel>
int
getBlackHeight(Image<Pixel>* img, int x, int y)
{
  int starty = y;
  while (y < img->getHeight()) {
    if (img->isNearBlack(img->getPixel(x, y))) y++; else break;
  }
  return (y-starty)-1;
}

template<class Pixel>
int
getBlackWidth(Image<Pixel>* img, int x, int y)
{
  int startx = x;
  while (x < img->getWidth()) {
    if (img->isNearBlack(img->getPixel(x, y))) x++; else break;
  }
  return (x-startx)-1;
}

template<class Pixel>
RectpSet*
findLargestRects(Image<Pixel>* img)
{
    RectpSet* blocks = new RectpSet;
    int h = img->getHeight();
    int w = img->getWidth();
    struct { int start; int len; } lines[h];
    for (int y=0; y<h; y++) {
        int x;
        for (x=0; x<w; x++) {
            Pixel c = img->getPixel(x, y);
            if (img->isNearBlack(c)) {
                // get x direc
                int rw = getBlackWidth(img, x, y);
                lines[y].start = x;
                lines[y].len = rw;
                break;
            }
        }
        if (x == w) {
            lines[y].start = 0;
            lines[y].len = 0;
        }
    }
    // now see about stacking them
    for (int y=0; y<h; ) {
        int minstart = lines[y].start;
        int start = lines[y].start;
        int end = lines[y].len + start;
        printf("Starting [%d,%d]\n", start, end);
        int maxlen = lines[y].len;
        int area = lines[y].len;
        int i;
        for (i=y+1; i<h; i++) {
            int s = lines[i].start;
            int e = lines[i].len + s;
            if ((e >= start)&&(s <= end)) {
                // keep going
                start = s;
                end = e;
                if (end-start > maxlen) maxlen = end-start;
                area += lines[i].len;
                if (start < minstart) minstart = start;
            } else {
                break;			// no longer intersecting
            }
        }
        addCandidate(minstart, y, maxlen, i-y, area, blocks);
        y=i;
    }
    return blocks;
}

static int listofcolors[] = {
0x00308F, 
0xE32636, 
0xC46210, 
0x841B2D, 
0x008000, 
0xE0218A, 
0x333399, 
0xCC0000, 
0x004225, 
0xFF0038, 
0x7B3F00, 
0xA40000, 
0x8B008B, 
0xFF8C00, 
0xFFFF00
};
#define maxloc 15

template<class Pixel>
void
checkRecolor(const char* prompt, Image<Pixel>* src, RegionList* allRegions)
{
    int bad = 0;
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Point p = list->get(0);
        int rid = src->getPixel(p.x, p.y);
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            if (src->getPixel(p.x, p.y) != rid) {
                printf("%s: checkrecolor fails @ (%d,%d) != %d (instead %d)\n", prompt, p.x, p.y, rid, src->getPixel(p.x, p.y));
                bad++;
            }
        }
    }
    assert(bad == 0);
}

template<class Pixel>
Image<int>*
recolor(Image<Pixel>* src, int expectedRegions)
{
  // init to white and black
  int colordiff = 0xfffff3/expectedRegions;
  //printf("regs=%d, colordiff = %08x\n", expectedRegions, colordiff);
  Image<int>* regions = new Image<int>(src->getWidth(), src->getHeight());
  for (int y=0; y<src->getHeight(); y++) {
    for (int x=0; x<src->getWidth(); x++) {
        if (src->getPixel(x,y) == 0)
            regions->setPixel(x, y, 0);
        else
            regions->setPixel(x, y, 1);
    }
  }
  if (1) {
      int regionid = 0x800000;
      int numreg = 0;
      for (int y=0; y<regions->getHeight(); y++) {
          for (int x=0; x<regions->getWidth(); x++) {
              if (regions->getPixel(x, y) == 1) {
                  // find and label this region
                  regions->labelRegion(x, y, 1, regionid);
                  regionid += colordiff;
                  numreg++;
              }
          }
      }
  } else {
      int regionid = 0;
      int numreg = 0;
      for (int y=0; y<regions->getHeight(); y++) {
          for (int x=0; x<regions->getWidth(); x++) {
              if (regions->getPixel(x, y) == 1) {
                  // find and label this region
                  regions->labelRegion(x, y, 1, listofcolors[regionid]);
                  regionid ++;
                  if (regionid > maxloc) regionid = 0;
                  numreg++;
              }
          }
      }
  }
  return regions;
}

template<class Pixel>
Image<int>*
recolor(Image<Pixel>* src, RegionList* allRegions)
{
    int colordiff = 0xfffff3/allRegions->numRegions();
    int regionid = 0x800000;
    Image<int>* regions = new Image<int>(src->getWidth(), src->getHeight());
    regions->fill(0);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Point p = list->get(0);
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            regions->setPixel(p.x, p.y, regionid);
        }
        regionid += colordiff;
    }
    return regions;
}

const int fmmdebug = 0;

typedef struct {
    int valid;
    int min;
    int max;
} Pelem;
 
template<class Pixel>
void
checkVaspect(Image<Pixel>* img, Pixel rid, Point ul, Point lr, Pelem* columns, Pelem* rows, int delta, double minratio=10.0)
{
    int outerXboundary = (delta > 0) ? lr.x : ul.x;
    for (int y=ul.y; y<lr.y; y++) {
        int xCheckStart = (delta > 0) ? columns[y].min : columns[y].max;
        int xCheckEnd = (delta > 0) ? columns[y].max : columns[y].min;
        int alongX = 0;
        for (int x=xCheckStart; x!=xCheckEnd; x+= delta) {
            if (alongX++ > 10) break; // only check 10 pixels in at most
            Pixel c;
            bool fail = false;
            int bottom;
            for (bottom = y+1; bottom < rows[x].max; bottom++) {
                c = img->getPixel(x, bottom);
                if (c == rid) break;
                if (c == 0) continue;
                fail = true;
                break;
            }
            if (fail) continue;
            // we have a gap of black from (x,y+1) - (x, bottom-1)
            if ((bottom - y) <= 1) continue;
            // gap in Y is at least 1 pixel
            if (fmmdebug) printf("Check gap starting at (%d,%d) down to (%d,%d)\n", x, y+1, lr.x, bottom);
            int xx;
            int yy;
            double area = 0;
            int endx = x;
            for (yy=y+1; yy<bottom; yy++) {
                for (xx=x; xx != outerXboundary; xx+=delta) {
                    Pixel c = img->getPixel(xx, yy);
                    if (c == 0) {
                        area++;
                        continue;
                    }
                    if (c == rid) break;
                    fail = true;
                }
                if ((delta > 0)&&(xx > endx)) endx = xx;
                if ((delta < 0)&&(xx < endx)) endx = xx;
                //printf("Got as far as (%d,%d) maxx=%d area=%g\n", xx, yy, endx, area);
            }
            if (fail) continue;
            if ((area / (double)(bottom-(y+1))) < minratio) {
                if (fmmdebug) printf("Skipping (%d,%d) to (%d,%d) too small - %g compared to %d\n", x, y+1, endx, bottom, area, bottom-(y+1));
                continue;
            }
            // now check top and bottom are also in rid
            int tx;
            for (tx=x; tx != endx; tx+=delta) if (img->getPixel(tx, y) != rid) break;
            if (tx != endx) break;
            for (tx=x; tx != endx; tx+=delta) if (img->getPixel(tx, bottom) != rid) break;
            if (tx != endx) break;

            // gap is clear - fill it in
            if (fmmdebug) printf("Filling gap from (%d,%d) to (%d,%d) %g vs. %d\n", x, y+1, endx, bottom, area, bottom-y+1);
            for (int yy=y+1; yy<bottom; yy++) {
                for (int xx=x; xx != endx; xx+=delta) {
                    if (img->getPixel(xx, yy) == 0) img->setPixel(xx, yy, rid);
                    else break;
                }
            }
        }
    }
}

template<class Pixel>
void
checkHaspect(Image<Pixel>* img, Pixel rid, Point ul, Point lr, Pelem* columns, Pelem* rows, int delta, double minratio=10.0)
{
    int outerYboundary = (delta > 0) ? lr.y : ul.y;
    for (int x=ul.x; x<lr.x; x++) {
        int yCheckStart = (delta > 0) ? rows[x].min : rows[x].max;
        int yCheckEnd = (delta > 0) ? rows[x].max : rows[x].min;
        int alongY = 0;
        for (int y=yCheckStart; y!=yCheckEnd; y+= delta) {
            if (alongY++ > 10) break; // only check 10 pixels in at most
            Pixel c;
            bool fail = false;
            int right;
            for (right = x+1; right < columns[y].max; right++) {
                c = img->getPixel(right, y);
                if (c == rid) break;
                if (c == 0) continue;
                fail = true;
                break;
            }
            if (fail) continue;
            // we have a gap of black from (x+1,y) - (right,y)
            if ((right - x) <= 1) continue;
            // gap in X is at least 1 pixel
            if (fmmdebug) printf("Check gap starting at (%d,%d) down to (%d,%d)\n", x, y+1, lr.x, right);
            int xx;
            int yy;
            double area = 0;
            int endy = y;
            for (xx=x+1; xx < right; xx++) {
	        for (yy=y; yy != outerYboundary; yy+=delta) {
                    Pixel c = img->getPixel(xx, yy);
                    if (c == 0) {
                        area++;
                        continue;
                    }
                    if (c == rid) break;
                    fail = true;
                }
                if ((delta > 0)&&(yy > endy)) endy = yy;
                if ((delta < 0)&&(yy < endy)) endy = yy;
                //printf("Got as far as (%d,%d) maxx=%d area=%g\n", xx, yy, endx, area);
            }
            if (fail) continue;
            if ((area / (double)(right-(x+1))) < minratio) {
                if (fmmdebug) printf("Skipping (%d,%d) to (%d,%d) too small - %g compared to %d\n", x, y+1, endy, right, area, right-(x+1));
                continue;
            }
            // now check left and right are also in rid
            int ty;
            for (ty=y; ty != endy; ty+=delta) if (img->getPixel(x, ty) != rid) break;
            if (ty != endy) break;
            for (ty=y; ty != endy; ty+=delta) if (img->getPixel(right, ty) != rid) break;
            if (ty != endy) break;

            // gap is clear - fill it in
            if (fmmdebug) printf("Filling gap from (%d,%d) to (%d,%d) %g vs. %d\n", x, y+1, endy, right, area, right-x+1);
            for (int yy=y; yy != endy; yy+=delta) {
                for (int xx=x+1; xx < right; xx++) {
		    if (img->getPixel(xx, yy) == 0) img->setPixel(xx, yy, rid);
                    else break;
                }
            }
        }
    }
}

template<class Pixel>
void
fillminmax(Image<Pixel>* img, Region* list, double minaspect=10.0)
{
    Pelem columns[img->getHeight()]; // holds min and max x for each column
    Pelem rows[img->getWidth()];     // holds min and max y for each row
    Point ul(img->getWidth(), img->getHeight());
    Point lr(0, 0);

    // get min and max boundaries of region along x & y axis
    Point p = list->get(0);
    int rid = img->getPixel(p.x, p.y);
    //printf("Perimieter for %d\n", rid);
    memset(&columns, 0, sizeof(Pelem)*img->getHeight());
    memset(&rows, 0, sizeof(Pelem)*img->getWidth());
    for (RPIterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        if (!columns[p.y].valid || columns[p.y].min > p.x) columns[p.y].min = p.x; 
        if (!columns[p.y].valid || columns[p.y].max < p.x) columns[p.y].max = p.x;
        if (!rows[p.x].valid || rows[p.x].min > p.y) rows[p.x].min = p.y;
        if (!rows[p.x].valid || rows[p.x].max < p.y) rows[p.x].max = p.y;
        columns[p.y].valid = 1;
        rows[p.x].valid = 1;
        if (p.x < ul.x) ul.x = p.x; 
        if (p.x > lr.x) lr.x = p.x; 
        if (p.y < ul.y) ul.y = p.y; 
        if (p.y > lr.y) lr.y = p.y; 

    }

    if (fmmdebug) {
        printf("\nX=%d Y=%d\n", ul.x, ul.y);
        printf("    \t");
        for (int x=ul.x; x<=lr.x; x++) {
            if ((x % 10) == 0) printf("0"); else printf(" ");
        }
        printf("\n");
        for (int y=ul.y; y<lr.y; y++) {
            printf("%d\t", y);
            for (int x=ul.x; x<=lr.x; x++) {
                if ((x < columns[y].min)||(x >columns[y].max)) {
                    printf(".");
                } else {
                    if (rows[x].valid && (rows[x].min <= y) && (rows[x].max >= y)) {
                        Pixel c = img->getPixel(x, y);
                        if (c == 0) printf("."); 
                        else {
                            if (c == rid) printf("X");
                            else printf("-");
                        }
                    } else {
                        printf("!");
                    }
                }
            }
            printf("\n");
        }
        printf("\n");
    }

    // fill in all points which are bounded by min & max and other pixels in region
    for (int y=ul.y; y<lr.y; y++) {
        int lastblack = -1;
        int startgood = -1;
        int frombad = 1;
        int possiblestart = -1;
        if (fmmdebug) printf("next line (%d,%d) -> (%d,%d)\n", columns[y].min, y, columns[y].max, y);
        for (int x=columns[y].min; x<=columns[y].max; x++) {
            if (rows[x].valid && (rows[x].min <= y) && (rows[x].max >= y)) {
                if (frombad) {
                    assert(lastblack == -1);
                }
                // in a good region
                Pixel c = img->getPixel(x, y);
                if (c == 0) {
                    // black pixel, track when this starts
                    if (!frombad && (lastblack == -1)) lastblack = x;
                    if (frombad && (startgood == -1) && (possiblestart == -1)) possiblestart = x;
                } else {
                    // not black
                    if (c != rid) {
                        // a pixel in someone elses region, draw old good line
                        if (!frombad) {
                            if (lastblack == -1) lastblack = x-1;
                            if (fmmdebug) printf("A: %d to %d\n", startgood, lastblack);
                            img->drawLine(startgood, y, lastblack, y, rid);
                            startgood = -1;
                            lastblack = -1;
                        } 
                        // coming from bad region, no possible start due to not black and not in rid
                        possiblestart = -2;
                        frombad = 1;
                    } else {
                        // this is a pixel from our region
                        assert(c == rid);
                        if (startgood == -1) {
                            if (possiblestart > 0) {
                                startgood = possiblestart;
                                possiblestart = -2;
                            } else
                                startgood = x; 
                        }
                        lastblack = -1;
                        frombad = 0;
                    }
                }
            } else {
                // not in the good region
                if (startgood >= 0) {
                    if (lastblack == -1) lastblack = x-1;
                    lastblack = x-1;
                    if (fmmdebug) printf("B: %d to %d\n", startgood, lastblack);
                    img->drawLine(startgood, y, lastblack, y, rid);
                }
                startgood = -1;
                lastblack = -1;
                frombad = 1;
                possiblestart = -1;
            }
        }
        if (startgood > 0) {
            if (fmmdebug)             printf("C: %d to %d\n", startgood, columns[y].max);
            img->drawLine(startgood, y, columns[y].max, y, rid);
        }
    }

    if (fmmdebug) {
        printf("\nX=%d Y=%d\n", ul.x, ul.y);
        printf("    \t");
        for (int x=ul.x; x<=lr.x; x++) {
            if ((x % 10) == 0) printf("0"); else printf(" ");
        }
        printf("\n");
        for (int y=ul.y; y<lr.y; y++) {
            printf("%d\t", y);
            for (int x=ul.x; x<=lr.x; x++) {
                if ((x < columns[y].min)||(x >columns[y].max)) {
                    printf(".");
                } else {
                    if (rows[x].valid && (rows[x].min <= y) && (rows[x].max >= y)) {
                        Pixel c = img->getPixel(x, y);
                        if (c == 0) printf("."); 
                        else {
                            if (c == rid) printf("X");
                            else printf("-");
                        }
                    } else {
                        printf("!");
                    }
                }
            }
            printf("\n");
        }
        printf("\n");
    }

    checkVaspect(img, rid, ul, lr, columns, rows, 1, minaspect);
    checkVaspect(img, rid, ul, lr, columns, rows, -1, minaspect);
    checkHaspect(img, rid, ul, lr, columns, rows, 1, minaspect);
    checkHaspect(img, rid, ul, lr, columns, rows, -1, minaspect);
}

template<class Pixel>
void
checkBlackRegion(int from, int to, int origx, int direc, Image<Pixel>* img, Pixel rid)
{
    // do walking square from (x,from) to (x, to) and record area
    // if area/(to-from) > 3 we fill this region

    int opening = to-from;
    int area = 0;
    bool fail = false;
    int x;
    for(x=origx; x<img->getWidth(); x++) {
        int startblack;
        bool hadblack = false;
        for (int y=from; y<=to; y++) {
            Pixel c = img->getPixel(x, y);
            if (c == 0) {
                hadblack = true;
                startblack = y;
                continue;
            }
            if (c == rid) {
                if (!startblack) continue;
                area += y-startblack;
                break;
            }
            fail = true;
            break;
        }
        if (fail) break;
        if (!hadblack) {
            // ran into a wall of rid. we are done here
            break;
        }
    }
    if (x == img->getWidth()) return;
    if (fail) return;
    // we had a black region
    if (((double)area/(double)(to-from)) > 3) {
        // we should color this in
        img->drawLine(origx, from, origx, to, rid);
    }
}

template<class Pixel>
void
aspectfill(Image<Pixel>* img, Region* list)
{
    enum AFstate { OUT, IN, BLACK, CHECK };
    Point p = list->get(0);
    Pixel rid = img->getPixel(p.x, p.y);
    printf("start for %d\n", rid);
    for (int x=0; x<img->getWidth(); x++) {
        AFstate state = OUT;
        int start = 0;
        for (int y=0; y<img->getHeight(); y++) {
            Pixel c = img->getPixel(x, y);
            switch (state) {
            case OUT:
                if (c == rid) state = IN;
                break;

            case IN:
                if (c == 0) {
                    state = BLACK;
                    start = y-1;
                } else 
                    if (c != rid) state = OUT;
                break;

            case BLACK:
                if (c == 0) break;
                if (c == rid) {
                    // just re-entered region, see if those blacks should go
                    printf("Check (%d,%d) -> (%d,%d)\n", x, start, x, y);
                    checkBlackRegion(start, y, x, 1, img, rid);
                    checkBlackRegion(start, y, x, -1, img, rid);
                    state = IN;
                } else {
                    state = OUT;
                }
                break;
            }
        }
    }
}

#if 0

// use barycentric coordinates (see http://www.blackpawn.com/texts/pointinpoly/)
bool 
stillValid(Point a, Point b, Point c, Image<int>* img, int rid)
{
    // Compute vectors and dot products that remain constant
    Point v0 = c-a;
    Point v1 = b-a;
    double dot00 = v0.dot(v0);
    double dot01 = v0.dot(v1);
    double dot11 = v1.dot(v1);
    double invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    // computer region we have to test
    Point ul = a;
    Point lr = a;
    if (b.x < ul.x) ul.x = b.x;
    if (b.y < ul.y) ul.y = b.y;
    if (b.x > lr.x) lr.x = b.x;
    if (b.y > lr.y) lr.y = b.y;
    if (c.x < ul.x) ul.x = c.x;
    if (c.y < ul.y) ul.y = c.y;
    if (c.x > lr.x) lr.x = c.x;
    if (c.y > lr.y) lr.y = c.y;
    // now check every point in the rectangle defined ul and lr.  If it is in abc triangle, make sure it is black or rid.  If so, we can zap b.
    for (int y=ul.y; y<=lr.y; y++) {
        for (int x=ul.x; x<= lr.x; x++) {
            // compute points and dot products for this point
            Point p(x, y);
            Point v2 = p-a;
            double dot02 = v0.dot(v2);
            double dot12 = v1.dot(v2);
            // now u & v
            double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
            double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
            // Check if point is in triangle
            if ((u >= 0) && (v >= 0) && (u + v < 1)) {
                // p is in triangle
                int c = img->getPixel(p.x, p.y);
                if (c != 0) return false;
            }
        }
    }
    return true;
}
#endif
        
// search from start in direction of delta til we find a black pixel
// followed only by black or RID return the point of that black point.
// If no black points exist that fulfill the condition, return the
// point of RID.  If no RID, error.
template<class Pixel>
Point 
findFirstGoodBlack(const Point& start, const Point& delta, int rid, const Image<Pixel>* img)
{
    int x = start.x;
    int y = start.y;
    bool found = false;
    Point p;
    for (; 1; x+=delta.x, y+=delta.y) {
        int c = img->getPixel(x, y);
        if (c == 0) {
            if (!found) {
                found = true;
                p = Point(x, y);
            }
        } else if (c == rid) {
            // we found rid
            if (found) return p;
            // we never had a black pixel, so return here
            return Point(x,y);
        } else {
            found = false;
        }
    }
    assert(0);
}

int rdebug=0;

struct DirectPoint {
    Point p;
    Point delta;
} ;

// search in direction of delta for ALL points on line segment being black. If so, adjust
template<class Pixel>
bool
moveIfBlackOnly(DirectPoint& a, DirectPoint& b, const Image<Pixel>*& img)
{
    Point direc(0,0);
    Point start = a.p;
    Point end = b.p;
    Point delta(0,0);
    if (a.p.x == b.p.x) {
        // push right or left
        if (a.p.y == b.p.y) return false; // singleton point, nothing to do.
        if (a.delta.x) direc.x = a.delta.x; else direc.x = b.delta.x;
        if (start.y > end.y) {
            start = b.p;
            end = a.p;
        }
        delta.y = 1;
    } else if (a.p.y == b.p.y) {
        // push up or down
        if (a.delta.y) direc.y = a.delta.y; else direc.y = b.delta.y;
        if (start.x > end.x) {
            start = b.p;
            end = a.p;
        }
        delta.x = 1;
    } else
        assert(0);
    for (int d=1; d<1000; d++) {
        bool ok=true;
        for (int x=start.x, y=start.y; (x != end.x) || (y != end.y); x+=delta.x, y+=delta.y) {
            if (img->getPixel(x+direc.x*d, y+direc.y*d) != 0) {
                ok=false;
                break;
            }
        }
        if (!ok) {
            if (--d > 0) {
                // move points over by d
                a.p.x += direc.x*d;
                a.p.y += direc.y*d;
                b.p.x += direc.x*d;
                b.p.y += direc.y*d;
                return true;
            }
            return false;
        }
    }
    assert(0);
}

// search from (startx,starty) in direction of delta to at most stop.  If (test and find rid) || (!rid) return true, otherwise return false
template<class Pixel>
bool
nonBlack(int startx, int starty, const Point& delta, const Point& stop, Pixel rid, bool test, const Image<Pixel>*& img)
{
    int stoploc = stop.x*delta.x + stop.y*delta.y;
    
    if (rdebug > 2) printf("(%d,%d)+(%d,%d)+%s", startx, starty, delta.x, delta.y, test ?"rid?":"!rid?");
    bool ret = false;
    for (; (startx*delta.x+starty*delta.y) <= stoploc; startx += delta.x, starty += delta.y) {
        Pixel c = img->getPixel(startx, starty);
        if (c == 0) continue;
        if (c == rid) {
            ret = test;
            break;
        }
        if (c != rid) {
            ret = !test;
            break;
        }
    }
    if (rdebug > 2) printf("->%s\t", (ret?"STOP":"more"));
    return ret;
}

bool 
isIntersecting(Point p0, Point p1, Point p2, Point p3, Point& intpoint)
{
    double s1_x, s1_y, s2_x, s2_y;
    s1_x = p1.x - p0.x;     s1_y = p1.y - p0.y;
    s2_x = p3.x - p2.x;     s2_y = p3.y - p2.y;

    double s, t;
    s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
    t = ( s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
        // Collision detected
        intpoint.x = p0.x + (t * s1_x);
        intpoint.y = p0.y + (t * s1_y);
        return true;
    }
    return false;
}

// check that a 3 pixel band is black or !rid from sx,sy to dx,dy
template<class Pixel>
bool
checkSwatch(const Image<Pixel>* regions, int sx, int sy, int dx, int dy, int rid)
{
    if (sy==dy) {
        int start=sy-1; if (start < 0) start=0;
        int end=sy+1; if (end >= regions->getHeight()) end=sy;
        while (sx <= dx) {
            for (int j=start; j<=end; j++) {
                Pixel c = regions->getPixel(sx,j);
                if ((c != 0)&&(c == rid)) return false;
            }
            sx++;
        }
        return true;
    } else {
        assert(sx == dx);
        int start=sx-1; if (start < 0) start=0;
        int end=sx+1; if (end >= regions->getWidth()) end=sx;
        while (sy <= dy) {
            for (int j=start; j<=end; j++) {
                Pixel c = regions->getPixel(j,sy);
                if ((c != 0)&&(c == rid)) return false;
            }
            sy++;
        }
        return true;
    }
}

template<class Pixel>
void
rectizeRegion(Region* list, const Image<Pixel>* regions, std::vector<Point>& boundary)
{
    Image<Pixel>* temp = new Image<Pixel>(regions->getWidth(), regions->getHeight());
    temp->fill(0);
    const Point DOWN(0, 1);
    const Point UP(0, -1);
    const Point RIGHT(1, 0);
    const Point LEFT(-1, 0);
    const Point makeInside(1, -1);
    const Point makeOutside(-1, 1);
    const Point turnBumpRID(-1, 1); // i.e., turn to the outside
    const Point turnFallOff(1, -1); // i.e., turn to the inside

    if (list->size() < 10) return;

    Point ul=list->getUL();
    Point lr=list->getLR();
    int rid = list->id;

    //if (rid == 2133)         rdebug=3;
    if (rdebug) printf("--- %d (%d,%d) (%d,%d)\n", rid, ul.x, ul.y, lr.x, lr.y);

    // prepare region, by connecting any !rid's that are only separated by black space
    // and connect any !rids to outside if entirely in the BB
    // and ENSURE that !rid-rid boundary has at least one black between them
    for (int y=ul.y; y<lr.y; y++) {
        int start = ul.x;
        int notrid;
        bool fromnotrid = false;
        for (int x=ul.x; x<lr.x; x++) {
            Pixel c = regions->getPixel(x, y);
            if (c == 0) {
                fromnotrid = false;
                continue;
            }
            temp->setPixel(x, y, c);
            if (c == rid) {
                start = -1;
                if (fromnotrid) {
                    temp->setPixel(x-1, y, 0);
                }
                fromnotrid = false;
            }
            if (c != rid) {
                if (start != -1) {
                    if (checkSwatch(regions, start, y, x, y, rid) && checkSwatch(temp, start, y, x, y, rid))
                        temp->drawLine(start-1, y, x, y, c);
                }
                notrid = c;
                start = x;
                fromnotrid = true;
            }
        }
        if (start != -1) {
            if (checkSwatch(regions, start, y, lr.x, y, rid) && checkSwatch(temp, start, y, lr.x, y, rid))
                temp->drawLine(start-1, y, lr.x+1, y, notrid);
        }
    }
    for (int x=ul.x; x<lr.x; x++) {
        int start = ul.y;
        int notrid;
        bool fromnotrid = false;
        for (int y=ul.y; y<lr.y; y++) {
            Pixel c = temp->getPixel(x, y);
            if (c == 0) {
                fromnotrid = false;
                continue;
            }
            if (c == rid) {
                start = -1;
                if (fromnotrid) {
                    temp->setPixel(x, y-1, 0);
                }
                fromnotrid = false;
            }
            if (c != rid) {
                if (start != -1) {
                    if (checkSwatch(regions, x, start, x, y, rid) && checkSwatch(temp, x, start, x, y, rid))
                        temp->drawLine(x, start-1, x, y, c);
                }
                notrid = c;
                start = y;
                fromnotrid = true;
            }
        }
        if (start != -1) {
            if (checkSwatch(regions, x, start, x, lr.y, rid) && checkSwatch(temp, x, start, x, lr.y, rid))
                temp->drawLine(x, start-1, x, lr.y+1, notrid);
        }
    }
    // and now make sure at least one black pixel between rid and nonrid
    for (int y=ul.y; y<lr.y; y++) {
        bool fromrid=false;
        int x;
        for (x=ul.x; x<lr.x; x++) {
            Pixel c = temp->getPixel(x, y);
            if (c == 0) {
                fromrid = false;
            } else if (c == rid) {
                fromrid = true;
            } else {
                if (fromrid)
                    temp->setPixel(x, y, 0);
                fromrid = false;
            }
        }
        if (fromrid && (x < regions->getWidth()))
            temp->setPixel(x, y, 0);
    }
    for (int x=ul.x; x<lr.x; x++) {
        bool fromrid=false;
        int y;
        for (y=ul.y; y<lr.y; y++) {
            Pixel c = temp->getPixel(x, y);
            if (c == 0) {
                fromrid = false;
            } else if (c == rid) {
                fromrid = true;
            } else  {
                if (fromrid)
                    temp->setPixel(x, y, 0);
                fromrid = false;
            }
        }
        if (fromrid && y < regions->getHeight())
            temp->setPixel(x, y, 0);
    }


    if (rdebug) {
        Image<Pixel>* out = new Image<Pixel>(temp);
        for (int x=ul.x; x<lr.x; x++) {
            int start = -1;
            for (int y=ul.y; y<lr.y; y++) {
                Pixel c = out->getPixel(x, y);
                if (c == rid) c = 0x0ff0000;
                else if (c != 0) c = 0x008000 | c;
                out->setPixel(x, y, c);                
            }
        }
        
        out->PPMout(imageFilename("src", "ppm"));    
        delete out;
    }
    
    if (0) {
        if (rid == 841) 
            rdebug = 1;
        else
            rdebug = 0;
    }
    // get set up.  start by moving down from first point after !rid from ul in +x direction and 
    Point state = DOWN;
    Point startPoint = findFirstGoodBlack(ul, RIGHT, rid, temp);
    Point lastPoint = lr;
    Point endPoint(startPoint.x, startPoint.y);
    bool takeLastMax = true;
    bool prevFellOff = true;
    while (1) {
        // calculate inside and outside directions
        Point inside(state.y*makeInside.x, state.x*makeInside.y);
        Point outside(state.y*makeOutside.x, state.x*makeOutside.y);
        //Point searchDirection(state.y*prevTurn.x, state.x*prevTurn.y);
        Point searchDirection(state.y*makeInside.x, state.x*makeInside.y);
        int endSearch = (lastPoint.x*searchDirection.x+lastPoint.y*searchDirection.y);
        if (rdebug) {
            printf("Currently: [");
            for (int i=0; i<boundary.size(); i++) {
                Point p = boundary[i];
                printf(" (%d,%d) ", p.x, p.y);
            }
            printf(" ]\nSearch from (%d,%d) towards (%d,%d) by searching in direction:(%d,%d) taking %s max stop@(%d)\n", 
                   startPoint.x, startPoint.y, state.x, state.y, 
                   searchDirection.x, searchDirection.y, takeLastMax ? "last" : "first",
                   endSearch);
        }

        // start searching for longest line we can add
        int maxlen=0;
        Point maxpoint;
        int nextTurn;
        for (int y=startPoint.y, x=startPoint.x; 
             ((y>=ul.y)&&(y<=lr.y))&&((x>=ul.x)&&(x<=lr.x));
             //(y*searchDirection.y+x*searchDirection.x)<=endSearch; 
             y+=searchDirection.y, x+=searchDirection.x) {
            Pixel c;
            int sx, sy; 
            bool fellOff = true;
            bool bump = false;
            if (rdebug > 2) printf("\t\tpos:(%d,%d):", x, y);
            int len = 1;
            for (sx=x+state.x, sy=y+state.y; ((sy>=ul.y)&&(sy<=lr.y))&&((sx>=ul.x)&&(sx<=lr.x)); sx+=state.x, sy+=state.y) {
                // stop going if:
                // - !rid inside
                // - rid outside
                // - run into !rid
                c = temp->getPixel(sx, sy);

                // run into !rid?
                if ((c != 0)&&(c != rid)) {
                    if (rdebug > 2) printf("c = %d\n", c);
                    fellOff = false;
                    break;
                }

                // search on the inside of our vector for !rid
                bool ok = true;
                for (int inx=sx+inside.x, iny=sy+inside.y; 
                     ((iny>=ul.y)&&(iny<=lr.y))&&((inx>=ul.x)&&(inx<=lr.x));
                     inx+=inside.x, iny+=inside.y) {
                    Pixel c = temp->getPixel(inx, iny);
                    if (c == 0) continue;
                    if (c == rid) break;
                    if (rdebug > 2) printf("stop inside(%d,%d) @ (%d,%d) with c = %d\n", inside.x, inside.y, inx, iny, c);
                    ok = false; // c == !rid, have to stop
                    break;
                }
                if (ok) {
                    // passed the inside test, now do the outside test for rid
                    for (int outx=sx+outside.x, outy=sy+outside.y; 
                         ((outy>=ul.y)&&(outy<=lr.y))&&((outx>=ul.x)&&(outx<=lr.x));
                         outx+=outside.x, outy+=outside.y) {
                        Pixel c = temp->getPixel(outx, outy);
                        if (c == 0) continue;
                        if (c != rid) break;
                        if (rdebug > 2) printf("stop outside(%d,%d) @ (%d,%d) with c = %d\n", outside.x, outside.y, outx, outy, c);
                        bump = true;
                        ok = false; // c == rid, have to stop
                        break;
                    }
                }
                if (!ok) {
                    fellOff = false;
                    break;
                }
                len++;          // keep going
            }
            if ((rdebug > 2)&&fellOff) printf("\n");
            // we have reached a stopping point, calculate the length
            // of the good portion of the vector from startpoint to
            // here
            if (fellOff) len--;
            if (bump && (len == 1) && (c == rid)) {
                // we started search on a pixel of rid, so we stop searching completely
                assert(maxlen > 0);
                if (rdebug) printf("Bumped into rid from @ (%d,%d)\n", x, y);
                break;
            }
            if ((len > maxlen)||(takeLastMax && (len==maxlen))) {
                // capture this point as a possible start of the vector
                maxlen = len;
                maxpoint = Point(x, y);
                if (bump) nextTurn=-1; else nextTurn = 1;
                if (rdebug) printf("\tnewmax:%d from:(%d,%d) will turn:%d to (%d,%d)\n", 
                                   len, x, y, nextTurn, searchDirection.x*nextTurn, searchDirection.y*nextTurn);
            }
            //if ((c == rid)&&(len == 0)&&takeLastMax) break;
        }
        assert(maxlen != 0);
        if (rdebug) printf("Add (%d,%d)\n", maxpoint.x, maxpoint.y);
        lastPoint = maxpoint;
        boundary.push_back(maxpoint);
        // check to see if we are done
        if (boundary.size() > 2) {
            Point p = boundary[0];
            if ((p.x == maxpoint.x)&&(p.y == maxpoint.y)) break;
        }
        // now get ready for next search
        startPoint = Point(maxpoint.x+state.x*maxlen, maxpoint.y+state.y*maxlen);
        if ((startPoint.x == endPoint.x) && (startPoint.y == endPoint.y)) break;
        // we have more to do, so update turn and state info
        takeLastMax = (nextTurn > 0) ? true : false;
        //state = Point(state.y*nextTurn.x, state.x*nextTurn.y);
        state = Point(searchDirection.x*nextTurn, searchDirection.y*nextTurn);
    }
    delete temp;
}

template<class Pixel>
Image<Pixel>* 
rectize(const Image<Pixel>* regions, RegionList* allRegions)
{
    Image<Pixel>* outline = new Image<int>(regions);
    int colordiff = 0xfffff3/allRegions->numRegions();
    int regionid = 0x800000;

    // now for each region make smallest rectilinear filled region
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        list->boundary.clear();
        rectizeRegion(list, regions, list->boundary);

        if (rdebug) printf("Complete Boundary for %d: ", list->id);
        if (list->boundary.size() > 0) {
            Point last = list->boundary[0];
            outline->setCircle(last.x, last.y, 0x0ffffff, 5);
            for (int i=0; i<list->boundary.size(); i++) {
                Point p = list->boundary[i];
                if (rdebug) printf(" (%d,%d)  ", p.x, p.y);
                outline->drawLine(last.x, last.y, p.x, p.y, regionid, 5);
                outline->setCircle(p.x, p.y, 0x0ffffff, 5);
                last = p;
            }
            Point p = list->boundary[0];
            outline->drawLine(last.x, last.y, p.x, p.y, regionid, 5);
        }
        if (rdebug) printf("\n");
        if (rdebug) outline->PPMout(imageFilename("subrect", "ppm"));    
        else {printf("%d ", list->id);fflush(stdout);}
        regionid+=colordiff;
    }
    printf("\n");
    return outline;
}

template<class Pixel>
void
checkadj(const char* prompt, RegionList* allRegions, Image<Pixel>* regions)
{
    assert(0);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Point p = list->get(0);
        int y = p.y;
        int x = p.x;
        int rid = regions->getPixel(x, y);
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            for (int i=0; i<8; i++) {
                int xx, yy;
                if (regions->getNeighbor(p.x, p.y, i, xx, yy)) {
                    Pixel c = regions->getPixel(xx, yy);
                    if ((c == 0)||(c == rid)) continue;
                    printf("checkadj:%s:Neighbor of %d around (%d,%d) is %d\n", prompt, rid, p.x, p.y, c);
                }
            }
        }
    }
}

template<class Pixel>
void
checkmatch(const char* prompt, RegionList* allRegions, Image<Pixel>* regions)
{
    int fail = false;
    int cnt = 0;
    Image<Pixel>* checker = new Image<int>(regions);
    for (int i=0; i<allRegions->size(); i++) {
        bool show=false;
        Region* list = allRegions->get(i);
        if (list == NULL) continue;
        if (list->id != i) {
            printf("Bad id %d != %d\n", list->id, i);
            assert(0);
        }
        if (0) {
            if (list->size() < 2) {
                printf("Region %d is of size < 2\n", list->id);
                show = true;
            }
        }
        cnt++;
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            if (show) printf("\t(%d,%d)\n", p.x, p.y);
            if (checker->getPixel(p.x, p.y) != i) {
                printf("%s (%d, %d) should have %d and has %d\n", prompt, p.x, p.y, i, checker->getPixel(p.x, p.y));
                fail = true;
            }
            else 
                checker->setPixel(p.x, p.y, -1);
        }
    }
    if (cnt != allRegions->numRegions()) printf("%s: Found %d non-null regions out of %d\n", prompt, cnt, allRegions->numRegions());
    for (int y=0; y<checker->getHeight(); y++) {
        for (int x=0; x<checker->getWidth(); x++) {
            if ((checker->getPixel(x, y) != 0)&&(checker->getPixel(x, y) != -1)) {
                printf("%s (%d,%d) isn't 0 -> %d\n", prompt, x, y, checker->getPixel(x, y));
                fail = true;
            }
        }
    }
    delete checker;
    assert(!fail);
}

template<class Pixel>
void
getHorzBounds(Image<Pixel>* img, int yy, Pixel c, int& minx, int& maxx)
{
    minx = img->getWidth();
    maxx = 0;
    for (int x = 0; x<img->getWidth(); x++) {
        if (img->getPixel(x, yy) == c) {
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
        }
    }
}

const int fbtdebug = 0;
const int blockingTileSize = 45;

Font* a12;

char*
imageFilename(const char* temp, const char* ext)
{
    static int id=0;
    static char filename[256];
    sprintf(filename, "%04d-%s.%s", id++, temp, ext);
    if ((id-1) < startImageSaving) {
        printf("Not Generating: %s\n", filename);
        return NULL;
    }

    printf("Generating: %s\n", filename);
    return filename;
}

void
saveimage(const char* prompt, Image<int>* regions, int numreg, RegionList* allRegions, bool check=true) 
{
    static double sizes[] = { 0.2,  0.3,  0.35, 0.4,  0.4,  .45,  0.5,  0.5, .6,   1,   1.2,  1.3, 1.3, 1.3, 1.3,     2,  2.5, 3,  6 };
    static int sizecuts[] = { 4000, 3000, 2000, 1500, 1000, 750,  700,  500, 375,  250, 200, 100,  75,  50,  40,   30,  20,  0, -100 };

    if (!si) return;
    char* filename = imageFilename(prompt, "ppm");
    if (filename == NULL) return;
    Image<int>* newregions = recolor(regions, allRegions);
    newregions->PPMout(filename);

    if (check) {
        checkmatch(prompt, allRegions, regions);
        checkRecolor(prompt, newregions, allRegions);
    }

    if (allRegions != NULL) {
        int i;
        for (i=0; allRegions->numRegions() < sizecuts[i]; i++) ;
        double fs = sizes[i];
        for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
            Region* list = allRegions->current(); // list of points in this region
            if (list == NULL) continue;
            list->label(newregions, a12, fs);
        }
        newregions->PPMout(imageFilename(prompt, "label.ppm"));
    }
    delete newregions;
}


void
modifyBlocker(Image<unsigned char>* blocker, Image<int>* tiled)
{
    for (int y=0; y<tiled->getHeight(); y++) {
        for (int x=0; x<tiled->getWidth(); x++) {
            unsigned int b = blocker->getPixel(x, y);
            if (tiled->getPixel(x, y) == 0x00ffffff) {
                b = 255;
            } else {
                b = b/2;
            }
            if (b > 255) b=255;
            blocker->setPixel(x, y, (unsigned char)b);
        }
    }
}

static const int WHITE = 0x0ffffff;
static const int RED = 0x0ff0000;
static const int YELLOW = 0x0ffff00;
static const int GREEN = 0x000ff00;

void calcTileLengths(Image<int>* tiled, Image<int>& lengths, int dx, int dy);
void PotentiallyFillGap(int x, int y, int dx, int dy, Image<int>* tiled, const Image<unsigned char>& check, const Image<int>& lengths);

void
calcTileLengths(Image<int>* tiled, Image<int>& lengths, int dx, int dy)
{
    int width = lengths.getWidth();
    int height = lengths.getHeight();
    lengths.fill(0);
    // get the real lengths
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            if ((tiled->getPixel(x, y) == WHITE)&&(lengths.getPixel(x,y)==0)) {
                // we reached a tiled pixel which hasn't been calculated
                int xx, yy;
                for (xx=x, yy=y; (xx<width)&&(yy<height); xx+=dx, yy+=dy) 
                    if (tiled->getPixel(xx, yy) != WHITE) break;
                int len = (xx-x)+(yy-y);
                xx -= dx;
                yy -= dy;
                for (int ax=x, ay=y; (ax<=xx)&&(ay<=yy); ax+=dx, ay+=dy)  lengths.setPixel(ax, ay, len);
            }
        }
    }
    // now modify based on adjacent ones
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            int blen = lengths.getPixel(x,y) & 0x0ffff;
            if (blen == 0) continue;
            // now look along the blen pixels in this line left&right
            // (or up&down) upto 10 pixels in each direction for the
            // lengths of adjacant tile lines.  If any are longer than
            // this one, add in their length/(2^dist) to this line.

            // calculate bounds of the search along the ortho direction
            int startx = x-(10*dy);
            int starty = y-(10*dx);
            if (startx < 0) startx=0;
            if (starty < 0) starty=0;
            int endx = x+(10*dy);
            int endy = y+(10*dx);
            if (endx >= width) endx = width-1;
            if (endy >= height) endy = height-1;
            int liney, linex;
            int maxadd = 0;
            // now search along the line of length blen in the + and -  ortho direction for longer tile lines
            for (liney=y, linex=x; (liney<(y+(blen*dy)))&&(linex<(x+(blen*dx))); liney+=dy, linex+=dx) {
                int add=0;
                for (int dist=1; dist<10; dist++) {
                    int cx = linex+(dy*dist);
                    int cy = liney+(dx*dist);
                    if ((cx > endx)||(cy > endy)) break;
                    int adjlen=lengths.getPixel(cx, cy) & 0x0ffff;
                    if (adjlen == 0) break;
                    if (adjlen > blen) add += (adjlen >> dist);
                }
                for (int dist=1; dist<10; dist++) {
                    int cx = linex-(dy*dist);
                    int cy = liney-(dx*dist);
                    if ((cx < startx)||(cy < starty)) break;
                    int adjlen=lengths.getPixel(cx, cy) & 0x0ffff;
                    if (adjlen == 0) break;
                    if (adjlen > blen) add += (adjlen >> dist);
                }
                if (add > maxadd) maxadd = add;
            }
            // maxadd has the amount we would add to all these pixels in the tile
            if (maxadd > 0) {
                int liney, linex;
                for (liney=y, linex=x; (liney<=(y+(blen*dy)))&&(linex<=(x+(blen*dx))); liney+=dy, linex+=dx) {
                    lengths.setPixel(linex, liney, blen+(maxadd<<16));
                }
            }
            x += (blen-1);
        }
    }
    // now that we have calculated how much to add to each tile, do it by adding the high order bits into the low order bits
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            int blen = lengths.getPixel(x,y);
            int addlen = blen >> 16;
            if (addlen == 0) continue;
            lengths.setPixel(x, y, addlen+(blen &0x0ffff));
        }
    }
}


void
PotentiallyFillGap(int x, int y, int dx, int dy, Image<int>* tiled, const Image<unsigned char>& check, const Image<int>& lengths)
{
    int width = tiled->getWidth();
    int height = tiled->getHeight();
    int maxGapX = width/4;
    int maxGapY = height/4;

    if (tiled->getPixel(x-dx,y-dy) == WHITE) {
        int xx=x;
        int yy=y;
        while ((xx<width)&&(yy<height)) {
            if (check.getPixel(xx, yy) != 1) break;
            if (tiled->getPixel(xx, yy) == WHITE) break;
            xx += dx;
            yy += dy;
        }

        int newcolor = GREEN; // color we will use to make gap

        if (((xx-x) < maxGapX)&&((yy-y) < maxGapY)) {
            // this gap (xx-x) or (yy-y) is a candidate for adding to the
            // tile.  See if the white (or red) on either
            // side is longer than the gap
            int gap = ((xx-x)+(yy-y));
            if (gap == 0) return;
            int surround = 0;
            
            // calculate amount of tile to left or above
            int left = x-dx;
            int up = y-dy;
            if (0) {
                while ((left > 0)&&(up>0)) {
                    int tc = tiled->getPixel(left,up);
                    if ((tc != WHITE)&&(tc != RED)) break;
                    left -= dx;
                    up -= dy;
                }
            }
            surround = lengths.getPixel(left, up);
            // calculate amount of tile to right or below
            int right = xx;
            int down = yy;
            if (0) {
                while ((right < width)&&(down < height)) {
                    int tc = tiled->getPixel(right,down);
                    if ((tc != WHITE)&&(tc != RED)) break;
                    right += dx;
                    down += dy;
                }
                int surround = ((x-left) + (right-xx))+((y-up) + (down-yy));
            }
	    if ((right < (width))&&(down < (height)))
                surround += lengths.getPixel(right, down);
            // now if surround > gap, we should add it back in
            if (surround > gap) {
                newcolor = WHITE;
            } 
            //printf("(%d,%d) + (%d,%d) -> (%d, %d) surround: %d, gap: %d\n", x, y, dx, dy, xx, yy, surround, gap, (newcolor == GREEN)?"FAIL":"SUCCEED");
        }
        // if we succeeded newcolor will be set to white
        while ((x <= xx)&&(y <= yy)) {
            tiled->setPixel(x, y, newcolor);
            x += dx;
            y += dy;
        }
    }
}



template<class Pixel>
Image<Pixel>* 
findBlockingTiles(Image<Pixel>* regions, Image<Pixel>* prevTile, RegionList* allRegions)
{
    // try to tile regions to discover
    int tsize = blockingTileSize;
    int reqtiles = 35;
    int numreg = allRegions->numRegions();
    Image<int>* tiled = recolor(regions, allRegions);
    checkRecolor("fbt", tiled, allRegions);
    const int WHITE = 0x0ffffff;
    const int RED = 0x0ff0000;
    const int GREEN = 0x000ff00;
    const int BLUE = 0x0000ff;

    if (0) {
        // first blur all regions
        for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
            Region* list = allRegions->current(); // list of points in this region
            if (list == NULL) continue;
            Point p = list->get(0);
            int y = p.y;
            int x = p.x;
            int rid = tiled->getPixel(x, y);
            for (RPIterator it = list->begin(); it != list->end(); ++it) {
                Point p = *it;
                if (1) {
                    for (int i=-2; i<2; i++) {
                        for (int j=-2; j<2; j++) {
                            int xx = p.x+i;
                            int yy = p.y+j;
                            if ((xx < 0)||(xx >= tiled->getWidth())) continue;
                            if ((yy < 0)||(yy >= tiled->getHeight())) continue;
                            tiled->setPixel(xx, yy, rid);
                        }
                    }
                } else {
                    for (int n=0; n<8; n++) {
                        int xx, yy;
                        if (tiled->getNeighbor(p.x, p.y, n, xx, yy))
                            tiled->setPixel(xx, yy, rid);
                    }
                }
            }
        }
    }
    // now find tiles
    for (int y=0; y<prevTile->getHeight(); y++) {
        for (int x=0; x<prevTile->getWidth(); x++) {
            if (prevTile->getPixel(x, y) != WHITE) continue;
            bool found = true;
            int offset = 1;
            for (int doit=0; found && (doit<2); doit++) {
                for (int i=0; i<tsize; i++) {
                    if (y+i >= tiled->getHeight()) break;
                    for (int j=0; j<tsize; j++) {
                        if (x+j >= tiled->getWidth()) break;
                        if (doit) {
                            tiled->setPixel(x+j, y+i, WHITE);
                        } else {
                            int c = tiled->getPixel(x+j, y+i);
                            if ((c != 0)&&(c != WHITE)||(prevTile->getPixel(x, y) != WHITE)) {
                                if (j > offset) offset = j;
                                found = false;
                                break;
                            }
                        }
                    }
                    if (!found) break;
                }
            }
            if (found) x+=(tsize>>2);
            else x+= (offset-1);
        }
    }
    if (si) tiled->PPMout(imageFilename("before-tiled", "ppm"));
    // now we eliminate all white regions with an area of less than reqtiles*tsize^2
    // and we eliminate all parts of the tile that are outside of the core
    Image<unsigned char> check(tiled->getWidth(), tiled->getHeight());
    check.fill(0);
    int maxkillX = tiled->getWidth()/10;
    int maxkillY = tiled->getHeight()/10;
    for (int y=0; y<tiled->getHeight(); y++) {
        for (int x=0; x<tiled->getWidth(); x++) {
            if (tiled->getPixel(x, y) == WHITE) {
                // we just entered a tiled region, turn all pixels to 1
                Region* points = tiled->labelAndReturnRegion(x, y, WHITE, 1);
                // check various conditions to eliminate this region, or parts of it from the tile

                if (points->size() < reqtiles*(tsize*tsize)) {
                    // too small
                    for (RPIterator it = points->begin(); it != points->end(); ++it) {
                        Point p = *it;
                        tiled->setPixel(p.x, p.y, RED);
                    }
                } else if (1) {
                    int debug = 0;
                    // get bounding box and eliminate all parts of tile that are out of the core of the region.
                    // we define the core as 1/3 the bounding box in either X or Y, but not larger than maxKillX or maxKillY
                    Point ul = points->getUL();
                    Point lr = points->getLR();
                    // set bounds
                    int minX = (lr.x-ul.x)/3;
                    int minY = (lr.y-ul.y)/3;
                    if (debug) printf("X:%d, Y:%d\n", minX, minY);
                    if (minX > maxkillX) minX = maxkillX;
                    if (minY > maxkillY) minY = maxkillY;
                    if (debug) printf("X:%d, Y:%d\n", minX, minY);
                    // see about bounds of each line - first we do each horiz line from top to bottom
                    for (int y=ul.y; y<=lr.y; y++) {
                        int x;
                        if (debug) printf("Y=%d", y);
                        // search for first white pixel
                        for (x=ul.x; x<=lr.x; x++) {
                            if (tiled->getPixel(x, y) == 1) break;
                        }
                        int start = x++; // start of the tile in the horiz direction at this Y
                        for (; x<=lr.x; x++) {
                            int c = tiled->getPixel(x, y);
                            if (((c == 0)||(c == 1)||(c == RED))&&(start == -1)) {
                                // we are starting a new part of the tile
                                start = x;
                                continue;
                            } 
                            if ((c != 1)&&(c != 0)&&(c != RED)&&(start != -1)) {
                                // line just ended
                                if (debug) printf(" [%d-%d]%d", start, x, c);
                                if ((x-start)>minX) {
                                    // this piece of the tile (from start,y to x,y) is large enough to keep
                                    if (debug) printf("*");
                                    check.drawLine(start,y,x,y,1);
                                }
                                start = -1;
                            }
                        }
                        // check to see if we were in the middel of a piece of the tile
                        if (start != -1) {
                            if (debug) printf(" [%d-%d]", start, x);
                            if ((x-start)>minX) {
                                if (debug) printf("*");
                                check.drawLine(start,y,x,y,1);
                            }
                        }
                        if (debug) printf("\n");
                    }
                    // Now repeat for each vertical line from left to right
                    for (int x=ul.x; x<=lr.x; x++) {
                        if (debug) printf("X=%d", x);
                        int y;
                        for (y=ul.y; y<=lr.y; y++) {
                            if (tiled->getPixel(x, y) == 1) break;
                        }
                        int start = y++;
                        for (; y<=lr.y; y++) {
                            int c = tiled->getPixel(x, y);
                            if (((c == 1)||(c==0)||(c == RED))&&(start == -1)) {
                                start = y;
                                continue;
                            } 
                            if ((c != 1)&&(c != 0)&&(c != RED)&&(start != -1)) {
                                // line just ended
                                if (debug) printf(" [%d-%d]", start, y);
                                if ((y-start)>minY) {
                                    if (debug) printf("*");
                                    check.drawLine(x,start,x,y,1);
                                }
                                start = -1;
                            }
                        }
                        if (start != -1) {
                            if (debug) printf(" [%d-%d]", start, y);
                            if ((y-start)>minY) {
                                if (debug) printf("*");
                                check.drawLine(x,start,x,y,1);
                            }
                        }
                        if (debug) printf("\n");
                    }
                } else if (0) {
                    // check to see if surrounded by all the same region
                    bool foundOther = false;
                    Pixel other;
                    bool failed = false;
                    for (RPIterator it = points->begin(); it != points->end(); ++it) {
                        Point p = *it;
                        Pixel c;
                        for (int x = p.x-1; x>=0; x--) {
                            c = tiled->getPixel(x, p.y);
                            if ((c == 0)||(c == 1)) continue;
                            if (!foundOther) {
                                foundOther = true;
                                if (fbtdebug) printf("A Found other = %d (%d,%d)\n", c, x, p.y);
                                other = c;
                                break;
                            }
                            else if (foundOther && (c != other)) {
                                if (fbtdebug) printf("A Failed due to %d != %d (%d,%d)\n", c, other, x, p.y);
                                failed = true;
                                break;
                            } else break;
                        }
                        if (failed) break;
                        for (int x = p.x+1; x<tiled->getWidth(); x++) {
                            c = tiled->getPixel(x, p.y);
                            if ((c == 0)||(c == 1)) continue;
                            if (!foundOther) {
                                if (fbtdebug) printf("B Found other = %d\n", c);
                                foundOther = true;
                                other = c;
                                break;
                            } else if (foundOther && (c != other)) {
                                if (fbtdebug) printf("B Failed due to %d != %d (%d,%d)\n", c, other, x, p.y);
                                failed = true;
                                break;
                            } else break;
                        }
                        if (failed) break;
                        for (int y = p.y+1; y<tiled->getHeight(); y++) {
                            c = tiled->getPixel(p.x, y);
                            if ((c == 0)||(c == 1)) continue;
                            if (!foundOther) {
                                if (fbtdebug) printf("C Found other = %d\n", c);
                                foundOther = true;
                                other = c;
                                break;
                            }
                            else if (foundOther && (c != other)) {
                                if (fbtdebug) printf("C Failed due to %d != %d\n", c, other);
                                failed = true;
                                break;
                            } else break;
                        }
                        if (failed) break;
                        for (int y = p.y-1; y>= 0; y--) {
                            c = tiled->getPixel(p.x, y);
                            if ((c == 0)||(c == 1)) continue;
                            if (!foundOther) {
                                if (fbtdebug) printf("D Found other = %d\n", c);
                                foundOther = true;
                                other = c;
                                break;
                            }
                            else if (foundOther && (c != other)) {
                                if (fbtdebug) printf("D Failed due to %d != %d\n", c, other);
                                failed = true;
                                break;
                            } else break;
                        }
                    }
                    if (failed) {
                        if (0) printf("Keep @ (%d,%d) %ld\n", x, y, points->size());
                    } else {
                        if (0) printf("Removing surrounded region @ (%d,%d)\n", x, y);
                        for (RPIterator it = points->begin(); it != points->end(); ++it) {
                            Point p = *it;
                            tiled->setPixel(p.x, p.y, GREEN);
                        }
                    }
                    
                }
                delete points;
            }
        }
    }
    // at this point check has the parts of the tile we want (good
    // pixels are set to 1) as well as parts we might want to add that
    // were not set in tiled, but were between other pieces of a tile on the same X or Y axis

    // tiled has has the original pieces of the tile that are large enough (pixels are 1)
    // tiled may have red pixels, which are contiguous tiled regions which are too small to keep

    // recolor back to white
    for (int y=0; y<tiled->getHeight(); y++) {
        for (int x=0; x<tiled->getWidth(); x++) {
            if (tiled->getPixel(x, y) == 1) {
                if (check.getPixel(x, y) == 1) 
                    tiled->setPixel(x, y, WHITE);
                else
                    tiled->setPixel(x, y, BLUE);
            } 
        }
    }

    if (si) tiled->PPMout(imageFilename("blue-tiled", "ppm"));


    // now fill in regions which are checked, but weren't in original.
    // If there is a gap in a part of the tile that is SMALLER than
    // the sum of the pieces on either side, add it in, otherwise,
    // don't.  Also, don't add in anything more than 1/4 the width or height of the image 

    // first create a x and a y image with the length of the tiled
    // regions in their respective directions.  If a tile line is less
    // than its adjacent line, we add in 1/2 of the length of the
    // adjacent line.  This is to deal with the case where something
    // pokes up just slightly into the tiled region breaking the line,
    // but really the line is part of a big block.
    Image<int> xlengths(tiled->getWidth(), tiled->getHeight());
    Image<int> ylengths(tiled->getWidth(), tiled->getHeight());
    calcTileLengths(tiled, xlengths, 1, 0);
    calcTileLengths(tiled, ylengths, 0, 1);
    for (int y=1; y<tiled->getHeight(); y++) {
        for (int x=1; x<tiled->getWidth(); x++) {
            if ((tiled->getPixel(x, y) != WHITE)&&(check.getPixel(x,y) == 1)) {
                PotentiallyFillGap(x, y, 1, 0, tiled, check, xlengths); // check Horizontal
                PotentiallyFillGap(x, y, 0, 1, tiled, check, ylengths); // check Vertical
            }
        }
    }
    
    if (si) tiled->PPMout(imageFilename("tiled","ppm"));
    delete prevTile;
    return tiled;
}

// eliminate all white which are not at least 1/2 the size of the bounding box
template<class Pixel>
Image<Pixel>* 
cleanBlockingTiles(Image<Pixel>* prevTile)
{

}

Image<int>* fill;
Image<int>* tiled;
Image<unsigned char>* pImage;
NearInfo<int> nearest[4];
int showsize = 0;



void
addLine2Region(Region* dest, int sx, int sy, int dx, int dy, RegionList* allRegions, Region* src, Image<int>* img)
{
    if (ldebug) printf("al2r %d <- %d\n", dest->id, src->id);
    img->addLine2Region(sx, sy, dx, dy, dest->id, allRegions, src->id);
    img->drawLine(sx, sy, dx, dy, dest->id);
    pImage->drawLine(sx, sy, dx, dy, pImage->black());
    if (0) {
        if (src != dest) {
            for (RPIterator it = src->begin(); it != src->end(); ++it) {
                Point p = *it;
                dest->add(p);
                img->setPixel(p.x, p.y, dest->id);
            }
        }
    }
}

void
mergeRegions(Region* dest, Region* src, Image<int>* img, RegionList* allRegions)
{
    if (ldebug) printf("MR: %d<-%d\n", dest->id, src->id);
    assert(dest->id != src->id);

    Region* a = dest;
    Region* b = src;
    if (dest->size() > src->size()) {
        a = src;
        b = dest;
    }
    int w = img->getWidth();
    int h = img->getHeight();
    int aid = a->id;
    int bid = b->id;
    for (RPIterator it = a->begin() ; it != a->end(); ++it) {
        Point p = *it;
        for (int d=0; d<4; d++) {
            int deltax = directions[d].x;
            int deltay = directions[d].y;
            int x = p.x;
            int y = p.y;
            int c = 0;
            while (1) {
                x += deltax;
                y += deltay;
                if (((x < 0)||(x >= w))||(y < 0)||(y>=h)) break;
                c = img->getPixel(x, y);
                if (c != 0) break;
                if (c == aid) break;
            }
            if (c == bid) {
                addLine2Region(dest, p.x, p.y, x, y, allRegions, src, img);
                allRegions->merge(dest, src, img);
                return;
            }
        }
    }
    assert(0);
}

static int cbbc=3;

void
checkBBcontainment(Image<int>* regions, RegionList* allRegions)
{
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        list->allValid(false);
    }

    if (cbbc-- < 0) return;

    int countr=20;
    Image<int>* intersect = new Image<int>(regions->getWidth(), regions->getHeight());
    intersect->fill(0);
    int regionid = 0x800000;
    int colordiff = 0xfffff3/allRegions->numRegions();
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        int mysize = list->size();
        if (mysize > 2200) continue;
        //printf("getting n of %d\n", list->id);
        Set<Region*>* nbrs =list->getNeighbors(regions, allRegions);
        Point myul = list->getUL();
        Point mylr = list->getLR();
        double myarea = (mylr.y-myul.y)*(mylr.x-myul.x);
        //printf("BB:%d:", list->id);
        for (SetIterator<Region*> it(*nbrs); !it.isEnd(); it.next()) {
            Region* nbr = it.current();
            if (nbr->size() < mysize) continue;
            //printf(" %d", nbr->id);
            // list < nbr.  See if list is in nbr
            Point ul = nbr->getUL();
            Point lr = nbr->getLR();
            if (ul.x < myul.x) ul.x = myul.x;
            if (ul.y < myul.y) ul.y = myul.y;
            if (lr.x > mylr.x) lr.x = mylr.x;
            if (lr.y > mylr.y) lr.y = mylr.y;
            // check for intersection
            if ((ul.x > lr.x)||(ul.y > lr.y)) continue;
            // now check percent of intersection
            double interArea = (lr.y-ul.y)*(lr.x-ul.x);
            double interX = (double)(lr.x-ul.x)/(double)(mylr.x-myul.x);
            double interY = (double)(lr.y-ul.y)/(double)(mylr.y-myul.y);
            bool include = (interArea/myarea > .5)||((interY+interX > .9)&&((interX>.25)&&(interY>.25)));
            if (ldebug) {
                printf("%d & %d intersect %6.2f (%6.2f) sizes:%ld & %ld.  x:%6.2f  y:%6.2f %s\n", 
                       list->id, nbr->id, 
                       interArea, 100.0*interArea/myarea, 
                       list->size(), nbr->size(), 
                       100*interX, 100*interY,
                       include ? "MERGE" : "skip");
            }
            int rid = include ? regionid : 0x000ff0000;
            
            for (RPIterator it = nbr->begin(); it != nbr->end(); ++it) {
                Point p = *it;
                intersect->setPixel(p.x, p.y, rid);
            }
            nbr->label(intersect, a12, 0.3);
            regionid += colordiff;
            rid = include ? regionid : 0x000ff00;
            for (RPIterator it = list->begin(); it != list->end(); ++it) {
                Point p = *it;
                intersect->setPixel(p.x, p.y, regionid);
            }
            list->label(intersect, a12, 0.3);
            regionid += colordiff;

            // merge if asked to
            if (include) {
                mergeRegions(list, nbr, regions, allRegions);
                break;
            }
        }
        //printf("\n");
    }
    if (si) intersect->PPMout(imageFilename("intersect", "ppm"));
    delete intersect;
}

template<class Pixel>
void
showbb(Image<Pixel>* src, RegionList* allRegions)
{
    if (!si) return;
    int colordiff = 0xfffff3/allRegions->numRegions();
    int regionid = 0x800000;
    Image<int>* regions = new Image<int>(src->getWidth(), src->getHeight());
    regions->fill(0);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Point ul = list->getUL();
        Point lr = list->getLR();
        for (int y=ul.y; y<=lr.y; y++) {
            regions->drawLine(ul.x, y, lr.x, y, regionid);
        }
        regionid += colordiff;
    }
    regions->PPMout(imageFilename("bb", "ppm"));
    delete regions;
}


enum ConnMode { Sym, Asym, Two };

const char* cm2str(ConnMode x) {
    switch (x) {
    case Sym: return "Symmetric";
    case Asym: return "Asymetric";
    case Two: return "NeedTwo";
    }
}

template<class Pixel>
void
connectRegions(Image<Pixel>*& regions, RegionList* allRegions, Region* list, std::vector<Distance>& connectors)
{
    int debug = 0;
    int rid = list->id;
    Set<int> dests;
    if (debug) printf("Connect: %d with %ld:", list->id, connectors.size());
    if (showsize) printf("two: %d of %ld -> ", list->id, list->size());
    for (std::vector<Distance>::iterator it = connectors.begin(); 
         it != connectors.end(); 
         ++it) {
        Distance dist = *it;
        Pixel targetRegionId = regions->getPixel(dist.end.x, dist.end.y);
        if (debug) printf(" (%d,%d)=[%d+?]=>%d@(%d,%d)", dist.start.x, dist.start.y, dist.end.distance(dist.start), targetRegionId, dist.end.x, dist.end.y);
        pImage->drawLine(dist.start.x, dist.start.y, dist.end.x, dist.end.y, pImage->black());
        if (!dests.exists(targetRegionId)) {
            dests.insert(targetRegionId);
            if (showsize) printf(" [%d of %ld]", targetRegionId, allRegions->get(targetRegionId)->size());
        }
        regions->addLine2Region(dist.start.x, dist.start.y, dist.end.x, dist.end.y, rid, allRegions, targetRegionId);
        regions->drawLine(dist.start.x, dist.start.y, dist.end.x, dist.end.y, rid);
    }
    if (showsize) printf("\n");
    if (debug) printf("\nConnected to %d <- ", rid);
    for (SetIterator<int> it(dests); !it.isEnd(); it.next()) {
        int targetRegionId = it.current();
        if (debug) printf(" %d", targetRegionId);
        allRegions->merge(list, targetRegionId, regions);
    }
    list->PerimValid(false);
    if (debug) printf("\n");
}

// combine rid with all matching targetId
// uses info in nearest
template<class Pixel>
void
combine(Image<Pixel>*& regions, RegionList* allRegions, Pixel rid, Region* list, Pixel targetRegionId)
{
    int debug = 0;
    if (debug) printf("Combine: %d->%d [%ld+%ld]:", rid, targetRegionId, allRegions->get(rid)->size(), allRegions->get(targetRegionId)->size());
    int possibleDup = 0;
    for (int j=0; j<4; j++) {
        if (!nearest[j].valid) continue;
        if (nearest[j].id != targetRegionId) continue;
        Point srcp = nearest[j].from;
        Point dstp = nearest[j].to;
        printf(" (%d,%d)=[%d+%g]=>(%d,%d)", srcp.x, srcp.y, nearest[j].distance, nearest[j].reldist-nearest[j].distance, dstp.x, dstp.y);

        pImage->drawLine(srcp.x, srcp.y, dstp.x, dstp.y, pImage->black());
        if (possibleDup++ == 0)
            allRegions->addLine2Region(srcp.x, srcp.y, dstp.x, dstp.y, targetRegionId);
        else
            regions->addLine2Region(srcp.x, srcp.y, dstp.x, dstp.y, targetRegionId, allRegions, rid);
        regions->drawLine(srcp.x, srcp.y, dstp.x, dstp.y, targetRegionId);
    }
    printf("\n");

    allRegions->merge(targetRegionId, list, regions);
}


template<class Pixel>
int
connregions1(RegionList* allRegions, Image<Pixel>*& regions, Image<unsigned char>*& blocker, int maxRegionSize, int sd, int ed, double bm, ConnMode sym=Asym)
{
    int debug = 0;
    printf("Try Upto size:%d, passes:%d-%d, bm:%g %s\n", maxRegionSize, sd, ed, bm, cm2str(sym));
    int connections = 0;
    int zeroConn = 3000;
    int numreg = allRegions->numRegions();
    for (int i=sd; i<ed; i++) {
        int iPass = 0;

        while (1) {
            printf("%d: %d regions: pass %d (%d)\n", i, numreg, iPass, connections);
            int changes = 0;

            if (intermediateFiles) {
                // recolor
                Image<int>* newregions = recolor(regions, allRegions);

                char filename[128];
                sprintf(filename, "%s-%d-%d-1-regions.ppm", intermediateFiles, i, iPass);
                newregions->PPMout(filename);
                delete newregions;
            }

            //checkmatch("after initial", allRegions, regions);

            if (numreg < 3) break;

            // find nearest, building regions as we go
            if (0) {
                Region* list = allRegions->get(2);
                printf("region 2 in list\n");
                for (RPIterator it = list->begin(); it != list->end(); ++it) {
                    Point p = *it;
                    printf("(%d, %d) ", p.x, p.y);
                }
                printf("region 2 in image\n");
                for (int y=0; y<regions->getHeight(); y++) {
                    for (int x=0; x<regions->getWidth(); x++) {
                        if (regions->getPixel(x, y) == 2) printf("(%d, %d) ", x, y);
                    }
                }
            }
      
            int maxallowed = i+2;
            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                if (list == NULL) continue;
                if ((maxRegionSize>0)&&(list->size() > maxRegionSize)) continue;
                Point p = list->get(0);
                int y = p.y;
                int x = p.x;
                int rid = regions->getPixel(x, y);
                //printf("R:%d %ld\n", rid, list->size());
                //if ((rid == 762)||(rid == 960)) debug=1; else debug=0;
                //if ((rid == 4493)||(rid == 4508)) debug=1; else debug=0;
                if (debug) printf("R:%d %ld\n", rid, list->size());
                //printf("%d, %d @ %d\n", x, y, rid);

                int nc = regions->findNearestRegions(list, nearest, maxallowed, (sym==Sym) ? maxRegionSize : 0, blocker, bm, allRegions);
                if (nc == 0) continue;
                // found some near region
                int firstvalid;
                for (firstvalid=0; firstvalid<4; firstvalid++) {
                    if (nearest[firstvalid].valid) break;
                }
                // make sure size info is valid
                if (nearest[firstvalid].size < 0) {
                    for (int j=0; j<4; j++) nearest[j].size = 1;
                }
                    
                if (debug) {
                    for (int j=0; j<4; j++) {
                        if (nearest[j].valid) printf(" in(%d,%d) -> %d %d+%g of %d\n", nearest[j].deltax, nearest[j].deltay, nearest[j].id, nearest[j].distance, nearest[j].reldist-nearest[j].distance, nearest[j].size);
                    }
                }

                // prefer connecting to a target which is on multiple sides
                int found = -1;
                {
                    int target;
                    double tmin = nearest[firstvalid].reldist;
                    for (int j=firstvalid; j<4; j++) {
                        if (!nearest[j].valid) continue;
                        target = nearest[j].id;
                        double min  = nearest[j].reldist;
                        bool ok = false;
                        for (int k=j+1; k<4; k++) {
                            if (!nearest[k].valid) continue;
                            if (nearest[k].id == target) {
                                ok = true;
                                if (min > nearest[k].reldist) min = nearest[k].reldist;
                            }
                        }
                        if (ok) {
                            // found >= 2 regions with same id
                            if ((found == -1)||(tmin > min)) {
                                // this is best of the multi regions we found
                                found = j;
                                tmin = min;
                            }
                        }
                    }
                }
                // did we find >= 2 of same region?
                if (found >= 0) {
                    combine(regions, allRegions, rid, list, nearest[found].id);
                    connections++;
                    changes++;
                    numreg--;
                    continue;
                }
                if (sym == Two) continue;

                // pick smallest of directions/sizes and combine
                
                found = firstvalid;
                double tmin = nearest[found].reldist*nearest[found].size;
                for (int j=firstvalid+1; j<4; j++) {
                    if ((nearest[j].valid)&&((nearest[j].reldist*nearest[j].size)<tmin)) {
                        found = j;
                        tmin = nearest[found].reldist*nearest[found].size;
                    }
                }
                combine(regions, allRegions, rid, list, nearest[found].id);
                numreg--;
                connections++;
                changes++;
                if (numreg < 4) break;
            }
            //checkmatch("after all regions", allRegions, regions);
            //sprintf(filename, "conn-%d.ppm", i);
            //pImage->PGMout(filename);
            //sprintf(filename, "after-%d.ppm", i);
            //Image<int>* afterRegions = recolor(regions, numreg);
            //afterRegions->PPMout(filename);
            if (numreg < 4) break;
            if (changes == 0) break;
            iPass++;
        }
        if (0) checkmatch("after while loop", allRegions, regions);

        if (connections > 0) {
            printf("pass %d: while total connected %d\n", i, connections);

            if (fill) delete fill;
            fill = recolor(regions, allRegions);
            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                if (list == NULL) continue;
                fillminmax(fill, list);
                //aspectfill(fill, list);
            }
            if (intermediateFiles) {
                char filename[128];
                sprintf(filename, "%s-%d-%d-2-filled.ppm", intermediateFiles, i, iPass);
                fill->PPMout(filename);
            }

            if (genHist) {
                int barlen = 600;
                Image<int>* base;
                Image<int>* hImg;
                if (0) {
                    base = recolor(fill,allRegions);
                    hImg = new Image<int>(base->getWidth()+barlen, base->getHeight()+barlen);
                    base->copyRegion(0, 0, base->getWidth(), base->getHeight(), hImg, 0, 0);
                }

                Histogram* hx = fill->collectEdgeHistogram(0, 0, edgelen);
                Histogram* hy = fill->collectEdgeHistogram(1, 0, edgelen);
                if (blocker) delete blocker;
                blocker = new Image<unsigned char>(fill->getWidth(), fill->getHeight());
                blocker->fill(0);
                hx->rescale(110);
                hy->rescale(140);
                for (int y=0; y<blocker->getHeight(); y++) {
                    for (int x=0; x<blocker->getWidth(); x++) {
                        blocker->setPixel(x, y, hx->get(x)+hy->get(y));
                    }
                }
                if (intermediateFiles) {
                    char filename[128];
                    sprintf(filename, "%s-%d-%d-2-blocker.ppm", intermediateFiles, i, iPass);
                    blocker->PGMout(filename);
                }
                if (0) {
                    //hx->print("X before remin");
                    printf("Base X:%d\n", hx->remin(10));
                    //hx->print("X after remin");
                    hx->rescale(barlen);
                    //hx->print("X after scale");
                    int starty = base->getHeight();
                    for (int x=0; x<base->getWidth(); x++) {
                        hImg->drawLine(x, starty, x, starty+hx->get(x), 0x00ffffff);
                    }
                
                    int startx = base->getWidth();
                    //hx->print("Y before remin");
                    printf("Base Y:%d\n", hy->remin(10));
                    //hx->print("Y after remin");
                    hy->rescale(barlen);
                    //hx->print("Y after scale");
                    for (int y=0; y<base->getHeight(); y++) {
                        hImg->drawLine(startx, y, startx+hy->get(y), y, 0x00ffffff);
                    }
                }
                if (1) {
                    char filename[128];
                    sprintf(filename, "%s-%d-hist.ppm", outname, i);
                    hImg->PPMout(filename);
                    delete hImg;
                    delete base;
                }
                delete hx;
                delete hy;

                if (0) {
                    for (int direc=0; direc <= 1; direc++) {
                        Histogram* h = regions->collectEdgeHistogram(direc, 0, 1);
                        printf("%d ------------------- %s\n", i, (direc == 0)?"HX":"HY");
                        for (int z=0; z<h->numBuckets(); z++) {
                            printf("%4d:(%6d):", z, h->get(z));
                            int dot = h->get(z);
                            for (int j=0; j<dot; j++) printf("=");
                            printf("\n");
                        }
                        delete h;
                    }
                }
            }
        }
        if (zeroConn-- <= 0) break;
    }
    return connections;
}

template<class Pixel>
void
updateFillBlocker(Image<unsigned char>*& blocker, RegionList* allRegions, Image<Pixel>*& regions)
{
    if (fill) delete fill;
    fill = recolor(regions, allRegions);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        fillminmax(fill, list);
        //aspectfill(fill, list);
    }
    if (intermediateFiles) {
        fill->PPMout(imageFilename("filled", "ppm"));
    }

    if (genHist) {
        int showimg = 0;
        int barlen = 600;
        Image<int>* base;
        Image<int>* hImg;
        if (showimg) {
            base = recolor(fill,allRegions);
            hImg = new Image<int>(base->getWidth()+barlen, base->getHeight()+barlen);
            base->copyRegion(0, 0, base->getWidth(), base->getHeight(), hImg, 0, 0);
        }

        Histogram* hx = fill->collectEdgeHistogram(0, 0, edgelen);
        Histogram* hy = fill->collectEdgeHistogram(1, 0, edgelen);
        if (blocker) delete blocker;
        blocker = new Image<unsigned char>(fill->getWidth(), fill->getHeight());
        blocker->fill(0);
        hx->rescale(110);
        hy->rescale(140);
        for (int y=0; y<blocker->getHeight(); y++) {
            for (int x=0; x<blocker->getWidth(); x++) {
                blocker->setPixel(x, y, hx->get(x)+hy->get(y));
            }
        }
        if (intermediateFiles) blocker->PGMout(imageFilename("blocker2", "ppm"));
        if (showimg) {
            //hx->print("X before remin");
            printf("Base X:%d\n", hx->remin(10));
            //hx->print("X after remin");
            hx->rescale(barlen);
            //hx->print("X after scale");
            int starty = base->getHeight();
            for (int x=0; x<base->getWidth(); x++) {
                hImg->drawLine(x, starty, x, starty+hx->get(x), 0x00ffffff);
            }
                
            int startx = base->getWidth();
            //hx->print("Y before remin");
            printf("Base Y:%d\n", hy->remin(10));
            //hx->print("Y after remin");
            hy->rescale(barlen);
            //hx->print("Y after scale");
            for (int y=0; y<base->getHeight(); y++) {
                hImg->drawLine(startx, y, startx+hy->get(y), y, 0x00ffffff);
            }
        }
        if (showimg) {
            hImg->PPMout(imageFilename("hist", "ppm"));
            delete hImg;
            delete base;
        }
        delete hx;
        delete hy;

        if (0) {
            for (int direc=0; direc <= 1; direc++) {
                Histogram* h = regions->collectEdgeHistogram(direc, 0, 1);
                printf("------------------- %s\n", (direc == 0)?"HX":"HY");
                for (int z=0; z<h->numBuckets(); z++) {
                    printf("%4d:(%6d):", z, h->get(z));
                    int dot = h->get(z);
                    for (int j=0; j<dot; j++) printf("=");
                    printf("\n");
                }
                delete h;
            }
        }
    }
}

template<class Pixel>
int
connregions(RegionList* allRegions, Image<Pixel>*& regions, Image<unsigned char>*& blocker, int maxRegionSize, int sd, int ed, double bm, ConnMode sym=Asym)
{
    int debug = 0;
    printf("Try Upto size:%d, passes:%d-%d, bm:%g %s\n", maxRegionSize, sd, ed, bm, cm2str(sym));
    int connections = 0;
    int zeroConn = 3000;
    int numreg = allRegions->numRegions();
    for (int i=sd; i<ed; i++) {
        int iPass = 0;

        while (1) {
            printf("%d: %d regions: pass %d (%d)\n", i, numreg, iPass, connections);
            int changes = 0;

            if (intermediateFiles) {
                // recolor
                Image<int>* newregions = recolor(regions, allRegions);

                char filename[128];
                sprintf(filename, "%s-%d-%d-1-regions.ppm", intermediateFiles, i, iPass);
                newregions->PPMout(filename);
                delete newregions;
            }

            //checkmatch("after initial", allRegions, regions);

            if (numreg < 3) break;

            int maxallowed = i+2;
            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                if (list == NULL) continue;
                if ((maxRegionSize>0)&&(list->size() > maxRegionSize)) continue;
                Point p = list->get(0);
                int y = p.y;
                int x = p.x;
                int rid = regions->getPixel(x, y);
                assert(rid == list->id);
                //printf("R:%d %ld\n", rid, list->size());
                //if ((rid == 762)||(rid == 960)) debug=1; else debug=0;
                //if ((rid == 4493)||(rid == 4508)) debug=1; else debug=0;
                if (debug) printf("R:%d %ld\n", rid, list->size());
                //printf("%d, %d @ %d\n", x, y, rid);

                list->findPerimeter(regions, allRegions);
                int nc = regions->findNearestRegionsByPerim(list, nearest, maxallowed, (sym==Sym) ? maxRegionSize : 0, blocker, bm, allRegions);
                if (nc == 0) continue;
                // found some near region
                int firstvalid;
                for (firstvalid=0; firstvalid<4; firstvalid++) {
                    if (nearest[firstvalid].valid) break;
                }
                // make sure size info is valid
                if (nearest[firstvalid].size < 0) {
                    for (int j=0; j<4; j++) nearest[j].size = 1;
                }
                    
                if (debug) {
                    for (int j=0; j<4; j++) {
                        if (nearest[j].valid) printf(" in(%d,%d) -> %d %d+%g of %d\n", nearest[j].deltax, nearest[j].deltay, nearest[j].id, nearest[j].distance, nearest[j].reldist-nearest[j].distance, nearest[j].size);
                    }
                }

                // prefer connecting to a target which is on multiple sides
                int found = -1;
                {
                    int target;
                    double tmin = nearest[firstvalid].reldist;
                    for (int j=firstvalid; j<4; j++) {
                        if (!nearest[j].valid) continue;
                        target = nearest[j].id;
                        double min  = nearest[j].reldist;
                        bool ok = false;
                        for (int k=j+1; k<4; k++) {
                            if (!nearest[k].valid) continue;
                            if (nearest[k].id == target) {
                                ok = true;
                                if (min > nearest[k].reldist) min = nearest[k].reldist;
                            }
                        }
                        if (ok) {
                            // found >= 2 regions with same id
                            if ((found == -1)||(tmin > min)) {
                                // this is best of the multi regions we found
                                found = j;
                                tmin = min;
                            }
                        }
                    }
                }
                // did we find >= 2 of same region?
                if (found >= 0) {
                    combine(regions, allRegions, rid, list, nearest[found].id);
                    connections++;
                    changes++;
                    numreg--;
                    continue;
                }
                if (sym == Two) continue;

                // pick smallest of directions/sizes and combine
                
                found = firstvalid;
                double tmin = nearest[found].reldist*nearest[found].size;
                for (int j=firstvalid+1; j<4; j++) {
                    if ((nearest[j].valid)&&((nearest[j].reldist*nearest[j].size)<tmin)) {
                        found = j;
                        tmin = nearest[found].reldist*nearest[found].size;
                    }
                }
                combine(regions, allRegions, rid, list, nearest[found].id);
                numreg--;
                connections++;
                changes++;
                if (numreg < 4) break;
            }
            //checkmatch("after all regions", allRegions, regions);
            //sprintf(filename, "conn-%d.ppm", i);
            //pImage->PGMout(filename);
            //sprintf(filename, "after-%d.ppm", i);
            //Image<int>* afterRegions = recolor(regions, numreg);
            //afterRegions->PPMout(filename);
            if (numreg < 4) break;
            if (changes == 0) break;
            iPass++;
        }
        if (0) checkmatch("after while loop", allRegions, regions);

        if (connections > 0) 
            printf("pass %d: while total connected %d\n", i, connections);
        if (zeroConn-- <= 0) break;
    }
    return connections;
}

template<class Pixel>
void
genSizeImage(Image<Pixel>* regions, RegionList* allRegions)
{
    Image<int>* newregions = recolor(regions, allRegions);
    int maxsize = 0;
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Point minp = list->getUL();
        Point maxp = list->getLR();
        Point p((minp.x+maxp.x)/2, (minp.y+maxp.y)/2);
        int z = list->size();
        char buffer[128];
        sprintf(buffer, "%d", z);
        a12->draw(newregions, buffer, 0x0ffffff, p.x, p.y, 0.2);
        if (z > maxsize) maxsize = z;
    }
    if (si) newregions->PPMout(imageFilename("sizes", "ppm"));
    delete newregions;

    Histogram sizes(maxsize+1);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        sizes.set(list->size());
    }
    sizes.print("Sizes:", 10);
}

template<class Pixel>
void
saveSmall(Image<Pixel>* regions, RegionList* allRegions, int maxsize)
{
    Image<int>* small = recolor(regions, allRegions);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        if (list->size() >= maxsize) {
            for (RPIterator it = list->begin(); it != list->end(); ++it) {
                Point p = *it;
                small->setPixel(p.x, p.y, 0x08b8989);
            }
        }
    }
    char buffer[128];
    sprintf(buffer, "lessthan-%d", maxsize);
    small->PPMout(imageFilename(buffer, "ppm"));
    delete small;
}

template<class Pixel>
void
saveSides(Image<Pixel>* regions, RegionList* allRegions)
{
    if (!si) return;
    Image<int>* small = new Image<Pixel>(regions);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        Pixel c = 0x0ffff00;
        switch (list->commonSides) {
        case 1: c = 0x02f4f4f; break;
        case 2: c = 0x0ff0000; break;
        case 3: c = 0x000ff00; break;
        case 4: c = 0x00000ff; break;
        case 5: c = 0x0ffffff; break;
        }
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            small->setPixel(p.x, p.y, c);
        }
    }
    small->PPMout(imageFilename("sides", "ppm"));
    delete small;
}


class Line {
public:
    Point start;
    int length;
    Point direction;
    Region* r;                  // while it exists, the region associated with this line
    int rid;
public:
    Line(Point _s, int _l, Point _d, Region* _r) : start(_s), length(_l), direction(_d), r(_r), rid(_r->id) {}
};

class Lines {
public:
    static std::vector<Lines*> all;
    static const int xIndentForLine = 10;
    static const int xSideIndentForLine = 5;

    std::vector<Line*> starts;
    int side;                   // 0 = left, 1 = right
    char* description;
    int edgex;
    int lineHeight;

    Lines(const char* desc, int s, int lh) : 
        description(strdup(desc)), side(s), lineHeight(lh), edgex(-1)
    {
        all.push_back(this);
    }
    void addLine(Point start, int length, Region* r) {
        starts.push_back(new Line(start, length, side ? directions[0] : directions[1], r));
        // track boundary edge
        int x;
        if (side) x = r->getLR().x; else x = r->getUL().x;
        if ((edgex == -1)||(side&&(edgex < x))||(!side && (edgex > x))) edgex = x;
    }
    Region* getStartRegion(void) {
        return starts[0]->r;
    }
    Region* getEndRegion(void) {
        return starts.back()->r;
    }
    void connect(Image<int>* regions, RegionList* allRegions, Hash<int, int>& remap) {
        int i=1;
        int end = starts.size();
        if (ldebug) {
            printf("CONNECT: "); for (int j=0; j<end; j++) printf(" %d", starts[j]->rid); printf("\n");
        }
        if (allRegions->get(starts[0]->rid) == NULL) {
            // already merged
            assert(remap.exists(starts[0]->rid));
            printf("Already merged head of lines region: %d\n", starts[0]->rid);
            return;
        }
        Region* first = starts[0]->r;
        Point start;
        if (side) {
            start = first->getLR();
            //int x = start.x - Lines::xIndentForLine;
            int x = edgex - Lines::xIndentForLine;
            if (x < 0) {
                x = Lines::xSideIndentForLine;
                if (x < start.x) addLine2Region(first, x, start.y, start.x, start.y, allRegions, first, regions);
            }
            start.x = x;
        } else {
            start = first->getUL();
            //int x = start.x + Lines::xIndentForLine;
            int x = edgex + Lines::xIndentForLine;
            if (x > regions->getWidth()) {
                x = regions->getWidth()-Lines::xSideIndentForLine;
                if (x > start.x) addLine2Region(first, x, start.y, start.x, start.y, allRegions, first, regions);
            }
            start.x = x;
        }
        int endy = starts[end-1]->start.y;
        // now join all regions
        for (i=1; i<end; i++) {
            Line* line = starts[i];
            Region* r = line->r;
            if (ldebug) printf("Merge %d into %d\n", r->id, first->id);
            bool skip = false;
            int rid = r->id;
            while (remap.exists(rid)) {
                // we already moved r into another region, so get its dest
                int rnext = *(remap.get(rid));
                if (ldebug) printf("remap: %d -> %d\n", rid, rnext);
                if (0) {
                    if (rnext == first->id) {
                        // already done because of a previous merge
                        printf("%d already merged because of previous set of lines\n", line->r->id);
                        skip = true;
                        break;
                    }
                    r = allRegions->get(rnext);
                    if (r == NULL) {
                        printf("remapped region %d was already merged into %d\n", rnext, first->id);
                        skip = true;
                        break;
                    }
                }
                rid = rnext;
            }
            r = allRegions->get(rid);
            if (r == NULL) {
                printf("remapped region %d was already merged into %d\n", rid, first->id);
                skip = true;
                break;
            }
            if (r->id == first->id) {
                // already done because of a previous merge
                printf("%d already merged because of previous set of lines\n", rid);
                skip = true;
                break;
            }
            if (skip == true) continue;
            remap.insert(r->id, first->id);
            mergeRegions(first, r, regions, allRegions);
        }
        if (0) {
            // now draw vertical line
            printf("Adding vertical\n");
            addLine2Region(first, start.x, start.y, start.x, endy, allRegions, first, regions);
            printf("Added vertical\n");
        }
        first->setLineHeight(lineHeight);
    }
};

std::vector<Lines*> Lines::all;

template<class Pixel>
void 
connectLines(Lines* lines, Image<Pixel>* regions, RegionList* allRegions, Image<Pixel>* show, Pixel regionid)
{
    int linew = 11;
    Region* start = lines->getStartRegion();
    Region* end = lines->getEndRegion();
    Point ul = start->getUL();
    Point lr = start->getLR();
    Point endul = end->getUL();
    Point endlr = end->getLR();

    if (lines->side == 0) {
        // draw on left & top
        show->drawLine(ul.x, ul.y, ul.x, endlr.y, regionid, linew);
        show->drawLine(ul.x, ul.y, lr.x, ul.y, regionid, linew);
    } else {
        // draw on right and bottom
        show->drawLine(lr.x, ul.y, lr.x, endlr.y, regionid, linew);
        show->drawLine(endul.x, endlr.y, endlr.x, endlr.y, regionid, linew);
    }
    for (std::vector<Line*>::iterator it = lines->starts.begin(); it != lines->starts.end(); ++it) {
        Region* r = (*it)->r;
        for (RPIterator it = r->begin(); it != r->end(); ++it) {
            Point p = *it;
            show->setPixel(p.x, p.y, regionid);
        }
    }
    a12->draw(show, lines->description, 0x0ffffff, ul.x+10, ul.y+10, 0.5);
}

Lines*
foundGoodLines(Histogram& linespacing, RegionList* allRegions, int* included, int* interline, int start, int count, int side)
{
    char buffer[256];
    sprintf(buffer, "%d->%d: total:%d mode:%d median:%5.2f mean:%5.2f sd:%5.2f", included[start], included[count], linespacing.getTotal(), linespacing.mode(), linespacing.median(), linespacing.avg(), linespacing.stddev());
    if (ldebug) printf("%s:\n\t", buffer);
    Lines* lines = new Lines(buffer, side, linespacing.median());

    // show the interline spacing and create the Lines object
    for (int i=start; i<=count; i++) {
        // create a line object for this one
        Region*r = allRegions->get(included[i]);
        Point ul = r->getUL();
        Point lr = r->getLR();
        Point ls(side ? lr.x : ul.x, (lr.y+ul.y)/2);
        lines->addLine(ls, lr.x-ul.x, r);

        // print out for debugging
        if (ldebug) {
            if (i <count) printf("\t%d", interline[i]);
            if (((i+1) % 8)==0) printf("\n");
        }

    }
    if (ldebug) printf("\n");
    return lines;
} 


int 
processInterLine(int startRid, Hash<int,int> &list, RegionList* allRegions, std::vector<Lines*>& connected, int side)
{
    Region* start = allRegions->get(startRid);
    Point ul = start->getUL();
    Point lr = start->getLR();
    bool incr = false;
    int count = 0;
    int included[1024];
    int interline[1024];
    // start of a left/right edge
    if (ldebug) printf("%s edge From %d (%d, %d) -> ", (side == 0) ? "Left" : "Right", start->id, ul.x, ul.y);
    int downrid;
    included[count] = startRid;
    bool fail = false;
    for (int rid=startRid; list.exists(rid); rid=downrid) {
        count++;                    // track # of regions in this column
        downrid = *(list.get(rid)); // get next one down
        assert(downrid != rid);
        included[count] = downrid; // keep a list of them
        list.remove(rid);          // remove rid from list so we don't do it again
        if (ldebug) printf("%d ", downrid);
        // get interline spacing from rid down to downrid
        Region* me = allRegions->get(rid);
        Region* down = allRegions->get(downrid);
        int meMid = (me->getUL().y+me->getLR().y)/2;
        int downMid = (down->getUL().y+down->getLR().y)/2;
        if (downMid-meMid < 0) {
            // this is a strange region
            printf("Tried to put %d below %d, but actually is above - overlapping regions?\n", rid, downrid);
            fail = true;
        }
        interline[count-1] = downMid-meMid;
    }
    if (count <= 2) {
        if (ldebug) printf(" <2\n");
        return 0;
    }
    if (fail) {
        if (ldebug) printf(" fail\n");
        return 0;
    }

    // we have a region boundary.  lets analyze interline spacing
    Histogram linespacing;
    for (int i=0; i< count; i++) {
        linespacing.set(interline[i]);
    }
    // set end to the bottom most region with the same left hand side
    Region* end = allRegions->get(included[count]);
    if (ldebug) printf(" [%d,%d] -> [%d,%d]: ", ul.x, ul.y, end->getLR().x, end->getLR().y);
    // computer interline
    char buffer[128];
    sprintf(buffer, "%d->%d\n  total:%d mode:%d median:%5.2f mean:%5.2f sd:%5.2f", startRid, downrid, linespacing.getTotal(), linespacing.mode(), linespacing.median(), linespacing.avg(), linespacing.stddev());
    if (ldebug) {
        printf("%s: ", buffer);
        for (int i=0; i<linespacing.numBuckets(); i++)
            if (linespacing.get(i)!=0) printf("  %d*%d", i, linespacing.get(i));
        printf("\n");
    }

    int found = 0;
    {
        // get rid of outliers
        int zap[1024];
        bool zapped = false;
        memset(zap, 0, sizeof(int)*1024);
        double median = linespacing.median();
        double stddev = linespacing.stddev();
        double min = median-2.0*stddev;
        double max = median+2.0*stddev;
        for (int i=0; i<count; i++) {
            if ((interline[i] < min)||(interline[i] > max)) {
                zap[i] = 1;
                zapped = true;
                if (ldebug) printf("ZAPING: [%d]=%d\n", i, interline[i]);
                linespacing.set(interline[i], -1);
            }
        }
        if (ldebug) {
            for (int i=0; i<count; i++) {
                printf("\t%d", interline[i]);
                if (zap[i]) printf("!");
                if (((i+1)%10) == 0) printf("\n");
            }
            printf("\n");
        }

        // see if we need to break this into multiple pieces
        int start = 0;
        while ((start < count)&&(linespacing.getTotal() > 1)) {
            double median = linespacing.median();
            double stddev = linespacing.stddev();
            if (ldebug) printf("Searching [%d,%d] t:%d m:%6.2f sd:%6.2f\n", start, count, linespacing.getTotal(), median, stddev);
            Histogram newset;
            int i;
            int newstart;
            if (!zapped && (stddev < 9)) {
                connected.push_back(foundGoodLines(linespacing, allRegions, included, interline, start, count, side));
                return found+1;
            } else {
                double min = median-3.0*stddev/4.0;
                double max = median+3.0*stddev/4.0;
                if (stddev < 9) {
                    min -= 2*stddev;
                    max += 2*stddev;
                }
                while ((start < count)&&(zap[start])) start++;
                for (i=start; i<count; i++) {
                    if (zap[i]) break;
                    if ((interline[i] > max)||(interline[i]<min)) break;
                    newset.set(interline[i]);
                }
            }
            // see if we have a good set
            if (i < (start+3)) {
                // first one was bad or region too small
                newstart = start+1;
                for (int j=start; j<newstart; j++) {
                    if (ldebug) printf("too small: %d\n", interline[j]);
                }
            } else {
                if (newset.stddev() < 9) {
                    connected.push_back(foundGoodLines(newset, allRegions, included, interline, start, i, side));
                    found++;
                    newstart = i+1;
                } else {
                    newstart = start+1;
                    if (ldebug) {
                        for (int j=start; j<i; j++) {
                            printf("bad stddev: %d\n", interline[j]);
                        }
                    }
                }
            }
            // remove all old ones from linespacing
            if (newstart >= count) break;
            for (i=start; i<newstart; i++) {
                if (zap[i]) continue;
                if (ldebug) printf("Zaping: [%d]=%d\n", i, interline[i]);
                linespacing.set(interline[i], -1);
            }
            start = newstart;
        }
    }
    return found;
}

bool
checkBlockerCrossing(Image<unsigned char>* blocker, Point start, Point end)
{
    if (blocker == NULL) return false;
    Point delta(start.x > end.x ? -1 : (start.x == end.x ? 0 : 1), start.y > end.y ? -1 : (start.y == end.y ? 0 : 1));
    if (debugcrossing) {
        printf("check: %d->%d (%d,%d)->(%d,%d) by (%d,%d) = ", 
               dci->getPixel(start.x, start.y), dci->getPixel(end.x, end.y),
               start.x, start.y,
               end.x, end.y,
               delta.x, delta.y);
    }
    while ((start.x != end.x) || (start.y != end.y)) {
        if (blocker->getPixel(start.x, start.y) > 200) {
            if (debugcrossing) printf("CROSSES\n");
            return true;
        }
        start.x += delta.x;
        start.y += delta.y;
    }
    if (debugcrossing) printf("OK\n");
    return false;
}


extern int showalmost;

//white= 255;
//black = 0;
int main( int argc, char* argv[] )
{
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
    for (int y=0; y<pImage->getHeight(); y++) {
	for (int x=0; x<pImage->getWidth(); x++) {
            if (pImage->isNearWhite(pImage->getPixel(x, y))) {
                regions->setPixel(x, y, 0);
            } else {
                regions->setPixel(x, y, 1);
            }
	}
    }

    tiled = new Image<int>(pImage->getWidth(), pImage->getHeight());
    tiled->fill(0x0ffffff);
    Image<unsigned char>* blocker = NULL;

    // zap singleton pixels
    for (int y=0; y<regions->getHeight(); y++) {
	for (int x=0; x<regions->getWidth(); x++) {
            if (regions->getPixel(x, y) == 1) {
                int count = 0;
                for (int i=0; i<8; i++) {
                    int xx, yy;
                    if (regions->getNeighbor(x, y, i, xx, yy)) {
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

    RegionList* allRegions = regions->createRegions();

    for (int i=0; i<4; i++) {
        nearest[i].deltax = directions[i].x;
        nearest[i].deltay = directions[i].y;
    }

    if (recttest) {
        if (allRegions->numRegions() < 100) {
            saveimage("one", regions, allRegions->numRegions(), allRegions);
            rectize(regions, allRegions);
        }
        exit(0);
    }

    // initial letter joining.  Join all regions which face the same region on more than one axis
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        list->findPerimeter(regions, allRegions);
        std::vector<int> targets;
        if (list->countMultipleSides(regions, &targets) > 1) {
            std::vector<Distance> connectors;
            for (std::vector<int>::iterator it = targets.begin(); it != targets.end(); ++it) {
                int tid = *it;
                regions->findConnectors(list, tid, connectors);
            }
            showsize = 0;
            connectRegions(regions, allRegions, list, connectors);
            showsize = 0;
        }
    }
    checkmatch("before letters", allRegions, regions);

    saveSides(regions, allRegions);
    saveimage("two", regions, allRegions->numRegions(), allRegions);

    printf("Connecting letters\n");
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        list->PerimValid(false);
    }

    dci = regions;              // for debugcrossing
    int mindmax = regions->getWidth()+regions->getHeight();
    bool firsttime = true;

    for (int reaching=0; reaching <= 1; reaching++) {
        for (int updown=0; updown<4; updown++) {
            for (int ltr=0; ltr<7; ltr++) {
                //if (allRegions->numRegions() < 45) debugcrossing = 1;
                int debug = 0;
                for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                    Region* list = allRegions->current(); // list of points in this region
                    list->findPerimeter(regions, allRegions);
                    //if ((list->id == 2045)||(list->id == 1659)) debug=1; else debug=0;
                    //if ((list->id == 826)) debug=1; else debug=0;
                    int inx = 0;
                    int iny = 0;
                    int mind = mindmax;
                    struct { int i; int distance; } mins[4];
                    int set=0;
                    for (int d=0; d<4; d++) {
                        mins[d].distance = regions->getWidth()+regions->getHeight();
                        mind = mindmax;
                        for (int i=0; i<list->distances[d].size(); i++) {
                            if (0) {
                                if (list->distances[d][i].start == list->distances[d][i].end) continue;
                                int dx = directions[d].x;
                                int dy = directions[d].y;
                                Point p = list->distances[d][i].start;
                                for (int j=1; (p.x+(dx*j) != list->distances[d][i].end.x)||(p.y+(dy*j) != list->distances[d][i].end.y); j++) {
                                    assert(regions->getPixel(p.x+(dx*j), p.y+(dy*j)) == 0);
                                }
                            }
                            int dist = list->distances[d][i].end.distance(list->distances[d][i].start);
                            if (checkBlockerCrossing(blocker, list->distances[d][i].start, list->distances[d][i].end)) continue;
                            if (dist == 0) continue;
                            if (dist < mind) {
                                set |= (1 << d);
                                mind = dist;
                                mins[d].i = i;
                                mins[d].distance = dist;
                            }
                        }
                        if (directions[d].x == 0) inx += mind; else iny += mind;
                    }
                    if (debug && reaching) printf("%d -> set:%1x inx:%d iny:%d  ", list->id, set, inx, iny);
                    switch (set) {
                    case 0:
                        if (debug) printf("%d -> had all bad crossings\n", list->id);
                        break;

                    case 1:
                    case 2:
                    case 4:
                    case 8:
                    case 5:
                    case 10:
                    case 3:
                    case 6:
                    case 9:
                    case 12:
                        if (reaching) {
                            iny -= mindmax;
                            inx -= mindmax;
                        } else {
                            if (debug) printf("Two or more missing: %d\n", set); 
                            set=0;
                        }
                        break;

                    case 13:
                    case 14:
                        iny -= mindmax;
                        inx = inx/2;
                        break;

                    case 11:
                    case 7: 
                        inx -= mindmax;
                        iny = iny/2;
                        break;

                    case 15:
                        break;
                    }
                    if (debug) 
                        printf("-> set:%1x inx:%d iny:%d -> ", set, inx, iny);
                    if (set == 0) {
                        if (debug) printf(" FAIL\n");
                        continue;
                    }
                    int j = -1;
                    if (iny+(5*(1-reaching)) <= inx) {
                        // link left and right
                        j=0;
                        if (mins[1].distance < mins[0].distance) j=1;
                    } else if (inx+(5*(1-reaching)) <= iny) {
                        // link up and down
                        j=2;
                        if (mins[3].distance < mins[2].distance) j=3;
                    } else {
                        if (debug) printf("%d too close to call %d & %d\n", list->id, inx, iny);
                    }
                    if (debug) 
                        printf("-> j:%d mind:%d -> %d\n", 
                               j, 
                               (j == -1) ? 999999 : mins[j].distance, 
                               (j == -1) ? 999999 : regions->getPixel(list->distances[j][mins[j].i].end.x, list->distances[j][mins[j].i].end.y));
                    if (j >= 0) {
                        // we have a close region
                        if ((((updown & 1)==0)&&(j<=1))||((((updown & 1)==1)&&(j>=2)))) {
                            if (mins[j].distance > 70+(reaching*70)) {
                                if (reaching || debug) printf("NOT connecting %d & %d across %d\n", 
                                                  list->id,
                                                  regions->getPixel(list->distances[j][mins[j].i].end.x, list->distances[j][mins[j].i].end.y),
                                                  mins[j].distance);
                            
                            } else {
                                if (debug) printf("Connecting letters %d to %d from  %d of %d distance:%d\n",  
                                                  j,
                                                  list->id,
                                                  regions->getPixel(list->distances[j][mins[j].i].end.x, list->distances[j][mins[j].i].end.y),
                                                  list->distances[j][mins[j].i].end.distance(list->distances[j][mins[j].i].start),
                                                  mins[j].distance);
                                std::vector<Distance> connectors;
                                connectors.push_back(list->distances[j][mins[j].i]);
                                connectRegions(regions, allRegions, list, connectors);
                            }
                        }
                    }
                }
                saveimage("letter", regions, allRegions->numRegions(), allRegions);
                showbb(regions, allRegions);
                checkmatch("loop", allRegions, regions);
                printf("After letter %d we have %d regions\n", ltr, allRegions->numRegions());
                // check for being contained completely in anothers bounding box
                checkBBcontainment(regions, allRegions);
            }

            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                list->PerimValid(false);
            }

            if (firsttime)
                {
                    firsttime = false;

                    if (0) {
                        ldebug =1;
                        si=1;
                    }

                    // now look at left/right stops to find regions
                    int debug = 0;
                    Image<int>* ls = new Image<int>(regions->getWidth(), regions->getHeight());
                    ls->fill(0);
                    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                        Region* r = allRegions->current(); // list of points in this region
                        for (RPIterator it = r->begin(); it != r->end(); ++it) {
                            Point p = *it;
                            ls->setPixel(p.x, p.y, 0x08b8989);
                        }
                    }

                    Set<int> done;
                    Hash<int,int> left;
                    Hash<int,int> right;
                    for (int y=0; y<regions->getHeight(); y++) {
                        for (int x=0; x<regions->getWidth(); x++) {
                            int c = regions->getPixel(x, y);
                            if (c == 0) continue;
                            if (done.exists(c)) continue;
                            done.insert(c);
                            Region* list = allRegions->get(c);
                            Point ul = list->getUL();
                            Point lr = list->getLR();
                            if ((lr.x - ul.x) <= Lines::xIndentForLine) {
                                // region too narrow to include in anything
                                continue;
                            }
                            // look down to next region 
                            int xx = ul.x+Lines::xIndentForLine;
                            if (xx > regions->getWidth()) xx = regions->getWidth()-Lines::xSideIndentForLine;
                            for (int yy=lr.y+1; yy<regions->getHeight(); yy++) {
                                int cc = regions->getPixel(xx, yy);
                                if (cc == 0) continue;
                                Region* down = allRegions->get(cc);
                                int d = down->getUL().x - ul.x;
                                if (d < 0) d = -d;
                                if (d < 10) {
                                    // we have two regions which line up vertically along their leftmost edge
                                    if (debug) printf("Left: %d & %d (%d,%d) above (%d,%d)\n", list->id, down->id, ul.x, ul.y, down->getUL().x, down->getUL().y);
                                    left.insert(c, cc);
                                }
                                break;
                            }
                            // look down to next region 20 point in from right
                            xx = lr.x-Lines::xIndentForLine;
                            if (xx < 0) xx = Lines::xSideIndentForLine;
                            for (int yy=lr.y+1; yy<regions->getHeight(); yy++) {
                                int cc = regions->getPixel(xx, yy);
                                if (cc == 0) continue;
                                Region* down = allRegions->get(cc);
                                int d = down->getLR().x - lr.x;
                                if (d < 0) d = -d;
                                if (d < 10) {
                                    // we have two regions which line up vertically along their rightmost edge
                                    right.insert(c, cc);
                                    if (debug) 
                                        printf("Right: %d & %d (%d,%d) above (%d,%d)\n", 
                                               list->id, down->id, lr.x, lr.y, down->getLR().x, down->getLR().y);
                                } else {
                                    // we have a misalignment. Need to stop searcing
                                    if (ldebug) printf("Misalignment between %d (rx:%d) & %d (rx:%d)\n", 
                                                       list->id, lr.x, down->id, down->getLR().x);
                                }
                                break;
                            }
                        }
                    }
                    // now lets draw the boxes
                    int linew = 11;
                    int colordiff = 0xfffff3/allRegions->numRegions();
                    int regionid = 0x800000;
                    for (int y=0; y<regions->getHeight(); y++) {
                        for (int x=0; x<regions->getWidth(); x++) {
                            int c = regions->getPixel(x, y);
                            if (c == 0) continue;
                            int nl;
                            if (left.exists(c)) {
                                std::vector<Lines*> connected;
                                nl = processInterLine(c, left, allRegions, connected, 0);
                                if (nl) {
                                    for (std::vector<Lines*>::iterator it = connected.begin(); it != connected.end(); ++it) {
                                        // connect and draw this set of lines
                                        connectLines(*it, regions, allRegions, ls, regionid);
                                        regionid += colordiff;
                                    }
                                }
                            }
                            if (right.exists(c)) {
                                std::vector<Lines*> connected;
                                nl = processInterLine(c, right, allRegions, connected, 1);
                                if (nl) {
                                    for (std::vector<Lines*>::iterator it = connected.begin(); it != connected.end(); ++it) {
                                        // connect and draw this set of lines
                                        connectLines(*it, regions, allRegions, ls, regionid);
                                        regionid += colordiff;
                                    }
                                }
                            }
                        }
                    }
                    if (si) ls->PPMout(imageFilename("boundary", "ppm"));
                    delete ls;
                    Hash<int, int> remap;
                    for (std::vector<Lines*>::iterator it = Lines::all.begin(); it != Lines::all.end(); ++it) {
                        if (ldebug) saveimage("inter", regions, allRegions->numRegions(), allRegions);
                        Lines* l = *it;
                        l->connect(regions, allRegions, remap);
                    }
                    saveimage("bcon", regions, allRegions->numRegions(), allRegions);
                    if (0) {
                        ldebug = 0;
                        si=0;
                    }
                }

            printf("After lineup: %d regions\n", allRegions->numRegions());

            tiled = findBlockingTiles(regions, tiled, allRegions);

            updateFillBlocker(blocker, allRegions, regions);

            modifyBlocker(blocker, tiled);
            if (blocker && si) blocker->PGMout(imageFilename("blocker-modified", "pgm"));


            {
                int debug = 0;
                // now check for words
                for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                    Region* list = allRegions->current(); // list of points in this region
                    list->findPerimeter(regions, allRegions);
                    Point ul = list->getUL();
                    Point lr = list->getLR();
                    double aspect = (double)(lr.x-ul.x)/(double)(lr.y-ul.y);
                    if (aspect > 1.5) {
                        int mind = mindmax;
                        struct { int i; int distance; } mins[2];
                        int set=0;
                        for (int d=0; d<2; d++) {
                            mins[d].distance = mindmax;
                            mind = mindmax;
                            for (int i=0; i<list->distances[d].size(); i++) {
                                if (0) {
                                    if (list->distances[d][i].start == list->distances[d][i].end) continue;
                                    int dx = directions[d].x;
                                    int dy = directions[d].y;
                                    Point p = list->distances[d][i].start;
                                    for (int j=1; (p.x+(dx*j) != list->distances[d][i].end.x)||(p.y+(dy*j) != list->distances[d][i].end.y); j++) {
                                        assert(regions->getPixel(p.x+(dx*j), p.y+(dy*j)) == 0);
                                    }
                                }
                                int dist = list->distances[d][i].end.distance(list->distances[d][i].start);
                                if (dist == 0) continue;
                                if (checkBlockerCrossing(blocker, list->distances[d][i].start, list->distances[d][i].end)) continue;
                                if (dist < mind) {
                                    set |= (1 << d);
                                    mind = dist;
                                    mins[d].i = i;
                                    mins[d].distance = dist;
                                }
                            }
                        }
                        if (mind == mindmax) {
                            if (ldebug) printf("Did not find any close region to %d\n", list->id);
                            continue;
                        }
                        int j=0; 
                        if (mins[1].distance < mins[0].distance) j=1;
                        if (mins[j].distance > 70+(reaching*70)) {
                            if (debug) printf("NOT connecting words %d & %d across %d\n", 
                                              list->id,
                                              regions->getPixel(list->distances[j][mins[j].i].end.x, list->distances[j][mins[j].i].end.y),
                                              mins[j].distance);
                        } else {
                            if (debug) printf("Connecting words %d to %d from %d of %d distance:%d\n",  
                                              j,
                                              list->id,
                                              regions->getPixel(list->distances[j][mins[j].i].end.x, list->distances[j][mins[j].i].end.y),
                                              list->distances[j][mins[j].i].end.distance(list->distances[j][mins[j].i].start),
                                              mins[j].distance);
                            std::vector<Distance> connectors;
                            connectors.push_back(list->distances[j][mins[j].i]);
                            connectRegions(regions, allRegions, list, connectors);
                        }
                    } else {
                        if (debug) printf("Not trying region %d with aspect: %g\n", list->id, aspect);
                        if (isnan(aspect)) printf("NAN from %d (%d,%d) and (%d,%d)\n", list->id, ul.x, ul.y, lr.x, lr.y);
                        assert (!isnan(aspect));
                    }
                }
                saveimage("word", regions, allRegions->numRegions(), allRegions);
                printf("After word: %d regions\n", allRegions->numRegions());
            }
            showbb(regions, allRegions);

            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                list->PerimValid(false);
            }

            if (0)
                {
                    // now look at regions and try and match them by linesize
                    Image<int>* ls = new Image<int>(regions->getWidth(), regions->getHeight());
                    ls->fill(0);
                    int colordiff = 0xfffff3/allRegions->numRegions();
                    int regionid = 0x800000;
                    Set<int> done;
                    for (int y=0; y<regions->getHeight(); y++) {
                        for (int x=0; x<regions->getWidth(); x++) {
                            int c = regions->getPixel(x, y);
                            if (c == 0) continue;
                            if (done.exists(c)) continue;
                            done.insert(c);
                            Region* list = allRegions->get(c);
                            Point ul = list->getUL();
                            Point lr = list->getLR();
                            ls->drawBox(ul, lr, regionid, 2);
                            Set<int> downs;
                            std::vector<Distance> dists = list->distances[3];
                            for (int i=0; i<dists.size(); i++) {
                                int dist = dists[i].end.distance(dists[i].start);
                                if (dist == 0) continue;
                                int target = regions->getPixel(dists[i].end.x, dists[i].end.y);
                                assert(target != 0);
                                if (downs.exists(target)) continue;
                                downs.insert(target);
                            }
                            int mind = 10000;
                            int maxd = 0;
                            int sumd = 0;
                            int count = 0;
                            printf("R%d: ", list->id);
                            for (SetIterator<int> it(downs); !it.isEnd(); it.next()) {
                                int tid = it.current();
                                Region* t = allRegions->get(tid);
                                Point tul = t->getUL();
                                int d2d = tul.y-ul.y;
                                printf(" %d", d2d);
                                if (d2d < mind) mind = d2d;
                                if (d2d > maxd) maxd = d2d;
                                sumd += d2d;
                                count++;
                            }
                            if (count == 0) {
                                printf(" ====> NOTHING\n");
                                continue;
                            }
                            printf(" ===> %d %g %d\n", mind, (double)sumd/count, maxd);
                            char buffer[128];
                            if (count == 0)
                                sprintf(buffer, "%d", lr.y-ul.y);
                            else if (count == 1)
                                sprintf(buffer, "%d/%d", lr.y-ul.y, maxd);
                            else if (count == 2)
                                sprintf(buffer, "%d/%d+%d", lr.y-ul.y, mind, sumd/count);
                            else 
                                sprintf(buffer, "%d/%d+%d+%d", lr.y-ul.y, mind, sumd/count, maxd);

                            a12->draw(ls, buffer, regionid, ul.x+5, ul.y+5, 0.4);
                            regionid += colordiff;
                        }
                    }
                    ls->PPMout(imageFilename("line", "ppm"));
                    delete ls;
                }

        }
    }

    if (0) {
        int numreg = allRegions->numRegions();
        fill = NULL;
        int zeroConn = 3000;

        double bfdiv=100;
        bool doFBupdate = false;
        for (int endSize = 512; endSize<4100; endSize = endSize<<1) {
            for (int ep=30; ep<55; ep+=5) {
                double bf=(ep-25)/bfdiv;
                for (int ms=8; ms<endSize; ms = ms << 1) {
                    char buffer[128];
                    sprintf(buffer, "sym-%d-%d", ep, ms);
                    while (connregions(allRegions, regions, blocker, ms, ep-1, ep, bf, Sym) > 0) {
                        doFBupdate = true;
                        saveimage(buffer, regions, allRegions->numRegions(), allRegions);
                    }
                    bf += .3;
                }
            }
            if (doFBupdate) updateFillBlocker(blocker, allRegions, regions);
            doFBupdate = false;

            checkmatch("master for size", allRegions, regions);
            if (endSize == 512) {
                connregions(allRegions, regions, blocker, 512, 32, 33, 0, Two);
                saveimage("two-512-30", regions, allRegions->numRegions(), allRegions);
            }
        
            saveSmall(regions, allRegions, 256);
            saveSmall(regions, allRegions, 512);
            saveSmall(regions, allRegions, 756);

            if (fill) fill->PPMout(imageFilename("fill", "ppm"));
            if (blocker) blocker->PGMout(imageFilename("blocker", "pgm"));
            bfdiv = bfdiv/2;

            tiled = findBlockingTiles(regions, tiled, allRegions);
            tiled->PPMout(imageFilename("tiled", "ppm"));

            modifyBlocker(blocker, tiled);
            if (blocker) blocker->PGMout(imageFilename("blocker-modified", "pgm"));

            genSizeImage(regions, allRegions);
        }

        for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
            Region* list = allRegions->current(); // list of points in this region
            if (list == NULL) continue;
            Set<Region*>* nbrs = list->getNeighbors(regions, allRegions);
            printf("Region %d ->", list->id);
            for (SetIterator<Region*> it(*nbrs); !it.isEnd(); it.next()) {
                Region* p = it.current();
                printf(" %d", p->id);
            }
            printf("\n");
        }

        bfdiv=100;
        for (int endSize = 512; endSize<(1<<16)+1; endSize = endSize<<1) {
            for (int ep=40; ep<65; ep+=5) {
                double bf=(ep-30)/bfdiv;
                for (int ms=endSize>>3; ms<endSize; ms = ms << 1) {
                    char buffer[128];
                    sprintf(buffer, "sym-%d-%d", ep, ms);
                    while (connregions(allRegions, regions, blocker, ms, ep-1, ep, bf, Sym) > 0) {
                        saveimage(buffer, regions, allRegions->numRegions(), allRegions);
                        doFBupdate = true;
                    }
                    bf += .3;
                }
            }

            saveSmall(regions, allRegions, 1024);

            if (doFBupdate) updateFillBlocker(blocker, allRegions, regions);
            doFBupdate = false;

            if (fill) fill->PPMout(imageFilename("fill", "ppm"));
            if (blocker) blocker->PGMout(imageFilename("blocker", "pgm"));
            bfdiv = bfdiv/2;

            tiled = findBlockingTiles(regions, tiled, allRegions);
            tiled->PPMout(imageFilename("tiled", "ppm"));

            modifyBlocker(blocker, tiled);
            if (blocker) blocker->PGMout(imageFilename("blocker-modified", "pgm"));

            connregions(allRegions, regions, blocker, endSize>>2, 65, 66, 1, Two);
            saveimage("two", regions, allRegions->numRegions(), allRegions);
        
            genSizeImage(regions, allRegions);
            checkmatch("master for size", allRegions, regions);

            updateFillBlocker(blocker, allRegions, regions);
            doFBupdate = false;
        }

        showalmost = 1;
        double extra = 0.0;
        for (int d=40; d<80; d++) {
            if (connregions(allRegions, regions, blocker, 0, d, d+1, 1.4+extra, Asym) > 0) {
                char buffer[128];
                sprintf(buffer, "final-%d", d);
                saveimage(buffer, regions, allRegions->numRegions(), allRegions);
                doFBupdate = true;
            }
            extra += 0.2;

            if ((d%5)==0) {
                if (blocker) blocker->PGMout(imageFilename("blocker", "pgm"));
                tiled = findBlockingTiles(regions, tiled, allRegions);
                tiled->PPMout(imageFilename("tiled", "ppm"));
                modifyBlocker(blocker, tiled);
                if (blocker) blocker->PGMout(imageFilename("blocker-modified", "pgm"));
            }

            if (doFBupdate) updateFillBlocker(blocker, allRegions, regions);
            doFBupdate = false;
        }
    }

    tiled = findBlockingTiles(regions, tiled, allRegions);
    tiled->PPMout(imageFilename("tiled", "ppm"));

    genSizeImage(regions, allRegions);

    Image<int>* afterRegions = recolor(regions, allRegions);
    checkRecolor("ar1", afterRegions, allRegions);
    
    char filename[128];
    sprintf(filename, "%s-region-last.ppm", outname);
    afterRegions->PPMout(filename);

    checkRecolor("ar2", afterRegions, allRegions);

    Image<int>* afterRegionsP = makePerimImage(afterRegions, allRegions);
    sprintf(filename, "%s-perim.ppm", outname);
    afterRegionsP->PPMout(filename);
    delete afterRegionsP;

    int unknown=0;
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current();
        if (list->getLineHeight() == 0) unknown++;
        else printf("%d:\t%g\n", list->id, list->getLineHeight());
        char buffer[128];
        sprintf(buffer, "%6.2g", list->getLineHeight());
        list->addInfo(afterRegions, a12, buffer);
        
    }
    if (unknown > 0) printf("Unknown LineHeight: %d\n", unknown);
    sprintf(filename, "%s-region-lh.ppm", outname);
    afterRegions->PPMout(filename);
    delete afterRegions;

    fill = recolor(regions, allRegions);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        fillminmax(fill, list, 0.1);
        //aspectfill(fill, list);
    }
    sprintf(filename, "%s-fill.ppm", outname);
    fill->PPMout(filename);
    Image<int>* pfill = makePerimImage(fill, allRegions);
    sprintf(filename, "%s-perim-fill.ppm", outname);
    pfill->PPMout(filename);
    delete pfill;
    delete fill;

    Image<int>* perim = rectize(regions, allRegions);
    sprintf(filename, "%s-rectperim.ppm", outname);
    perim->PPMout(filename);
    
    // lets output a descriptor
    {
        sprintf(filename, "%s-desc.txt", outname);
        FILE* fp = fopen(filename, "wb");
        assert(fp);                 // should be better way
        fprintf(fp, "%d regions (%d,%d)\n", allRegions->numRegions(), pImage->getWidth(), pImage->getHeight());
        for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
            Region* list = allRegions->current(); // list of points in this region
            double lh = list->getLineHeight();
            Point ul = list->getUL();
            Point lr = list->getLR();
            bool single = false;
            if (lh == 0) {
                // single line region?
                lh = lr.y-ul.y;
                single = true;
            }
            int nb = list->boundary.size();
            fprintf(fp, "%d: (%d,%d) (%d,%d) %6.3f %s %d\n", list->id, ul.x, ul.y, lr.x, lr.y, lh, single ? "one" :"multi", nb);
            for (int i=0; i<nb; i++) {
                Point p = list->boundary[i];
                fprintf(fp, "(%d,%d)\n", p.x, p.y);
            }
        }
        fclose(fp);
    }

    sprintf(filename, "%s.pgm", outname);
    pImage->PGMout(filename);
    if (blocker) delete blocker;
    delete allRegions;
    delete pImage;
    delete regions;
    delete tiled;
    delete a12;
}

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

