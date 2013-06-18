#include "image.h"
#include "arg.h"
#include <unordered_set>
#include <set>
#include "histogram.h"
#include "font.h"

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
checkVaspect(Image<Pixel>* img, Pixel rid, Point ul, Point lr, Pelem* columns, Pelem* rows, int delta)
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
            if ((area / (double)(bottom-(y+1))) < 10.0) {
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
fillminmax(Image<Pixel>* img, Region* list)
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

    checkVaspect(img, rid, ul, lr, columns, rows, 1);
    checkVaspect(img, rid, ul, lr, columns, rows, -1);

    if (0) {
    // fill in all edge black areas where the area is much larger then the opening
    // and only rectilinear ones
    for (int y=ul.y; y<lr.y; y++) {
        // first do min to right
        int maxx = columns[y].min+10;
        if (maxx > columns[y].max) maxx = columns[y].max;
        for (int x=columns[y].min; x<=maxx; x++) {
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
                for (xx=x; xx<lr.x; xx++) {
                    Pixel c = img->getPixel(xx, yy);
                    if (c == 0) {
                        area++;
                        continue;
                    }
                    if (c == rid) break;
                    fail = true;
                }
                if (xx > endx) endx = xx;
                //printf("Got as far as (%d,%d) maxx=%d area=%g\n", xx, yy, endx, area);
            }
            if (fail) continue;
            if ((area / (double)(bottom-(y+1))) < 10.0) {
                if (fmmdebug) printf("Skipping (%d,%d) to (%d,%d) too small - %g compared to %d\n", x, y+1, endx, bottom, area, bottom-(y+1));
                continue;
            }
            // now check top and bottom are also in rid
            int tx;
            for (tx=x; tx<endx; tx++) if (img->getPixel(tx, y) != rid) break;
            if (tx < endx) break;
            for (tx=x; tx<endx; tx++) if (img->getPixel(tx, bottom) != rid) break;
            if (tx < endx) break;

            // gap is clear - fill it in
            if (fmmdebug) printf("Filling gap from (%d,%d) to (%d,%d) %g vs. %d\n", x, y+1, endx, bottom, area, bottom-y+1);
            for (int yy=y+1; yy<bottom; yy++) {
                for (int xx=x; xx<endx; xx++) {
                    if (img->getPixel(xx, yy) == 0) img->setPixel(xx, yy, rid);
                    else break;
                }
            }
        }
    }
    }
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



enum Move { LEFT = 0, RIGHT = 1, DOWN = 2, UP = 3, SaddleRight = 4, SaddleLeft = 5, ILLEGAL=6 };
static const char* m2s[] = {
    "left", "right", "down", "up", "sr", "sl", "??"
};
static Point pointOrder[] = {
//    UL ,    UR ,   LR  ,   LL
    {0,0}, {1, 0}, {1, 1}, {0, 1}
};
static Move newMove[] = {
    ILLEGAL,                    // 0 - none
    LEFT,                       // 1 - UL
    UP,                         // 2 - UR
    LEFT,                       // 3 - UL & UR
    RIGHT,                      // 4 - LR
    SaddleRight,                // 5 - UL, LR
    UP,                         // 6 - UR & LR
    LEFT,                       // 7 - UL, UR, LR
    DOWN,                       // 8 - LL
    DOWN,                       // 9 - UL & LL
    SaddleLeft,                 // 10 - UR & LL
    DOWN,                       // 11 - UL, UR, LL
    RIGHT,                      // 12 - LR & LL
    RIGHT,                      // 13 - UL, LR, LL
    UP,                         // 14 - UR, LR, LL
    ILLEGAL,                    // 15 - ALL
};
    
