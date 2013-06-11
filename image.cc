#include "image.h"
#include "stdio.h"
#include <vector>
#include "hash.h"
#include "set.h"
#include <assert.h>

template <class Pixel>
int Image<Pixel>::verbose = 0;

template <class Pixel>
Image<Pixel>* 
Image<Pixel>::readPGM(const char* fileName) 
{
    int sx;
    int sy;
    Pixel* pImage = 0;
  
    fprintf(stderr, "reading %s\n", fileName);
    FILE * fp = fopen(fileName, "rb");
	
    if ( 0 == fp ) {
        fprintf(stderr, "fp == 0\n");
        return NULL;
    }

    int nGrayValues;
	
    char format[16];
    int xx = fscanf(fp, "%s\n", (char*)&format);
    if (xx != 1) fprintf(stderr, "format not found\n");
    fprintf(stderr, "[%s]\n", format);

    // Kommentar-Zeilen überlesen
    char tmpCharBuf[256];
	
    fpos_t position;
    fgetpos (fp, &position);

    while ( true )
        {
            char* p = fgets( tmpCharBuf, 255, fp );
            if (p == NULL) fprintf(stderr, "fgets error\n");
            const int cnt = strncmp( tmpCharBuf, "#", 1 );
            if (0 != cnt)
                {
                    fsetpos(fp, &position);
                    break;
                }
            else
                {
                    fgetpos (fp, &position);
                }
        }

    int nParamRead1 = fscanf( fp, "%d %d\n", &sx, &sy );
    int nParamRead2 = fscanf( fp, "%d\n", &nGrayValues );

    fprintf(stderr, "[%d %d %d] %d %d\n", sx, sy, nGrayValues, nParamRead1, nParamRead2);

    Image<Pixel>* img = NULL;
    if ( (nParamRead1 != 2) || (nParamRead2 != 1) ) {
        fprintf(stderr, "not 2 or not 1\n");
        fclose(fp);
        return NULL;
    } else {
        if ( (0 == strncmp("P2", format, 2)) && (4096 == nGrayValues) )  {
            const double fact = 255./4095.;
            img = new Image(sx, sy);
            int idx = 0;
            for (int y = 0; y < sy; ++y)  {
                for (int x = 0; x < sx; ++x) {
                    int val;
                    int xx = fscanf (fp, "%d", &val);
                    if (xx != 1) fprintf(stderr, "val not read\n");
                    img->setPixel(x, y, (unsigned char)floor( 0.5 + (double)val * fact ));
                }
            }
        } else if ( (0 == strncmp("P5", format, 2)) && ( (255 == nGrayValues) || (256 == nGrayValues) ) ) {
            img = new Image<Pixel>(sx, sy);
            //fseek(fp, -(long)sx*(long)sy*(long)sizeof(Pixel), SEEK_END);
            //printf("at pos: %ld\n", ftell(fp));
            const int readcount = (int)(fread(img->pixels, sx*sizeof(Pixel), sy, fp) );
            if (sy != readcount)
                {
                    fclose(fp);
                    fprintf(stderr, "sys != readcount\n");
                    return NULL;
                }
        } else {
            fprintf(stderr, "P2 & P5\n");
            fclose(fp);
            return NULL;
        }
    }
    return img;
}

template <class Pixel>
int 
Image<Pixel>::getLastBadRegion(unsigned int *data, int start, int end, int incr)
{
  int sofarGood = 0;
  int sofarBad = 0;
  int largestBad = 0;
  int largestGood = 0;
  int badone = 0;
  int len = 0;
  int last = -1;
  int lasti = -1;
  int lastbreak = 0;
  bool neverfound = true;

  printf("---------------------- GLBR: %d -> %d by %d\n", start, end, incr);
  for (int y=start; y != end; y += incr) {
    if (data[y] == last) continue;
    if (lasti != -1) {
      int runlen = incr * (y-lasti);
      if (last < 2) {
	sofarBad += runlen;
	if (runlen > largestBad) largestBad = runlen; 
	if ((runlen > sofarGood)&&(runlen > largestGood)) badone = 1; else badone = 0;
	if (badone) {
            lastbreak = y-incr;
            neverfound = false;
        }
      } else {
	sofarGood += runlen;
	if (runlen > largestGood) largestGood = runlen; 
	badone = 0;
      }
      if (Image<Pixel>::verbose) {
          printf("%3d:%3d %2d  good:%3d %4d  bad:%3d %4d %s\n",
                 lasti, runlen, last, largestGood, sofarGood, largestBad, sofarBad,
                 (badone > 0) ? "Break" : "");
      }
    }
    lasti = y;
    last = data[y];
  }
  if (Image::verbose) printf("lastbreak=%d, neverfound=%s %d\n", lastbreak, (neverfound ? "yes" : "no"), start);
  if (neverfound) return start;
  return lastbreak;
}

template <class Pixel>
Image<Pixel>*
Image<Pixel>::trim(int edge)
{
    unsigned int hist[256];
    memset( hist, 0, 256*sizeof(unsigned int) );
    unsigned int *lines = new unsigned int [height];
    memset( lines, 0, height*sizeof(unsigned int) );

    // determine how many pixels on this line are really black
    for( int y = 0; y < height; y++ ) {
        int black = 0;
        for ( int x = 0; x < width; x++ )  {
            Pixel c = getPixel(x,y);
            if (c < 20) {
                black++;
            }
        }
        lines[y] = black;
    }

    // determine how many pixels in this column are really black
    unsigned int *columns = new unsigned int [width];
    memset( columns, 0, width*sizeof(unsigned int) );
    for ( int x = 0; x < width; x++ )  {
        int black = 0;
        for( int y = 0; y < height; y++ ) {
            Pixel c = getPixel(x,y);
            if (c < 20) {
                black++;
            }
        }
        columns[x] = black;
    }

    // get a scaling factor
    double maxval = 0;
    for ( int x = 0; x < width; x++ )  {
        for( int y = 0; y < height; y++ ) {
            if (lines[y]*columns[x] > maxval) {
                maxval = lines[y]*columns[x];
            }
        }
    }
    maxval = maxval/256;

    // mask will get a value of 255 if we think that pixel is a good one, 0 if we think it is useless
    Image* mask = new Image(width, height);
    for ( int x = 0; x < width; x++ )  {
        for( int y = 0; y < height; y++ ) {
            double f = lines[y];
            f = f * (double)columns[x];
            f = f/(double)maxval;
            f = f * (256-(double)getPixel(x,y)/256.0);
            if (f< 0) f = 0;
            if (f>255) f = 255;
            unsigned int i = f;
            Pixel c = i;
            mask->setPixel(x,y,c);
        }
    }
    
    if (1) {
        mask->PGMout("../xx.pgm");
    }

    // now divide into vertical regions of goodness
    for( int y = 0; y < height; y++ ) {
        int good = 0;
        lines[y] = 0;
        for ( int x = 0; x < width; x++ )  {
            Pixel c = mask->getPixel(x, y);
            if (c > 192) {
                good++;
            }
        }
        double f = floor(10*((double)good/(double)width));
        lines[y] = (int)f;
    }
      
    // now we find last bad region going down, last good region going up
    int starty = getLastBadRegion(lines, 0, height, 1);
    int endy = getLastBadRegion(lines, height, 0, -1);

    // now divide across x
    for( int x = 0; x < width; x++ ) {
        int good = 0;
        columns[x] = 0;
        for ( int y = 0; y < height; y++ )  {
            Pixel c = mask->getPixel(x, y);
            if (c > 192) {
                good++;
            }
        }
        double f = floor(10*((double)good/(double)height));
        columns[x] = (int)f;
    }
      
    // now we find last bad region going down, last good region going up
    int startx = getLastBadRegion(columns, 0, width, 1);
    int endx = getLastBadRegion(columns, width, 0, -1);
    startx -= edge;
    endx += edge;
    starty -= edge;
    endy += edge;
    if (startx < 0) startx = 0;
    if (starty < 0) starty = 0;
    if (endx > width) endx = width;
    if (endy > height) endy = height;

    printf("vertically %d:%d  horizontaly %d:%d\n", starty, endy, startx, endx);
    Image*cropImage = crop(startx, endx, starty, endy);
    return cropImage;
}

