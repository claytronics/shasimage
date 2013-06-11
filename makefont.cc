#include "image.h"
#include "font.h"
#include "arg.h"
#include <unordered_set>
#include <set>

int edgelen = 100;

int verbose = 0;
const char* inname = "y.pgm";
const char* outname = "block";
int radius = 1;
char* intermediateFiles = NULL;
int maxPasses = 50;
int startPass = 20;
int startSize = 10;
int endSize = 1000;

ArgDesc argdescs[] = {
    { ArgDesc::Position, "", 0, ArgDesc::Pointer, &inname, ArgDesc::Optional },
    { ArgDesc::Flag, "--radius", 0, ArgDesc::Int, &radius, ArgDesc::Optional },
    { ArgDesc::Flag, "--start", 0, ArgDesc::Int, &startPass, ArgDesc::Optional },
    { ArgDesc::Flag, "--passes", 0, ArgDesc::Int, &maxPasses, ArgDesc::Optional },
    { ArgDesc::Flag, "--ss", 0, ArgDesc::Int, &startSize, ArgDesc::Optional },
    { ArgDesc::Flag, "--es", 0, ArgDesc::Int, &endSize, ArgDesc::Optional },
    { ArgDesc::Flag, "-v", 0, ArgDesc::Flag, &verbose, ArgDesc::Optional },
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
    int timer = list->size()*4;
    Point start = list->getUL();
    start.x--;
    start.y--;
    Point p = list->get(0);
    int rid = src->getPixel(p.x, p.y);
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
        if (0) {
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
            info = 10;
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
            info = 10;
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
checkmatch(const char* prompt, RegionList* allRegions, Image<Pixel>* regions)
{
    int fail = false;
    int cnt = 0;
    Image<Pixel>* checker = new Image<int>(regions);
    for (int i=0; i<allRegions->size(); i++) {
        Region* list = allRegions->get(i);
        if (list == NULL) continue;
        cnt++;
        for (RPIterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            if (checker->getPixel(p.x, p.y) != i) {
                printf("%s (%d, %d) should have %d and has %d\n", prompt, p.x, p.y, i, checker->getPixel(p.x, p.y));
                fail = true;
            }
            else 
                checker->setPixel(p.x, p.y, 0);
        }
    }
    if (cnt != allRegions->numRegions()) printf("%s: Found %d non-null regions out of %d\n", prompt, cnt, allRegions->numRegions());
    for (int y=0; y<checker->getHeight(); y++) {
        for (int x=0; x<checker->getWidth(); x++) {
            if (checker->getPixel(x, y) != 0) {
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
static const int blockingTileSize = 40;

template<class Pixel>
Image<Pixel>* 
findBlockingTiles(Image<Pixel>* regions, Image<Pixel>* prevTile, RegionList* allRegions)
{
    // try to tile regions to discover
    int tsize = blockingTileSize;
    int reqtiles = 12;
    int numreg = allRegions->numRegions();
    Image<int>* tiled = recolor(regions, numreg);
                
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
                for (int i=-8; i<8; i++) {
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
    tiled->PPMout("tiled.ppm");
    // now we eliminate all white regions with an area of less than reqtiles*tsize^2
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
                } else {
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
                tiled->setPixel(x, y, 0x00ffffff);
            }
        }
    }
    tiled->PPMout("tiled1.ppm");
    delete prevTile;
    return tiled;
}

Image<int>* fill;
Image<int>* tiled;
Image<unsigned char>* pImage;

template<class Pixel>
int
connregions(RegionList* allRegions, Image<Pixel>*& regions, Image<unsigned char>*& blocker, int maxRegionSize, int sd, int ed, double bm)
{
    printf("Try Upto %d (%d-%d)\n", maxRegionSize, sd, ed);
    int zeroConn = 3000;
    int numreg = allRegions->numRegions();
    for (int i=sd; i<ed; i++) {
        int iPass = 0;
        int mindist=regions->getHeight();
        int maxdist=0;
        int connections = 0;

        if (0 && (i > blockingTileSize)) {
            tiled = findBlockingTiles(regions, tiled, allRegions);
            char filename[128];
            sprintf(filename, "%s-%d-%d-1-tiled.ppm", intermediateFiles, i, iPass);
            tiled->PPMout(filename);
            
        }
        while (1) {
            printf("%d: %d regions: pass %d (%d)\n", i, numreg, iPass, connections);
            int changes = 0;

            if (intermediateFiles) {
                // recolor
                Image<int>* newregions = recolor(regions, numreg);

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
                if (list->size() > maxRegionSize) continue;
                Point p = list->get(0);
                int y = p.y;
                int x = p.x;
                int rid = regions->getPixel(x, y);
                printf("R:%d %ld\n", rid, list->size());
                //printf("%d, %d @ %d\n", x, y, rid);

                int sx, sy, xx, xy, xdist, xok;
                int tx, ty, yx, yy, ydist, yok;
                tx = sx = -1;                // for safety
                xok = regions->findNearestRegion(list, 0, sx, sy, xx, xy, xdist, maxallowed, blocker, bm);
                if (0 && xok) {
                    int targetRegionId = regions->getPixel(xx, xy);
                    if (allRegions->get(targetRegionId)->size() > maxRegionSize) {
                        printf("->%d %ld too big\n", targetRegionId, allRegions->get(targetRegionId)->size());
                        xok = false;
                    }
                }
                // (i < blockingTileSize) ? NULL : tiled);
                //printf("in X: (%d,%d)->(%d,%d) for %d %d->%d\n", sx, sy, xx, xy, xdist, regions->getPixel(sx, sy), regions->getPixel(xx, xy));
                yok = regions->findNearestRegion(list, 1, tx, ty, yx, yy, ydist, maxallowed, blocker, bm);
                if (0 && yok) {
                    int targetRegionId = regions->getPixel(yx, yy);
                    if (allRegions->get(targetRegionId)->size() > maxRegionSize) {
                        printf("->%d %ld too big\n", targetRegionId, allRegions->get(targetRegionId)->size());
                        yok = false;
                    }
                }
                //(i < blockingTileSize) ? NULL : tiled);
                //printf("in Y: (%d,%d)->(%d,%d) for %d %d->%d\n", tx, ty, yx, yy, ydist, regions->getPixel(tx, ty), regions->getPixel(yx, yy));
                int d;
                int targetRegionId;
                Point srcp;
                Point dstp;
                if (xok && (!yok || (xdist < ydist))) {
                    //printf("Merging up and down! %d,%d\n", xx, yy);
                    targetRegionId = regions->getPixel(xx, xy);
                    d = xdist;
                    pImage->drawLine(sx, sy, xx, xy, pImage->black());
                    regions->drawLine(sx, sy, xx, xy, targetRegionId);
                    allRegions->addLine2Region(sx, sy, xx, xy, targetRegionId);
                    srcp.x = sx;
                    srcp.y = sy;
                    dstp.x = xx;
                    dstp.y = xy;
                } else if (yok) {
                    targetRegionId = regions->getPixel(yx, yy);
                    d = ydist;
                    pImage->drawLine(tx, ty, yx, yy, pImage->black());
                    regions->drawLine(tx, ty, yx, yy, targetRegionId);
                    allRegions->addLine2Region(tx, ty, yx, yy, targetRegionId);
                    srcp.x = tx;
                    srcp.y = ty;
                    dstp.x = yx;
                    dstp.y = yy;
                }
                //checkmatch("after DRAW", allRegions, regions);
                if (d < mindist) mindist = d;
                if (d > maxdist) maxdist = d;
                if (xok || yok) {
                    // combine regions source into target
                    printf("Combine %d@(%d,%d)[%ld] -> %d@(%d,%d)[%ld]\n", 
                           rid, srcp.x, srcp.y, allRegions->get(rid)->size(),
                           targetRegionId, dstp.x, dstp.y, allRegions->get(targetRegionId)->size());
                    //printf("Combining %d into %d for %d regions\n", rid, targetRegionId, numreg-1);
                    changes++;
                    connections++;
                    zeroConn = 3;
                    Region* target = allRegions->get(targetRegionId);
                    for (RPIterator it = list->begin(); it != list->end(); ++it) {
                        Point p = *it;
                        //printf("Adding (%d,%d) to %d\n", p.x, p.y, targetRegionId);
                        target->add(p);
                        regions->setPixel(p.x, p.y, targetRegionId);
                    }
                    allRegions->set(rid, NULL);
                    numreg--;
                }
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
        checkmatch("after while loop", allRegions, regions);
        printf("%d: min:%d max:%d connected:%d\n", i, mindist, maxdist, connections);

        if (fill) delete fill;
        fill = recolor(regions, numreg);
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

        if (zeroConn-- <= 0) break;
    }
}

//white= 255;
//black = 0;
int main( int argc, char* argv[] )
{
    ArgDesc::procargs(argc, argv, argdescs);
    printf("%s: reading fontimage %s to produce fontfile: %s\n", ArgDesc::progname, inname, outname);

    //////////////////////////////////////////////////////////////////////////
    // Read gray-value image
    //////////////////////////////////////////////////////////////////////////
    pImage = Image<unsigned char>::readPGM(inname);
    if( ! pImage )  {
        printf( "Reading image failed!\n");
        exit(-1);
    }
    char chars[1024];
    int maxchar = 0;
    char buffer[256];
    sprintf(buffer, "%s.lst", inname);
    FILE* fp = fopen(buffer, "rb");
    if (fp == NULL) exit(-1);
    while (1) {
        if (fgets(buffer, 10, fp) == NULL) break;
        chars[maxchar++] = buffer[0];
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

    RegionList* allRegions = regions->createRegions();
    int numreg = allRegions->numRegions();
    fill = NULL;
    Image<unsigned char>* ni = NULL;
    connregions(allRegions, regions, ni, 20000, 50, 70, 1.0);
    if (allRegions->numRegions() != maxchar) {
        printf("got %d letters, but asked for %d letters\n", allRegions->numRegions(), maxchar);
        regions = recolor(regions, allRegions->numRegions());
        regions->PPMout("fontout.ppm");
        exit(-1);
    }
    int lastreg = 0;
    Font* f = new Font();
    fp = f->saveSetup(outname, allRegions->numRegions(), chars[0]);
    int ci = 0;
    for (int y=0; y<regions->getHeight(); y++) {
        for (int x=0; x<regions->getWidth(); x++) {
            int rid = regions->getPixel(x, y);
            if ((rid == 0) || (lastreg == rid)) continue;
            lastreg = rid;
            for (allRegions->start(); allRegions->hasMore(); allRegions->next()) {
                Region* list = allRegions->current(); // list of points in this region
                if (list == NULL) continue;
                Point p = list->get(0);
                if (regions->getPixel(p.x, p.y) != rid) continue;
                Letter* l = new Letter(list);
                printf("%c -> [%d,%d]\n", chars[ci], l->getWidth(), l->getHeight());
                l->show(0.5);
                f->save(fp, chars[ci], l);
                ci++;
                break;
            }
        }
    }
    fclose(fp);
}

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