template<class Pixel>
void
showsub(Image<Pixel>* img, Point p, Pixel rid, int side = 8)
{
    int x = p.x - side;
    int y = p.y - side;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    printf("Around %d,%d\n", p.x, p.y);
    for (int i=0; i<side*2+1; i++) {
        if (y+i >= img->getHeight()) continue;
        for (int j=0; j<side*2+1; j++) {
            if (x+j >= img->getWidth()) break;
            Pixel c = img->getPixel(x+j, y+i);
            char q = '.';
            if ((x+j == p.x)&&(y+i == p.y)) q = '+';
            if ((c == rid)&&(q == '.')) q = 'x';
            if ((c == rid)&&(q == '+')) q = 'X';
            if ((c != rid)&&(c != 0)) q=',';
            printf("%c", q);
        }
        printf("\n");
    }
}

template<class Pixel>
void
makePerimeter(Image<Pixel>* dest, Image<Pixel>* src, Region* list)
{
    if (0) {
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            printf("(%d, %d)\t", p.x, p.y);
        }
        printf("\n");
    }
    int debug = 0;
    int timer = list->size()*4;
    Point start = list->getULPoint();
    start.x--;
    start.y--;
    Point p = list->get(0);
    int rid = src->getPixel(p.x, p.y);

    if (list->size() < 2) {
        printf("Region %d %d,%d is of size < 2", rid, start.x, start.y);
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            printf("(%d, %d)\t", p.x, p.y);
        }
        printf("\n");
        return;
    }
    //printf("make perimeter for %d\n", rid);
    Point walker(start.x, start.y);
    Move last = RIGHT;
    int w = src->getWidth();
    int h = src->getHeight();
    int moved = 0;
    int info = 0;
    //if (list->size() < 50) info = 50;
    Point special;
    while ((!(moved == 0x0f)) || ((walker.x != start.x)||(walker.y != start.y))) {
        if (timer-- <= 0) {
            fprintf(stderr, "We did more than 4x the points for region %d", rid);
            return;
        }
        if (debug) {
            if (info > 0) {
                showsub(src, walker, rid, 5);
                info--;
            }
        }
        int idx = 0;
        for (int i=0; i<4; i++) {
            int x = walker.x+pointOrder[i].x;
            int y = walker.y+pointOrder[i].y;
            if ((x < 0)||(y < 0)||(x >= w)||(y >= h)) continue;
            idx += (((src->getPixel(x, y) == rid) ? 1 : 0) << i);
        }
        moved |= idx;
        Move m =newMove[idx]; 
        //printf("(%d, %d) -> %s %x\n", walker.x, walker.y, m2s[(int)m], moved);
        switch (m) {
        case SaddleLeft:
            info += 10;
            special = walker;
            if ((last == UP)||(last == RIGHT)) {
                special.x++;
                special.y++;
            }
            if (src->count8Neighbors(special.x, special.y, rid) == 1) {
                // special case for lone lower right pixel
                dest->setPixel(special.x, special.y, rid);
                if ((last == DOWN)||(last == LEFT)) m = DOWN;
                else m = RIGHT;
            } else {
                switch (last) {
                case UP: m = LEFT; break;
                case DOWN: m = LEFT; break;
                case LEFT: m = DOWN; break;
                case RIGHT: m = UP; break;
                default: assert(0);
                }
            }
            break;

        case SaddleRight:
            info += 10;
            special = walker;
            if ((last == DOWN)||(last == LEFT)) {
                special.x++;
                special.y++;
            }
            if (src->count8Neighbors(special.x, special.y, rid) == 1) {
                // special case for lone lower right pixel
                dest->setPixel(special.x, special.y, rid);
                if ((last == DOWN)||(last == LEFT)) m = LEFT;
                else m = UP;
            } else {
                switch (last) {
                case UP: m = LEFT; break;
                case DOWN: m = RIGHT; break;
                case LEFT: m = DOWN; break;
                case RIGHT: m = DOWN; break;
                default: assert(0);
                }
                break;
            }
        default:
            break;
        }
        switch (m) {
        case UP:
            dest->setCircle(walker.x+1, walker.y, rid, 4);
            walker.y--;
            break;

        case DOWN:
            dest->setCircle(walker.x, walker.y+1, rid, 4);
            walker.y++;
            break;

        case LEFT:
            dest->setCircle(walker.x, walker.y, rid, 4);
            walker.x--;
            break;

        case RIGHT:
            dest->setCircle(walker.x+1, walker.y+1, rid, 4);
            walker.x++;
            break;
        case ILLEGAL:
        default:
            printf("timer:%d (%d, %d) -> now:%s last:%s rid:%d\n", timer, walker.x, walker.y, m2s[(int)m], m2s[(int)last], rid);
            showsub(src, walker, rid, 10);
            printf("Illegal move in region %d %d,%d of %ld", rid, start.x, start.y, list->size());
            int count=100;
            for (RPIterator it = list->begin(); it != list->end(); ++it) {
                Point p = *it;
                printf("(%d, %d)\t", p.x, p.y);
                if (count-- <= 0) break;
            }
            printf("\n");
            assert(0);
        }
        last = m;
    }
}    