int max4(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    int max = a;
    if (max < b) max = b;
    if (max < c) max = c;
    if (max < d) max = d;
    return max;
}

int min4(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    int min = a;
    if (min > b) min = b;
    if (min > c) min = c;
    if (min > d) min = d;
    return min;
}

// &1 -> do lines
// &2 -> do cols
// &3 -> do both
template <class Pixel>
Image<Pixel>*
Image<Pixel>::block(int directions)
{
    Image* out = new Image<Pixel>(width, height);
    out->fill(white());

    // longest white line
    if (directions & 1) {
        for( int y = 0; y < height; y++ ) {
            int startx = 0;
            int longrun = 0;
            int prev = black();           // black
            int run = 0;
            for ( int x = 0; x < width; x++ )  {
                Pixel c = getPixel(x,y);
                //if (c != 255) printf("%d %s\n", c, isNearWhite(c) ? "white" : "black");
                if (isNearWhite(c)) {
                    if (isNearWhite(prev)) run++;
                    else {
                        prev = c;
                        run = 1;
                    }
                } else {
                    if (run > longrun) {
                        startx = x-run;
                        longrun = run;
                    }
                    prev = black();
                }
            }
            if (isNearWhite(prev) && (run > longrun)) {
                longrun = run;
                startx = width-run;
            }
            //printf("%4d: %4d @ %4d\n", y, longrun, startx);
            for (int x = startx; x<startx+longrun; x++) {
                out->setPixel(x, y, black());
            }
        }
        out->PGMout("zz.pgm");
    }

    if (directions & 2) {
        for( int x = 0; x < width; x++ ) {
            int starty = 0;
            int longrun = 0;
            int prev = black();
            int run = 0;
            for ( int y = 0; y < height; y++ )  {
                Pixel c = getPixel(x,y);
                if (isNearWhite(c)) {
                    if (isNearWhite(prev)) run++;
                    else {
                        prev = c;
                        run = 1;
                    }
                } else {
                    if (run > longrun) {
                        starty = y-run;
                        longrun = run;
                    }
                    prev = black();
                }
            }
            if (isNearWhite(prev) && (run > longrun)) {
                longrun = run;
                starty = height-run;
            }
            //printf("%4d: %4d @ %4d\n", x, longrun, starty);
            for (int y = starty; y<starty+longrun; y++) {
                out->setPixel(x, y, black());
            }
        }
    }
    return out;
}

template <class Pixel>
int 
Image<Pixel>::fillIn(int radius)
{
    int maxrad = radius*2+1;
    int changed = 0;
    int show = 0;
    for( int y = radius; y < height-radius; y++ ) {
        for ( int x = radius; x < width-radius; x++ )  {
            Pixel c = getPixel(x, y);
            if (c == 0) continue;
            double d = 0;
            int count = 0;
            // get values of four corners
            int corners;
            corners = round((getPixel(x-radius, y-radius)+getPixel(x-radius, y+radius)+
                             getPixel(x+radius, y-radius)+getPixel(x+radius, y+radius))/4.0);
            if (0) {
                corners = min4(getPixel(x-radius, y-radius),getPixel(x-radius, y+radius),
                               getPixel(x+radius, y-radius),getPixel(x+radius, y+radius));
            }
            show = 0;
            for (int i=-radius; i<=radius; i++) {
                for (int j=-radius; j<=radius; j++) {
                    Pixel pix = getPixel(x+j, y+i);
                    d += pix;
                    count++;
                }
            }
            d = round((d/(double)count));
                if (show) {                
                    printf("(%d,%d) corner:%d newval:%g oldval:%d  %s\n", x, y, corners, d, c, ((d < corners)||((d <= corners)&&(d<c))) ? "yes" : "");
                for (int i=-radius; i<=radius; i++) {
                    for (int j=-radius; j<=radius; j++) {
                        Pixel pix = getPixel(x+j, y+i);
                        printf("%3d\t", pix);
                    }
                    printf("\n");
                }
                printf("\n");
                }

                if ((d >= corners)&&(d < c)) {
                    changed++;
                    setPixel(x, y, (Pixel)d);
                }
        }
    }
    return changed;
}

template <class Pixel>
void 
Image<Pixel>::copyRegion(int sx, int sy, int sw, int sh, Image* dest, int dx, int dy)
{
    for (int i=0; i<sh; i++) {
        for (int j=0; j<sw; j++) {
            Pixel c = getPixel(sx+j, sy+i);
            dest->setPixel(dx+j, dy+i, c);
        }
    }
}

template <class Pixel>
int
Image<Pixel>::countAndZapBlack(int startx, int starty) 
{
    int cnt = 0;
    setPixel(startx, starty, 255);
    for (int i=-1; i<=1; i++) {
        for (int j=-1; j<=1; j++) {
            if ((startx+i < 0)||(starty+j < 0)) continue;
            if ((startx+i < width)||(starty+j > height)) continue;
            if (getPixel(startx+i, starty+j) < 30) {
                cnt++;
                setPixel(startx+i, starty+j, 255);
                cnt += countAndZapBlack(startx+i, starty+j);
            }
        }
    }
    return cnt;
}

template <class Pixel>
void
Image<Pixel>::blobHist(void)
{
    Image* blob = new Image(this);
    int hist[256];
    for (int i=0; i<256; i++) hist[i] = 0;

    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            Pixel c = blob->getPixel(x, y);
            int cnt = 0;
            if (c < 30) {
                cnt = blob->countAndZapBlack(x, y);
                if (cnt > 256) cnt = 255;
                //printf("%4d: (%d,%d)\n", cnt, x, y);
                hist[cnt]++;
            }
        }
    }

    for (int i=0; i<256; i++) {
        if (hist[i] > 0) printf("%3d: %6d\n", i, hist[i]);
    }
    blob->PGMout("blob.pgm");
    delete blob;
}

template <class Pixel>
int
Image<Pixel>::copyBlackAndZap(int startx, int starty, int num, int size, Point* points)
{
    //printf("Z:%02d,%02d ", startx, starty);
    setPixel(startx, starty, 255);
    if (num < size) {
        points[num].x = startx;
        points[num].y = starty;
    }
    num++;
    for (int i=-1; i<=1; i++) {
        for (int j=-1; j<=1; j++) {
            if (((startx+i)<0)||((startx+i)> width)||
                ((starty+j)<0)||((starty+j)> height)) continue;
            if (getPixel(startx+i, starty+j) < 30) {
                int cnt = copyBlackAndZap(startx+i, starty+j, num, size, points);
                if (cnt == 0) cnt = size+1; 
                num = cnt;
            }
        }
    }
    if (num <= size) return num;
    return 0;
}

template <class Pixel>
Image<Pixel>* 
Image<Pixel>::findBlobs(int size, int border)
{
    Image* blob = new Image(this);
    Image* out = new Image(this);
    out->fill(255);
    Point points[size];

    printf("Finding Blobs of size: %d\n", size);
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            Pixel c = blob->getPixel(x, y);
            int cnt = 0;
            if (c < 30) {
                //printf("black:(%d,%d)\n", x, y);
                int cnt = blob->copyBlackAndZap(x, y, 0, size, points);
                //printf("cnt = %d for size:%d @ (%d,%x\n", cnt, size, x, y);
                if (cnt == size) {
                    Point ul = {x, y};
                    Point lr = {x, y};
                    printf("blob: ");
                    for (int i=0; i<cnt; i++) {
                        Point* p = points+i;
                        printf("(%4d,%4d) ", p->x, p->y);
                        if (p->x < ul.x) ul.x = p->x;
                        if (p->y < ul.y) ul.y = p->y;
                        if (p->x > lr.x) lr.x = p->x;
                        if (p->y > lr.y) lr.y = p->y;
                        out->setPixel(p->x, p->y, 0);
                    }
                    printf("\n");
                    for (int i=ul.x-border; i<= lr.x+border; i++) {
                        for (int j=ul.y-border; j<=lr.y+border; j++) {
                            if ((i < 0)||(i>width)||(j<0)||(j>height)) continue;
                            if (out->getPixel(i, j) == 255) out->setPixel(i, j, 128);
                        }
                    }
                }
            }
        }
    }
    delete blob;
    return out;
}

template <class Pixel>
bool 
Image<Pixel>::PPMout(const char * fileName)
{
    unsigned char * pTmpBuffer = new unsigned char[width*height*3];
    Pixel* p = pixels;
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            RGB val(*p++);
            pTmpBuffer[y*width*3+x*3] = val.m_r;
            pTmpBuffer[y*width*3+x*3+1] = val.m_g;
            pTmpBuffer[y*width*3+x*3+2] = val.m_b;
        }
    }

    FILE* fp = fopen(fileName, "wb");
    if( !fp ) return false;

    fprintf(fp,"P6\n%d %d\n255\n", width, height);
    if( 1 != fwrite(pTmpBuffer, width*height*3*sizeof(unsigned char), 1, fp) ) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    delete[] pTmpBuffer;
    return true;
}


template <class Pixel>
bool 
Image<Pixel>::PGMout(const char * fileName)
{
    FILE* fp = fopen(fileName, "wb");
    if( !fp )
        return false;

    if (sizeof(Pixel) != 1) {
        fprintf(stderr, "illegal size for PGM out\n");
        return false;
    }
    fprintf(fp,"P5\n%d %d\n%d\n", width, height, 255);
    bool ret = true;
    if( 1 != fwrite(pixels, width*height, 1, fp) ) {
        ret = false;
    }
    fclose(fp);
    return ret;
};

static Point around[] = {{ -1, -1 }, {0, -1}, {1, -1}, 
                          { -1, 0 },           {1, 0 }, 
                          { -1, 1 }, {0,   1}, {1, 1}};

template <class Pixel>
bool
Image<Pixel>::getNeighbor(int x, int y, int n, int& xx, int& yy) {
    assert ((n >= 0)||(n<=7));
    xx = x + around[n].x;
    yy = y + around[n].y;
    if ((xx < 0)||(xx >= width)) return false;
    if ((yy < 0)||(yy >= height)) return false;
    return true;
}

template <class Pixel>
void 
Image<Pixel>::setCircle(const int x, const int y, const Pixel val, int radius) 
{

    setPixel(x, y, val);
    for (int i=0; i<=radius; i++) {
        for (int j=0; j<=radius; j++) {
            if (sqrt((double)(i*i+j*j)) < radius) {
                if ((x+i) < getWidth()) {
                    if ((y+j) < getHeight()) setPixel(x+i, y+j, val);
                    if ((y-j) > 0) setPixel(x+i, y-j, val);
                }
                if ((x-i) > 0) {
                    if ((y+j) < getHeight()) setPixel(x-i, y+j, val);
                    if ((y-j) > 0) setPixel(x-i, y-j, val);
                }
            }
        }
    }
}


template <class Pixel>
RegionList*
Image<Pixel>::createRegions(void)
{
    RegionList* allRegions = new RegionList;
    int regionid = 2;
    int numreg = 0;
    for (int y=0; y<getHeight(); y++) {
        for (int x=0; x<getWidth(); x++) {
            if (getPixel(x, y) == 1) {
                // find and label this region
                allRegions->set(regionid, labelAndReturnRegion(x, y, 1, regionid));
                regionid++;
                numreg++;
            }
        }
    }
    //printf("returning %d regions, #2 has %ld\n", allRegions->size(), allRegions->get(2)->size());
    return allRegions;
}

template <class Pixel>
Region*
Image<Pixel>::labelAndReturnRegion(int x, int y, Pixel srcid, Pixel destid)
{
    std::vector<Point> todo;
    Region* final = new Region(destid);
    Point p(x,y);
    todo.push_back(p);
    setPixel(x, y, destid);
    while (todo.size() > 0) {
        Point p = todo.back();
        todo.pop_back();
        final->add(p);     // add to final list
        int x, y;
        for (int i=0; i<8; i++) {
            if (getNeighbor(p.x, p.y, i, x, y)) {
                // valid neighbor
                int r = getPixel(x, y);
                if (r == srcid) {
                    setPixel(x, y, destid);
                    Point q(x,y);
                    todo.push_back(q);
                }
            }
        }
    }
    return final;
 }

template <class Pixel>
void 
Image<Pixel>::labelRegion(int x, int y, Pixel srcid, Pixel destid)
{
    //printf("Labeling %d,%d from %d->%d\n", x, y, srcid, destid);
    std::vector<Point*> todo;
    Point* p = new Point(x, y);
    todo.push_back(p);
    setPixel(x, y, destid);
    while (todo.size() > 0) {
        Point* p = todo.back();
        todo.pop_back();
        int x, y;
        for (int i=0; i<8; i++) {
            if (getNeighbor(p->x, p->y, i, x, y)) {
                // valid neighbor
                int r = getPixel(x, y);
                if (r == srcid) {
                    setPixel(x, y, destid);
                    Point* q = new Point(x, y);
                    todo.push_back(q);
                }
            }
        }
        delete p;
    }
    //printf("Labeled (%d,%d) as %d (%08x) with %d\n", x, y, destid, destid, count);
}