template<class Pixel>
Image<Pixel>* 
makePerimImage(Image<Pixel>* img, RegionList* allRegions)
{
    Image<Pixel>* target = new Image<Pixel>(img->getWidth(), img->getHeight());
    target->fill(0x00ffffff);
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        if (list == NULL) continue;
        makePerimeter(target, img, list);
    }
    return target;
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

static const int fbtdebug = 0;
static const int blockingTileSize = 45;

Font* a12;

char*
imageFilename(const char* temp, const char* ext)
{
    static int id=0;
    static char filename[256];
    sprintf(filename, "%04d-%s.%s", id++, temp, ext);
    printf("Generating: %s\n", filename);
    return filename;
}

void
saveimage(const char* prompt, Image<int>* regions, int numreg, RegionList* allRegions, bool check=true) 
{
    if (!si) return;
    char* filename = imageFilename(prompt, "ppm");
    Image<int>* newregions = recolor(regions, allRegions);
    newregions->PPMout(filename);

    if (check) {
        checkmatch(prompt, allRegions, regions);
        checkRecolor(prompt, newregions, allRegions);
    }

    if (allRegions != NULL) {
        for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
            Region* list = allRegions->current(); // list of points in this region
            if (list == NULL) continue;
            list->label(newregions, a12, 0.2);
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


template<class Pixel>
Image<Pixel>* 
findBlockingTiles(Image<Pixel>* regions, Image<Pixel>* prevTile, RegionList* allRegions)
{
    // try to tile regions to discover
    int tsize = blockingTileSize;
    int reqtiles = 20;
    int numreg = allRegions->numRegions();
    Image<int>* tiled = recolor(regions, allRegions);
    checkRecolor("fbt", tiled, allRegions);
                
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
            if (prevTile->getPixel(x, y) != 0x00ffffff) continue;
            bool found = true;
            int offset = 1;
            for (int doit=0; found && (doit<2); doit++) {
                for (int i=0; i<tsize; i++) {
                    if (y+i >= tiled->getHeight()) break;
                    for (int j=0; j<tsize; j++) {
                        if (x+j >= tiled->getWidth()) break;
                        if (doit) {
                            tiled->setPixel(x+j, y+i, 0x00ffffff);
                        } else {
                            int c = tiled->getPixel(x+j, y+i);
                            if ((c != 0)&&(c != 0x00ffffff)||(prevTile->getPixel(x, y) != 0x00ffffff)) {
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
    if (si) tiled->PPMout("tiled.ppm");
    // now we eliminate all white regions with an area of less than reqtiles*tsize^2
    Image<unsigned char> check(tiled->getWidth(), tiled->getHeight());
    check.fill(0);
    int maxkillX = tiled->getWidth()/15;
    int maxkillY = tiled->getHeight()/15;
    for (int y=0; y<tiled->getHeight(); y++) {
        for (int x=0; x<tiled->getWidth(); x++) {
            if (tiled->getPixel(x, y) == 0x00ffffff) {
                Region* points = tiled->labelAndReturnRegion(x, y, 0x00ffffff, 1);
                if (points->size() < reqtiles*(tsize*tsize)) {
                    // too small
                    for (RPIterator it = points->begin(); it != points->end(); ++it) {
                        Point p = *it;
                        tiled->setPixel(p.x, p.y, 0x0ff0000);
                    }
                } else if (1) {
                    int debug = 0;
                    // get bounding box and eliminate all not half size
                    Point ul = points->get(0);
                    Point lr = points->get(0);
                    for (RPIterator it = points->begin(); it != points->end(); ++it) {
                        Point p = *it;
                        if (p.x < ul.x) ul.x = p.x;
                        if (p.y < ul.y) ul.y = p.y;
                        if (p.x > lr.x) lr.x = p.x;
                        if (p.y > lr.y) lr.y = p.y;
                    }
                    // set bounds
                    int minX = (lr.x-ul.x)/3;
                    int minY = (lr.y-ul.y)/3;
                    if (debug) printf("X:%d, Y:%d\n", minX, minY);
                    if (minX > maxkillX) minX = maxkillX;
                    if (minY > maxkillY) minY = maxkillY;
                    if (debug) printf("X:%d, Y:%d\n", minX, minY);
                    // see about bounds of each line
                    for (int y=ul.y; y<=lr.y; y++) {
                        int x;
                        if (debug) printf("Y=%d", y);
                        for (x=ul.x; x<=lr.x; x++) {
                            if (tiled->getPixel(x, y) == 1) break;
                        }
                        int start = x++;
                        for (; x<=lr.x; x++) {
                            int c = tiled->getPixel(x, y);
                            if (((c == 0)||(c == 1)||(c == 0x0ff0000))&&(start == -1)) {
                                start = x;
                                continue;
                            } 
                            if ((c != 1)&&(c != 0)&&(c != 0x0ff0000)&&(start != -1)) {
                                // line just ended
                                if (debug) printf(" [%d-%d]%d", start, x, c);
                                if ((x-start)>minX) {
                                    if (debug) printf("*");
                                    check.drawLine(start,y,x,y,1);
                                }
                                start = -1;
                            }
                        }
                        if (start != -1) {
                            if (debug) printf(" [%d-%d]", start, x);
                            if ((x-start)>minX) {
                                if (debug) printf("*");
                                check.drawLine(start,y,x,y,1);
                            }
                        }
                        if (debug) printf("\n");
                    }
                    for (int x=ul.x; x<=lr.x; x++) {
                        if (debug) printf("X=%d", x);
                        int y;
                        for (y=ul.y; y<=lr.y; y++) {
                            if (tiled->getPixel(x, y) == 1) break;
                        }
                        int start = y++;
                        for (; y<=lr.y; y++) {
                            int c = tiled->getPixel(x, y);
                            if (((c == 1)||(c==0)||(c == 0x0ff0000))&&(start == -1)) {
                                start = y;
                                continue;
                            } 
                            if ((c != 1)&&(c != 0)&&(c != 0x0ff0000)&&(start != -1)) {
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
                            tiled->setPixel(p.x, p.y, 0x000ff00);
                        }
                    }
                    
                }
                delete points;
            }
        }
    }
    // recolor back to white
    for (int y=0; y<tiled->getHeight(); y++) {
        for (int x=0; x<tiled->getWidth(); x++) {
            if (tiled->getPixel(x, y) == 1) {
                if (check.getPixel(x, y) == 1) 
                    tiled->setPixel(x, y, 0x00ffffff);
                else
                    tiled->setPixel(x, y, 0x000000ff);
            } 
        }
    }
    // now fill in regions which are checked, but weren't in original
    int maxAddX = tiled->getWidth()/20;
    int maxAddY = tiled->getHeight()/20;
    for (int y=1; y<tiled->getHeight(); y++) {
        for (int x=1; x<tiled->getWidth(); x++) {
            if ((tiled->getPixel(x, y) != 0x00ffffff)&&(check.getPixel(x,y) == 1)) {
                if (tiled->getPixel(x-1,y) == 0x00ffffff) {
                    int xx;
                    for (xx=x; xx<tiled->getWidth(); xx++) {
                        if (check.getPixel(xx, y) != 1) break;
                        if (tiled->getPixel(xx, y) == 0x00ffffff) break;
                    }
                    if ((xx-x) < maxAddX) {
                        for (int ax=x; ax<xx; ax++) tiled->setPixel(ax, y, 0x00ffffff);
                    }
                }
                if (tiled->getPixel(x,y-1) == 0x0ffffff) {
                    int yy;
                    for (yy=y; yy<tiled->getHeight(); yy++) {
                        if (check.getPixel(x, yy) != 1) break;
                        if (tiled->getPixel(x, yy) == 0x00ffffff) break;
                    }
                    if ((yy-y) < maxAddY) {
                        for (int ay=y; ay<yy; ay++) tiled->setPixel(x, ay, 0x00ffffff);
                    }
                }
            }
        }
    }
    
    if (si) tiled->PPMout(imageFilename("tiled1","ppm"));
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
                if (0) {
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
        if (intermediateFiles) blocker->PGMout(imageFilename("blocker2", "ppm"));
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
        if (0) {
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

    Lines(const char* desc, int side) {
        description = strdup(desc);
        this->side = side;
        all.push_back(this);
        edgex = -1;
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
        printf("CONNECT: "); for (int j=0; j<end; j++) printf(" %d", starts[j]->rid); printf("\n");
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
    Lines* lines = new Lines(buffer, side);

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
    while ((start.x != end.x) && (start.y != end.y)) {
        if (blocker->getPixel(start.x, start.y) > 200) return true;
        start.x += delta.x;
        start.y += delta.y;
    }
    return false;
}

static int
bysize(const void *a, const void *b)
{
    const Region *ra = *(const Region** ) a;
    const Region *rb = *(const Region** ) b;
    
    return ra->size() - rb->size();
}

static int
byarea(const void *a, const void *b)
{
    Region *ra = *( Region** ) a;
    Region *rb = *( Region** ) b;
    
    int ad =  ra->getArea()-rb->getArea();
    if (ad == 0) return bysize(a, b);
    return ad;
}

static int
bybb(const void *a, const void *b)
{
     Region *ra = *( Region** ) a;
     Region *rb = *( Region** ) b;
    
    int ad = ra->size() - rb->size();
    if (ad == 0) {
        ad =  ra->getArea()-rb->getArea();
        if (ad == 0) {
            double x = (double)ra->size()/(double)ra->getArea();
            double y = (double)rb->size()/(double)rb->getArea();
            ad = (int)(x - y);
        }
    }
    return ad;
}

static int
bybbx(const void *a, const void *b)
{
     Region *ra = *( Region** ) a;
     Region *rb = *( Region** ) b;

     int ad =  (ra->getLR().x - ra->getUL().x) - (rb->getLR().x - rb->getUL().x);
     if (ad == 0) {
         ad =  (ra->getLR().y - ra->getUL().y) - (rb->getLR().y - rb->getUL().y);
         if (ad == 0) ad = ra->size() - rb->size();
         if (ad == 0) {
             ad =  ra->getArea()-rb->getArea();
         }
     }
     return ad;
}

static int
byaspect(const void *a, const void *b)
{
    Region *ra = *( Region **) a;
    Region *rb = *( Region **) b;
    
    int ad =  ra->getArea()-rb->getArea();
    if (ad == 0) {
        ad = ra->size() - rb->size();
        if (ad == 0) {
            ad = (double)(ra->getLR().x - ra->getUL().x)/(double)(ra->getLR().y - ra->getUL().y) -
                (double)(rb->getLR().x - rb->getUL().x)/(double)(rb->getLR().y - rb->getUL().y);
        }
    }
    return ad;
}

// write all points in r to img using color.  place all at pos in img
template<class Pixel>
void
writeRegion(Image<Pixel>* img, Point pos, Region* r, Pixel color)
{
    Point delta = r->getUL();
    delta.x -= pos.x;
    delta.y -= pos.y;
    for (RPIterator it = r->begin(); it != r->end(); ++it) {
        Point p = *it;
        img->setPixel(p.x-delta.x, p.y-delta.y, color);
    }
}

int colorsize(Region* r)
{
    return r->size()/10;
}

int colorarea(Region* r)
{
    return r->getArea()/10;
}

int coloraspect(Region* r)
{
    return floor(30*((double)(r->getLR().x - r->getUL().x)/(double)(r->getLR().y - r->getUL().y)));
}

int colorbbx(Region* r)
{
    return r->getLR().x - r->getUL().x;
}

int colorbby(Region* r)
{
    return r->getLR().y - r->getUL().y;
}

void
showOrder(RegionList* allRegions, Region** rids, int count, int (*sortfunc)(const void* a, const void* b), const char* fname, int (*color)(Region* r))
{
    int colordiff = 0x00171;
    int regionid = 0x000000;
    int lastcolor = 0;
    qsort(rids, count, sizeof(Region*), sortfunc);
    int outw = 4000;
    int outh = 5000;
    int leftMargin = 4;
    Image<int>* output = new Image<int>(outw+leftMargin, outh);
    Image<int>* img = new Image<int>(pImage->getWidth(), pImage->getHeight());
    img->fill(0);
    output->fill(0x0ffffff);
    Point pos(leftMargin, 4);   // current output position
    int maxh = 0;               // max hieight of this line
    int interline = 10;
    int interchar = 5;
    bool filled=false;          // set too true when at end of line
    int i;
    for (i=0; !filled && i<count; i++) {
        Region* r = rids[i];
        int lw = r->getLR().x-r->getUL().x;
        int lh = r->getLR().y-r->getUL().y;
        if (lw+pos.x > outw) {
            // goto next line
            pos.x = leftMargin;
            pos.y = pos.y + interline +maxh;
            maxh = 0;
            if (pos.y > outh) {
                filled = true;
                break;
            }
        }
        if (lh+pos.y > outh) {
            // ran past end of page
            filled = true;
            break;
        }
        if (filled) break;
        //printf("%d %ld @ %d,%d\n", r->id, r->size(), pos.x, pos.y);
        int newcolor = color(r); // check to see if we should get a new color
        if (newcolor != lastcolor) {
            regionid += colordiff;
            lastcolor = newcolor;
        }
        writeRegion(output, pos, r, regionid);
        writeRegion(img, r->getUL(), r, regionid);
        pos.x += (lw+interchar);
        if (lh > maxh) maxh = lh;
    }
    printf("Output %d of %d\n", i, count);
    output->PPMout(imageFilename(fname, "ppm"));
    img->PPMout(imageFilename(fname, "ppm"));
    delete output;
    delete img;
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

    // get all regions in image
    RegionList* allRegions = regions->createRegions();

    for (int i=0; i<4; i++) {
        nearest[i].deltax = directions[i].x;
        nearest[i].deltay = directions[i].y;
    }

    Region** rlist = new Region*[allRegions->numRegions()];
    int nr = 0;
    Histogram area;
    Histogram aspect;
    Histogram size;
    // put all regions into rlist
    for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
        Region* list = allRegions->current(); // list of points in this region
        rlist[nr++] = list;
        area.set(list->getArea(), 1);
        aspect.set(floor(1000.0 * (double)(list->getLR().x - list->getUL().x)/(double)(1+list->getLR().y - list->getUL().y)));
        size.set(list->size());
    }

    area.print("area");
    aspect.print("aspect*1000");
    size.print("size");

    showOrder(allRegions, rlist, nr, bysize, "bysize", colorsize);
    showOrder(allRegions, rlist, nr, byarea, "area", colorarea);
    showOrder(allRegions, rlist, nr, bybbx, "bbx", colorbbx);
    showOrder(allRegions, rlist, nr, bybb, "bby", colorbby);
    showOrder(allRegions, rlist, nr, byaspect, "aspect", coloraspect);
}

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