template <class Pixel>
bool
Image<Pixel>::findNearestRegion(int x, int y, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist)
{
    if (maxdist == 0) maxdist = (getWidth() > getHeight()) ? getWidth() : getHeight();
    int regionid = getPixel(x, y);
    Set<Point*> list;
    std::vector<Point*> todo;
    
    //printf("(%d,%d) %d\n", x, y, regionid);
    Point* p = new Point(x, y);
    todo.push_back(p);
    while (todo.size() > 0) {
        Point* p = todo.back();
        todo.pop_back();
        if (list.exists(p)) {
            delete p;
            continue;
        }
        list.insert(p);
        for (int i=0; i<8; i++) {
            if (getNeighbor(p->x, p->y, i, x, y)) {
                // valid neighbor
                int r = getPixel(x, y);
                if (r == regionid) {
                    Point* p = new Point(x, y);
                    if (list.exists(p)) {
                        delete p;
                        continue;
                    }
                    todo.push_back(p);
                }
            }
        }
    }
    if (1) {
        for (SetIterator<Point*> it(list); !it.isEnd(); it.next()) {
            Point*p = it.current();
            printf("(%d,%d) ", p->x, p->y);
        }
        printf("\n");
    }

    // iterate through the points in the region
    int deltax = direc ? 1 : 0;
    int deltay = direc ? 0 : 1;
    int maxX = getWidth();
    int maxY = getHeight();
    int mindist = getWidth() * deltax + getHeight()*deltay;
    if (mindist > maxdist) mindist = maxdist;
    Point* minpoint = NULL;
    int minDirec = -1;
    for (SetIterator<Point*> it(list); !it.isEnd(); it.next()) {
        Point* p = it.current();
        for (int minus = 1; minus >= -1; minus-=2) {
            //printf("Search\t[%d,%d] %d>", p->x, p->y, minus);
            int distance = 1;
            Pixel c;
            bool falloff = false;
            for (; distance < mindist; distance++) {
                x = p->x + minus*distance*deltax;
                y = p->y + minus*distance*deltay;
                if ((x < 0)||(y < 0)) { falloff=true; break; }
                if ((x >= maxX)||(y >= maxY)) {falloff=true; break; }
                c = getPixel(x, y);
                //printf(" (%d,%d:%d)", x, y, c);
                if (c == 0) continue;
                if (c == regionid) break;
                break;
            }
            if (!falloff && (c != regionid) && (distance < mindist)) {
                mindist = distance;
                minpoint = p;
                minDirec = minus;
            }
            //printf(" min:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
        }
    }
    //printf("MIN:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
    if (minpoint != NULL) {
        fromx = minpoint->x;
        fromy = minpoint->y;
        tox = minpoint->x + minDirec * deltax * mindist;
        toy = minpoint->y + minDirec * deltay * mindist;
        dist = mindist;
    }

    // clean up
    for (SetIterator<Point*> it(list); !it.isEnd(); it.next()) {
        Point* p = it.current();
        delete p;
    }
    return minpoint != NULL;
 }

template <class Pixel>
int
Image<Pixel>::count8Neighbors(int x, int y, Pixel rid)
{
    int count = 0;
    for (int i=0; i<8; i++) {
        int xx;
        int yy;
        if (getNeighbor(x, y, i, xx, yy)) {
            if (getPixel(xx, yy) == rid) count++;
        }
    }
    return count;
}


template <class Pixel>
bool
Image<Pixel>::findNearestRegion(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<int>* blocker)
{
    if (maxdist == 0) maxdist = (getWidth() > getHeight()) ? getWidth() : getHeight();
    Point start = list->get(0);
    int regionid = getPixel(start.x, start.y);

    // iterate through the points in the region
    int deltax = direc ? 1 : 0;
    int deltay = direc ? 0 : 1;
    int maxX = getWidth();
    int maxY = getHeight();
    int mindist = getWidth() * deltax + getHeight()*deltay;
    if (mindist > maxdist) mindist = maxdist;
    Point minpoint(-1,-1);
    int minDirec = -1;
    for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        for (int minus = 1; minus >= -1; minus-=2) {
            //printf("Search\t[%d,%d] %d>", p->x, p->y, minus);
            int distance = 1;
            Pixel c;
            bool falloff = false;
            bool blocked = false;
            for (; distance < mindist; distance++) {
                int x = p.x + minus*distance*deltax;
                int y = p.y + minus*distance*deltay;
                if ((x < 0)||(y < 0)) { falloff=true; break; }
                if ((x >= maxX)||(y >= maxY)) {falloff=true; break; }
                c = getPixel(x, y);
                //printf(" (%d,%d:%d)", x, y, c);
                if (c == 0) {
                    if (blocker && (blocker->getPixel(x, y) == 0x00ffffff)) {
                        blocked = true;
                        break;
                    }
                    continue;
                }
                if (c == regionid) break;
                break;
            }
            if (!falloff && (c != regionid) && (distance < mindist) && !blocked) {
                mindist = distance;
                minpoint = p;
                minDirec = minus;
            }
            //printf(" min:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
        }
    }
    //printf("MIN:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
    if (minpoint.x != -1) {
        fromx = minpoint.x;
        fromy = minpoint.y;
        tox = minpoint.x + minDirec * deltax * mindist;
        toy = minpoint.y + minDirec * deltay * mindist;
        dist = mindist;
    }

    return (minpoint.x != -1);
 }

int showalmost = 0;

template <class Pixel>
int
Image<Pixel>::findNearestRegions(Region* list, NearInfo<Pixel>* result, int maxdist, int maxsize, Image<unsigned char>* blocker, double bfactor, RegionList* rl)
{
    // setup basic things for every direction
    if (bfactor > 0) bfactor = 256.0/bfactor; else bfactor=10000.0;
    if (maxdist == 0) maxdist = (getWidth() > getHeight()) ? getWidth() : getHeight();
    Point start = list->get(0);
    int regionid = getPixel(start.x, start.y);
    int debug = 0;
    int maxX = getWidth();
    int maxY = getHeight();
    int count = 0;
    // now iterate through result searching in each direction
    for (int direction=0; direction<4; direction++) {
        NearInfo<Pixel>* rp = result+direction;
        int deltax = rp->deltax;
        int deltay = rp->deltay;
        rp->valid = false;
        rp->id = 0;             // set for debugging, see below
        int mindist = maxX * deltax + maxY*deltay;
        if (mindist < 0) mindist = -mindist;
        if (mindist > maxdist) mindist = maxdist;
        double reldist = mindist;
        Point minpoint(-1,-1);
        if (debug) printf("FNR:%d, dir:%d, md:%d, bf:%g, ms:%d\n", regionid, direction, maxdist, bfactor, maxsize);
        int minRID = 0;
        int minSize = -1;
        for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
            Point p = *it;
            int distance = 1;
            Pixel c = 0;
            bool falloff = false;
            double extraDist = 0;
            double thismaxdist = 3*maxdist;
            // search for nearest region
            int x;
            int y;
            for (; (distance+extraDist) < thismaxdist; distance++) {
                x = p.x + distance*deltax;
                y = p.y + distance*deltay;
                if ((x < 0)||(y < 0)) { falloff=true; break; }
                if ((x >= maxX)||(y >= maxY)) {falloff=true; break; }
                c = getPixel(x, y);
                if (c != 0) break;
                if (blocker != NULL) extraDist += ((double)blocker->getPixel(x, y))/bfactor;
            }        
            if (falloff || (c == regionid) || (c == 0)) continue;
            // we have found another region
            int tsize;
            if (rl != NULL) {
                tsize = rl->get(c)->size();
                if ((maxsize > 0)&&(tsize > maxsize)) {
                    if (debug) printf(" %d exceeds size with %d\n", c, tsize);
                    continue;
                }
            } else
                tsize = minSize;
            // we have found another reason within size constraints
            double sratio = (double)tsize/(double)minSize;
            double dratio = (distance+extraDist)/reldist;
            if (sratio*dratio < 1) {
                if (debug && (tsize > minSize))
                    printf(" was:%d+%g->%d[%d]  now:%d+%g->%d[%d]\n", mindist, reldist-mindist, minRID, minSize,
                           distance, extraDist, c, tsize);
                if (distance+extraDist > maxdist) {
                    // this is for debugging only
                    rp->reldist = distance+extraDist;
                    rp->distance = distance;
                    rp->id = c;
                    rp->size = tsize;
                    continue;
                }
                if (!rp->valid) count++;
                rp->valid = true;
                rp->from = p;
                rp->to = Point(x, y);
                mindist = rp->distance = distance;
                reldist = rp->reldist = distance+extraDist;
                if (reldist*1.2 < thismaxdist) thismaxdist = reldist*1.2;
                minRID = rp->id = c;
                minSize = rp->size = tsize;
            } else {
                if (debug) printf("fail: %d+%g %d of %d\n", distance, extraDist, c, tsize);
            }
        }
        // for debugging
        if (showalmost) {
            if ((rp->valid == false)&&(rp->id != 0)) {
                printf("almost: %d->%d of %d+%g of %d (max:%d %g)\n", regionid, rp->id, rp->distance, rp->reldist-rp->distance, rp->size, maxdist, (10000.0 == bfactor) ? 0 : 256.0/bfactor);
            }
        }
    }
    return count;
}

template <class Pixel>
void
Image<Pixel>::findConnectors(Region* list, int targetId, std::vector<Distance>& connectors)
{
    assert(list->isPerimValid());
    for (int direction=0; direction<4; direction++) {
        for (int di=0; di<list->distances[direction].size(); di++) {
            Distance dist = list->distances[direction][di];
            Point p = dist.start;
            Point ep = dist.end;
            if ((p.x == ep.x)&&(p.y == ep.y)) continue;
            Pixel tid = getPixel(ep.x, ep.y);
            if (tid == targetId) {
                connectors.push_back(dist);
            }
        }
    }
}

template <class Pixel>
int
Image<Pixel>::findNearestRegionsByPerim(Region* list, NearInfo<Pixel>* result, int maxdist, int maxsize, Image<unsigned char>* blocker, double bfactor, RegionList* rl)
{
    assert(list->isPerimValid());
    // setup basic things for every direction
    if (bfactor > 0) bfactor = 256.0/bfactor; else bfactor=10000.0;
    if (maxdist == 0) maxdist = (getWidth() > getHeight()) ? getWidth() : getHeight();
    int regionid = list->id;
    int debug = 0;
    int maxX = getWidth();
    int maxY = getHeight();
    int count = 0;
    // now iterate through result searching in each direction
    for (int direction=0; direction<4; direction++) {
        NearInfo<Pixel>* rp = result+direction;
        int deltax = rp->deltax;
        int deltay = rp->deltay;
        rp->valid = false;
        rp->id = 0;             // set for debugging, see below
        int mindist = maxX * deltax + maxY*deltay;
        if (mindist < 0) mindist = -mindist;
        if (mindist > maxdist) mindist = maxdist;
        double reldist = mindist;
        Point minpoint(-1,-1);
        if (debug) printf("FNR:%d, dir:%d, md:%d, bf:%g, ms:%d\n", regionid, direction, maxdist, bfactor, maxsize);
        int minRID = 0;
        int minSize = -1;
        for (int di=0; di<list->distances[direction].size(); di++) {
            Distance dist = list->distances[direction][di];
            Point p = dist.start;
            Point ep = dist.end;
            if ((p.x == ep.x)&&(p.y == ep.y)) continue;
            int distance = 1;
            Pixel c = 0;
            double extraDist = 0;
            double thismaxdist = 4*maxdist;
            // search for nearest region
            int x;
            int y;
            for (; (distance+extraDist) < thismaxdist; distance++) {
                x = p.x + distance*deltax;
                y = p.y + distance*deltay;
                assert ((x >= 0)&&(y >= 0));
                assert ((x < maxX)&&(y < maxY));
                c = getPixel(x, y);
                if (c != 0) break;
                if (blocker != NULL) extraDist += ((double)blocker->getPixel(x, y))/bfactor;
            }        
            assert(c != regionid);
            list->distances[direction][di] = Distance(p, ep, reldist);
            if (c == 0) continue;
            // we have found another region
            int tsize;
            if (rl != NULL) {
                tsize = rl->get(c)->size();
                if ((maxsize > 0)&&(tsize > maxsize)) {
                    if (debug) printf(" %d exceeds size with %d\n", c, tsize);
                    continue;
                }
            } else
                tsize = minSize;
            // we have found another reason within size constraints
            double sratio = (double)tsize/(double)minSize;
            double dratio = (distance+extraDist)/reldist;
            if (sratio*dratio < 1) {
                if (debug && (tsize > minSize))
                    printf(" was:%d+%g->%d[%d]  now:%d+%g->%d[%d]\n", mindist, reldist-mindist, minRID, minSize,
                           distance, extraDist, c, tsize);
                if (distance+extraDist > maxdist) {
                    // this is for debugging only
                    rp->reldist = distance+extraDist;
                    rp->distance = distance;
                    rp->id = c;
                    rp->size = tsize;
                    continue;
                }
                if (!rp->valid) count++;
                rp->valid = true;
                rp->from = p;
                rp->to = Point(x, y);
                mindist = rp->distance = distance;
                reldist = rp->reldist = distance+extraDist;
                if (reldist*1.2 < thismaxdist) thismaxdist = reldist*1.2;
                minRID = rp->id = c;
                minSize = rp->size = tsize;
            } else {
                if (debug) printf("fail: %d+%g %d of %d\n", distance, extraDist, c, tsize);
            }
        }
        // for debugging
        if (showalmost) {
            if ((rp->valid == false)&&(rp->id != 0)) {
                printf("almost: %d->%d of %d+%g of %d (max:%d %g)\n", regionid, rp->id, rp->distance, rp->reldist-rp->distance, rp->size, maxdist, (10000.0 == bfactor) ? 0 : 256.0/bfactor);
            }
        }
    }
    return count;
}


template <class Pixel>
bool
Image<Pixel>::findNearestRegion(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<unsigned char>* blocker, double bfactor, RegionList* rl, int maxsize)
{
    if (bfactor > 0) bfactor = 256.0/bfactor; else bfactor=10000;
    if (maxdist == 0) maxdist = (getWidth() > getHeight()) ? getWidth() : getHeight();
    Point start = list->get(0);
    int regionid = getPixel(start.x, start.y);
    int debug = 0;
    if ((regionid == 761)||(regionid == 730)) debug = 1;

    // iterate through the points in the region
    int deltax = direc ? 1 : 0;
    int deltay = direc ? 0 : 1;
    int maxX = getWidth();
    int maxY = getHeight();
    int mindist = getWidth() * deltax + getHeight()*deltay;
    if (mindist > maxdist) mindist = maxdist;
    double reldist = mindist;
    Point minpoint(-1,-1);
    int minDirec = -1;
    if (debug) printf("FNR:%d, dir:%d, md:%d, bf:%g, ms:%d\n", regionid, direc, maxdist, bfactor, maxsize);
    int minRID = 0;
    int minSize = -1;
    for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        for (int minus = 1; minus >= -1; minus-=2) {
            //if (debug) printf("Search\t[%d,%d] %d", p.x, p.y, minus);
            int distance = 1;
            Pixel c = 0;
            bool falloff = false;
            bool blocked = false;
            double extraDist = 0;
            for (; (distance+extraDist) < reldist; distance++) {
                int x = p.x + minus*distance*deltax;
                int y = p.y + minus*distance*deltay;
                if ((x < 0)||(y < 0)) { falloff=true; break; }
                if ((x >= maxX)||(y >= maxY)) {falloff=true; break; }
                c = getPixel(x, y);
                //printf(" (%d,%d:%d)", x, y, c);
                if (c == 0) {
                    if (blocker != NULL) extraDist += ((double)blocker->getPixel(x, y))/bfactor;
                    continue;
                }
                if (c == regionid) break;
                break;
            }
            int tsize;
            if ((c != 0)&&(rl != NULL)&&(!falloff)&&(c!=regionid)) {
                tsize = rl->get(c)->size();
                if ((maxsize > 0)&&(tsize > maxsize)) {
                    if (debug) printf(" %d exceeds size with %d\n", c, tsize);
                    continue;
                }
            }
            if (!falloff && (c != 0) && (c != regionid) && !blocked) {
                double sratio = (double)tsize/(double)minSize;
                double dratio = (distance+extraDist)/reldist;
                if ((minSize==-1)||(sratio*dratio < 1)) {
                    if ((minSize>0)&&(tsize > minSize)) 
                        printf(" was:%d+%g->%d[%d]  now:%d+%g->%d[%d]\n", mindist, reldist-mindist, minRID, minSize,
                               distance, extraDist, c, tsize);
                    reldist = distance+extraDist;
                    mindist = distance;
                    minpoint = p;
                    minDirec = minus;
                    minRID = c;
                    minSize = tsize;
                } else {
                    if (debug) printf("fail: %d+%g %d of %d\n", distance, extraDist, c, tsize);
                }
            }
            if (debug && (minpoint.x != -1)) printf(" min:%d from:(%d,%d) in:%d to:%d of %d\n", mindist, minpoint.x, minpoint.y, minDirec, minRID, minSize);
        }
    }
    if (debug) printf("MIN:%d from:(%d,%d) in:%d to:%d of %d\n", mindist, minpoint.x, minpoint.y, minDirec, minRID, minSize);
    if (minpoint.x != -1) {
        if (debug) printf("Nearest in %s: %d+%g\n", deltax ? "Y" : "X", mindist, reldist-mindist);
        fromx = minpoint.x;
        fromy = minpoint.y;
        tox = minpoint.x + minDirec * deltax * mindist;
        toy = minpoint.y + minDirec * deltay * mindist;
        dist = mindist;
    }

    return (minpoint.x != -1);
 }

typedef struct {
    double rdist;
    Point start;
    int mdist;
} NRInfo;

static int
compare_doubles(const void *a, const void *b)
{
    const double *da = (const double *) a;
    const double *db = (const double *) b;
    
    return (*da > *db) - (*da < *db);
}

static int
compare_nrinfo(const void *a, const void *b)
{
    const NRInfo *da = (const NRInfo *) a;
    const NRInfo *db = (const NRInfo *) b;
    
    return (da->rdist > db->rdist) - (da->rdist < db->rdist);
}

// look for nearest region up or down
template <class Pixel>
bool
Image<Pixel>::findNearestRegionX(Region* list, int direc, int& fromx, int& fromy, int& tox, int& toy, int&dist, int maxdist, Image<unsigned char>* blocker)
{
    assert(direc == 0);
    if (maxdist == 0) maxdist = getHeight();
    Point start = list->get(0);
    int regionid = getPixel(start.x, start.y);
    printf("fnrX (%d,%d) %d upto %d\n", start.x, start.y, regionid, maxdist);
    NRInfo upleft[getWidth()];
    NRInfo downright[getWidth()];
    for (int i=0; i<getWidth(); i++) {
        upleft[i].rdist = downright[i].rdist = 0.0;
    }
    // iterate through the points in the region
    int deltax = 0;
    int deltay = 1;
    int maxX = getWidth();
    int maxY = getHeight();
    int foundCount = 0;
    for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        for (int minus = 1; minus >= -1; minus-=2) {
            //printf("Search\t[%d,%d] %d>", p.x, p.y, minus);
            NRInfo* store = (minus > 0) ? downright : upleft;
            int distance = 1;
            Pixel c;
            bool falloff = false;
            double extraDist = 0;
            for (; (distance+extraDist) < maxdist; distance++) {
                int x = p.x;
                int y = p.y + minus*distance;
                if (y < 0) { falloff=true; break; }
                if (y >= maxY) {falloff=true; break; }
                c = getPixel(x, y);
                //printf(" (%d,%d:%d)", x, y, c);
                if (c == 0) {
                    if (blocker != NULL) extraDist += ((double)blocker->getPixel(x, y))/128.0;
                    continue;
                }
                break;
            }
            if (distance < maxdist) printf("%d+%g\n", distance, extraDist);
            if (!falloff && (c != regionid) && ((distance+extraDist)<maxdist)) {
                printf("storing (%d,%d) <- %7.2g\n", p.x, p.y, distance+extraDist);
                store[p.x].rdist = distance+extraDist;
                store[p.x].mdist = distance;
                store[p.x].start = p;
                foundCount++;
            }
            //printf(" min:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
        }
    }
    //printf("MIN:%d from:(%d,%d) in:%d\n", mindist, minpoint ? minpoint->x : -1, minpoint ? minpoint->y : -1, minDirec);
    if (foundCount == 0) return false;
    double upvals[foundCount];
    double downvals[foundCount];
    int upi=0;
    int downi=0;
    for (int i=0; i<getWidth(); i++) {
        if (upleft[i].rdist != 0) upvals[upi++] = upleft[i].rdist;
        if (downright[i].rdist != 0) downvals[downi++] = downright[i].rdist;
    }
    qsort (upvals, upi, sizeof (double), compare_doubles);
    qsort (downvals, downi, sizeof (double), compare_doubles);
    printf("%6d: up min:%7.2g,%7.2g,%7.2g cnt:%3d\n", regionid, (upi > 0) ? upvals[0] : 9999, (upi > 0) ? upvals[upi/4] : 9999, (upi > 0) ? upvals[upi/2] : 9999, upi);
    printf("      : dn min:%7.2g,%7.2g,%7.2g cnt:%3d\n", (downi > 0) ? downvals[0] : 9999, (downi > 0) ? downvals[downi/4] : 9999, (downi > 0) ? downvals[downi/2] : 9999, downi);
    NRInfo* ret;
    int minus;
    if ((downi == 0)||((upi!=0)&&((upvals[upi/4] < downvals[downi/4])||(upvals[0] < downvals[0])))) {
        minus = -1;
        for (int i=0; i<getWidth(); i++) 
            if (upleft[i].rdist == upvals[0]) {
                ret = upleft+i;
                break;
            }
    } else {
        assert(downi > 0);
        minus = 1;
        for (int i=0; i<getWidth(); i++) 
            if (downright[i].rdist == downvals[0]) {
                ret = downright+i;
                break;
            }
    }
    // now return point of minimum distance
    dist = ret->mdist;
    fromx = ret->start.x;
    fromy = ret->start.y;
    tox = fromx;
    toy = fromy + minus*dist;
    return true;
 }

template <class Pixel>
void 
Image<Pixel>::getDistHelper(int& fromx, int& fromy, int dx, int dy, Pixel rid, int&tox, int&toy, int&dist)
{
    //printf("Search\t[%d,%d] %d>", fromx, fromy, minus);
    int distance = 1;
    Pixel c;
    bool falloff = false;
    int x = fromx;
    int y = fromy;
    while (1) {
        int x = fromx + dx*distance;
        int y = fromy + dy*distance;
        assert((x>=0)&&(y>=0));
        assert((x<getWidth())&&(y<getHeight()));
        c = getPixel(x, y);
        if (c == 0) {
            distance++;
            continue;
        }
        assert (c != rid);
        break;
    }
    tox = x;
    toy = y;
    dist = distance;
}

template <class Pixel>
void 
Image<Pixel>::drawBox(Point ul, Point lr, Pixel color, int lw)
{
    for (int x=ul.x; x<=lr.x; x++) {
        setPixel(x, ul.y, color);
        setPixel(x, ul.y+1, color);

        setPixel(x, lr.y, color);
        setPixel(x, lr.y-1, color);
    }

    for (int y=ul.y; y<=lr.y; y++) {
        setPixel(ul.x, y, color);
        setPixel(ul.x+1, y, color);

        setPixel(lr.x, y, color);
        setPixel(lr.x-1, y, color);
    }
}


// draw a line BETWEEN (sx,sy) and (dx,dy)

template <class Pixel>
void 
Image<Pixel>::drawLine(int sx, int sy, int dx, int dy, Pixel color)
{
    //printf("drawline from (%d,%d) -> (%d,%d)\n", sx, sy, dx, dy);
    if (dy < sy) {
        int t = dy;
        dy=sy;
        sy=t;
    }
    if (dx < sx) {
        int t = dx;
        dx=sx;
        sx=t;
    }
    if (dx == sx) {
        sy++;
        while (sy < dy) setPixel(sx, sy++, color);
    } else {
        sx++;
        while (sx < dx) setPixel(sx++, sy, color);
    }
}

template <class Pixel>
void 
Image<Pixel>::drawLine(int sx, int sy, int dx, int dy, Pixel color, int linew)
{
    //printf("drawline from (%d,%d) -> (%d,%d)\n", sx, sy, dx, dy);
    if (linew == 1) return drawLine(sx, sy, dx, dy, color);
    int startw = linew/2;
    if (dy < sy) {
        int t = dy;
        dy=sy;
        sy=t;
    }
    if (dx < sx) {
        int t = dx;
        dx=sx;
        sx=t;
    }
    if (dx == sx) {
        sy++;
        sx -= startw;
        if (sx < 0) sx = 0;
        if (sx+linew >= getWidth()) sx = getWidth()-(linew+1);
        while (sy < dy) {
            for (int i = 0; i<linew; i++) setPixel(sx+i, sy, color);
            sy++;
        }
    } else {
        sx++;
        sy -= startw;
        if (sy < 0) sy = 0;
        if (sy+linew >= getHeight()) sy = getHeight()-(linew+1);
        while (sx < dx) {
            for (int i = 0; i<linew; i++) setPixel(sx, sy+i, color);
            sx++;
        }
    }
}

template <class Pixel>
RegionSet* 
Image<Pixel>::getPointsOfRegion(int x, int y)
{
    int regionid = getPixel(x, y);
    RegionSet* list = new RegionSet;
    std::vector<Point*> todo;
    
    //printf("(%d,%d) %d\n", x, y, regionid);
    Point* p = new Point(x, y);
    todo.push_back(p);
    while (todo.size() > 0) {
        Point* p = todo.back();
        todo.pop_back();
        if (list->count(p) > 0) continue;
        list->insert(p);
        for (int i=0; i<8; i++) {
            if (getNeighbor(p->x, p->y, i, x, y)) {
                // valid neighbor
                int r = getPixel(x, y);
                if (r == regionid) {
                    Point* p = new Point(x, y);
                    if (list->count(p) > 0) continue;
                    todo.push_back(p);
                }
            }
        }
    }
    if (0) {
        for (RegionSet::iterator it = list->begin() ; it != list->end(); ++it) {
            Point*p = *it;
            printf("(%d,%d) ", p->x, p->y);
        }
        printf("\n");
    }
    return list;
}


template <class Pixel>
void 
Image<Pixel>::fillInscribe(int x, int y)
{
    RegionSet* list = getPointsOfRegion(x, y);
    Point ul(getWidth(),getHeight());
    Point lr(0,0);
    for (std::unordered_set<Point*>::iterator it = list->begin() ; it != list->end(); ++it) {
        Point*p = *it;
        if (p->x < ul.x) ul.x = p->x; 
        if (p->x > lr.x) lr.x = p->x; 
        if (p->y < ul.y) ul.y = p->y; 
        if (p->y > lr.y) lr.y = p->y; 
    }
    printf("(%d,%d) -> (%d,%d)\n", ul.x, ul.y, lr.x, lr.y);
    bool done = false;
    Point* p = new Point(0,0);
    while (!done) {
        // check lowest y along x axis
        p->y=ul.y;
        bool allok = true;
        for (int j=ul.x; j<=lr.x; j++) {
            p->x = j;
            if (list->count(p) > 0) continue;
            if (getPixel(p->x, p->y) == 0) continue;
            ul.y++;
            allok = false;
            break;
        }
        // check highest y along x axis
        p->y=lr.y;
        for (int j=ul.x; j<=lr.x; j++) {
            p->x = j;
            if (getPixel(p->x, p->y) == 0) continue;
            if (list->count(p) > 0) continue;
            lr.y--;
            allok = false;
            break;
        }
        if (ul.y >= lr.y) break;
        // check highest x along y axis
        p->x=lr.x;
        for (int j=ul.y; j<=lr.y; j++) {
            p->y = j;
            if (getPixel(p->x, p->y) == 0) continue;
            if (list->count(p) > 0) continue;
            lr.x--;
            allok = false;
            break;
        }
        // check lowest x along y axis
        p->x=ul.x;
        for (int j=ul.y; j<=lr.y; j++) {
            p->y = j;
            if (getPixel(p->x, p->y) == 0) continue;
            if (list->count(p) > 0) continue;
            ul.x++;
            allok = false;
            break;
        }
        if (ul.x >= lr.x) break;
        done = allok;
    }
    printf("Interior bounding rect is (%d,%d) -> (%d,%d) %s\n", ul.x, ul.y, lr.x, lr.y, done?"":"FAILED");
    if (done) {
        // add interior points
        int regionid = getPixel(x,y);
        for (y=ul.y; y<=lr.y; y++) {
            for (x=ul.x; x<=lr.x; x++) {
                setPixel(x, y, regionid);
            }
        }
    }
}

template <class Pixel>
void 
Image<Pixel>::fillInscribe(Region* list)
{
    Point ul(getWidth(),getHeight());
    Point lr(0,0);
    for (RPIterator it = list->begin() ; it != list->end(); ++it) {
        Point p = *it;
        if (p.x < ul.x) ul.x = p.x; 
        if (p.x > lr.x) lr.x = p.x; 
        if (p.y < ul.y) ul.y = p.y; 
        if (p.y > lr.y) lr.y = p.y; 
    }
    printf("(%d,%d) -> (%d,%d)\n", ul.x, ul.y, lr.x, lr.y);
    Point p = list->get(0);
    int rid = getPixel(p.x,p.y);
    bool done = false;
    p.y = p.x = 0;
    while (!done) {
        // check lowest y along x axis
        p.y=ul.y;
        bool allok = true;
        for (int j=ul.x; j<=lr.x; j++) {
            p.x = j;
            if (getPixel(p.x,p.y) == rid) continue;
            if (getPixel(p.x, p.y) == 0) continue;
            ul.y++;
            allok = false;
            break;
        }
        // check highest y along x axis
        p.y=lr.y;
        for (int j=ul.x; j<=lr.x; j++) {
            p.x = j;
            if (getPixel(p.x,p.y) == rid) continue;
            if (getPixel(p.x, p.y) == 0) continue;
            lr.y--;
            allok = false;
            break;
        }
        if (ul.y >= lr.y) break;
        // check highest x along y axis
        p.x=lr.x;
        for (int j=ul.y; j<=lr.y; j++) {
            p.y = j;
            if (getPixel(p.x,p.y) == rid) continue;
            if (getPixel(p.x, p.y) == 0) continue;
            lr.x--;
            allok = false;
            break;
        }
        // check lowest x along y axis
        p.x=ul.x;
        for (int j=ul.y; j<=lr.y; j++) {
            p.y = j;
            if (getPixel(p.x,p.y) == rid) continue;
            if (getPixel(p.x, p.y) == 0) continue;
            ul.x++;
            allok = false;
            break;
        }
        if (ul.x >= lr.x) break;
        done = allok;
    }
    printf("Interior bounding rect is (%d,%d) -> (%d,%d) %s\n", ul.x, ul.y, lr.x, lr.y, done?"":"FAILED");
    if (done) {
        // add interior points
        for (int y=ul.y; y<=lr.y; y++) {
            for (int x=ul.x; x<=lr.x; x++) {
                setPixel(x, y, rid);
            }
        }
    }
}

template <class Pixel>
Pixel 
Image<Pixel>::firstDiffColor(int x, int y, Pixel c, int dx, int dy)
{
    while ((x >= 0)&&(x < getWidth())&&(y >= 0)&&(y<getHeight())&&(c==getPixel(x,y))) {
        x += dx;
        y += dy;
    }
    if ((x >= 0)&&(x < getWidth())&&(y >= 0)&&(y<getHeight()))
        return getPixel(x,y);
    return c;
}

// collect histogram of color pixel along direc axis, of runlen units
// 0 -> count of pixels for each column
// 1 -> count of pixels for each row
template <class Pixel>
Histogram* 
Image<Pixel>::collectEdgeHistogram(int direc, Pixel color, int runlen)
{
    int b;
    int outLimit;
    int innerLimit;
    int first;
    int second;
    if (direc == 0) {
        outLimit = getWidth(); 
        innerLimit = getHeight();
        first = 1;
        second = 0;
    } else {
        outLimit = getHeight();
        innerLimit = getWidth();
        second = 1;
        first = 0;
    }
    Histogram* hist = new Histogram(outLimit);
    for (int i=0; i<outLimit; i++) {
        int cnt = 0;
        Pixel last = color+1;
        int run = 0;
        for (int j=0; j<innerLimit; j++) {
            Pixel c = getPixel(i*first+j*second, i*second+j*first);
            if (c == color) {
                run++;
            } else {
                if ((last == color)&&(run >= runlen)) cnt+=run;
                run=0;
            }
            last = c;
        }
        if ((last == color)&&(run >= runlen)) cnt+=run;
        hist->set(i, cnt);
    }
    return hist;
}

// transfer all points on line BETWEEN (sx,sy) and (dx,dy) to targetRegionId (id of rl)
template <class Pixel>
void 
Image<Pixel>::addLine2Region(int sx, int sy, int dx, int dy, int targetRegionId, RegionList* rl,int srcId)
{
    //printf("al2r %d %d,%d->%d,%d\n", targetRegionId, sx, sy, dx, dy);
    if (dy < sy) {
        int t = dy;
        dy=sy;
        sy=t;
    }
    if (dx < sx) {
        int t = dx;
        dx=sx;
        sx=t;
    }
    Pixel c;
    if (dx == sx) {
        sy++;
        while (sy < dy) {
            c = getPixel(sx, sy);
            if (c == targetRegionId) {
                sy++;
                continue;
            }
            if (c != srcId) {
                if (c != 0) {
                    printf("@(%d,%d)=%d adding %d->%d in x\n", sx, sy, c, srcId, targetRegionId);
                    assert(0);
                }
                rl->addPixel2Region(sx, sy++, targetRegionId);
            } else sy++;
        }
    } else {
        sx++;
        while (sx < dx) {
            c = getPixel(sx, sy);
            if (c == targetRegionId) {
                sx++;
                continue;
            }
            if (c != srcId) {
                if (c != 0) {
                    printf("@(%d,%d)=%d adding %d->%d in y\n", sx, sy, c, srcId, targetRegionId);
                    assert(0);
                }
                rl->addPixel2Region(sx++, sy, targetRegionId);
            } else sx++;
        }
    }
}

RegionList::~RegionList() {
    for (start(); hasMore(); next()) {
        Region* list = current(); // list of points in this region
        if (list == NULL) continue;
        delete list;
    }
}

// transfer all points on line BETWEEN (sx,sy) and (dx,dy)
void 
RegionList::addLine2Region(int sx, int sy, int dx, int dy, int targetRegionId)
{
    //printf("nochk %d %d,%d->%d,%d\n", targetRegionId, sx, sy, dx, dy);
    if (dy < sy) {
        int t = dy;
        dy=sy;
        sy=t;
    }
    if (dx < sx) {
        int t = dx;
        dx=sx;
        sx=t;
    }
    if (dx == sx) {
        sy++;
        while (sy < dy) {
            addPixel2Region(sx, sy++, targetRegionId);
        }
    } else {
        sx++;
        while (sx < dx) {
            addPixel2Region(sx++, sy, targetRegionId);
        }
    }
}


// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

