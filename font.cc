#include "font.h"

Font::Font(const char* filename) : base(0), maxchar(0), maxWidth(0), maxHeight(0), chars(NULL)
{
    load(filename);
}


Font::~Font()
{
    delete[] chars;
}

FILE*
Font::saveSetup(const char* filename, int size, char base)
{
    FILE* fp = fopen(filename, "wb");    
    fprintf(fp, "%d %c\n", size, base);
    return fp;
}

void 
Font::save(FILE* fp, char c, Letter* ltr)
{
    fprintf(fp, "%c %d\n", c, ltr->getSize());
    Region* points = ltr->getPoints();
    for (std::vector<Point>::iterator it = points->begin(); it != points->end(); ++it) {
        Point p = *it;
        fprintf(fp, "%d, %d\n", p.x, p.y);
    }
}

void 
Font::load(const char* filename)
{
    
    FILE * fp = fopen(filename, "rb");
    if ( 0 == fp ) {
        fprintf(stderr, "fp == 0\n");
        return;
    }
    int num;
    char c;
    int xx = fscanf(fp, "%d %c\n", (int*)&num, (char*)&c);
    if (xx != 2) fprintf(stderr, "num not found\n");
    base = c;
    maxchar = base+num;
    chars = new Letter[num];
    for (int i=0; i<num; i++) {
        char c;
        int np;
        int xx = fscanf(fp, "%c %d\n", (char*)&c, (int*)&np);
        if (xx != 2) fprintf(stderr, "char not found\n");
        if ((c < base)||(c >= maxchar)) exit(-1);
        chars[c-base].load(fp, np);
        if (chars[c-base].getWidth() > maxWidth) maxWidth = chars[c-base].getWidth();
        if (chars[c-base].getHeight() > maxHeight) maxHeight = chars[c-base].getHeight();
    }
}

int
Font::draw(Image<unsigned char>* target, char c, unsigned char color, int x, int y, double scale)
{
    if ((c < base)||(c >= maxchar)) return 0;
    c = c - base;
    Letter* ltr = &(chars[c]);
    return ltr->draw(target, color, x, y, scale);
}

int
Font::draw(Image<unsigned char>* target, const char* s, unsigned char color, int x, int y, double scale)
{
    int startx = x;
    int lw = maxWidth*scale/2;
    int lh = maxHeight*scale;
    char c;
    while (c = *s++) {
        if (c == ' ') x+=lw;
        else if (c == '\n') {
            x = startx;
            y += lh;
        } else x += (draw(target, c, color, x, y, scale)+4);
    }
}

int
Font::draw(Image<int>* target, char c, int color, int x, int y, double scale)
{
    if ((c < base)||(c >= maxchar)) return 0;
    c = c - base;
    Letter* ltr = &(chars[c]);
    if (0) {
        printf("\nat 1.0\n");
        ltr->show(1.0); printf("\n and at %g\n", scale);
        ltr->show(scale);printf("\n");
    }
    return ltr->draw(target, color, x, y, scale);
}

int
Font::draw(Image<int>* target, const char* s, int color, int x, int y, double scale)
{
    int startx = x;
    int lw = maxWidth*scale/2;
    int lh = maxHeight*scale;
    char c;
    while (c = *s++) {
        if (c == ' ') x+=lw;
        else if (c == '\n') {
            x = startx;
            y += lh;
        } else x += (draw(target, c, color, x, y, scale)+1);
    }
}

int
Letter::draw(Image<unsigned char>* target, unsigned char color, int x, int y, double scale)
{
    for (std::vector<Point>::iterator it = points.begin(); it != points.end(); ++it) {
        Point p = *it;
        target->setPixel(x+p.x, y+p.y, color);
    }
    return width;
}

int
Letter::draw(Image<int>* target, int color, int x, int y, double scale)
{
    Pmap* map = scaleit(scale);
    int w = target->getWidth();
    int h = target->getHeight();
    for (int dy=0; dy<map->height(); dy++) {
        for (int dx=0; dx<map->width(); dx++) {
            int xx = x+dx;
            int yy = y+dy;
            if (yy >= h) continue;
            if (xx >= w) continue;
            if (map->isOn(dx, dy)) target->setPixel(xx, yy, color);
        }
    }
    w = map->width();
    delete map;
    return w;
}

Pmap* Letter::scaleit(double scale)
{
    int h = height*scale;
    int w = width*scale;
    char* rows[height+1];
    for (int i=0; i<=height; i++) {
        rows[i] = new char[width+2];
        for (int j=0; j<=width; j++) rows[i][j] = 0;
        rows[i][width+1] = 0;
    }
    for (std::vector<Point>::iterator it = points.begin(); it != points.end(); ++it) {
        Point p = *it;
        rows[p.y][p.x] = 1;
    }
    Pmap* result = new Pmap(w+2, h+1);
    double jump = 1/scale;
    if (jump < 1) jump = 1;
    for (int y=0; y<height; y++) {
        int i = floor((double)y*jump);
        if (i >= height) break;
        for (int x=0; x<width; x++) {
            int j = floor((double)x*jump);
            if (j >= width) break;
            int sum = 0;
            //printf("Looking at (%d,%d)\n", j, i);
            for (int yi=0; yi<jump; yi++) {
                for (int xi=0; xi<jump; xi++) {
                    int sy = i+yi;
                    int sx = j+xi;
                    if (sy > height) continue;
                    if (sx > width) continue;
                    sum += rows[sy][sx];
                }
            }
            if (sum < jump*jump/2) sum = 0;
            int oy = (scale > 1) ? floor(y*scale) : y;
            int ox = (scale > 1) ? floor(x*scale) : x;
            for (int yi=0; yi<scale; yi++) {
                for (int xi=0; xi<scale; xi++) {
                    int ty = oy+yi;
                    int tx = ox+xi;
                    if (ty > h) continue;
                    if (tx > w) continue;
                    //printf("\tsetting (%d,%d) <- %c\n", tx, ty, sum ? '1' : '0');
                    result->rows[ty][tx] = sum ? 1 : 0;
                }
            }
        }
    }
    
    for (int i=0; i<=height; i++) {
        delete[] rows[i];
    }
    return result;
}

void
Letter::show(double scale)
{
    Pmap* map = scaleit(scale);
    for (int x=0; x<map->width(); x++) { printf("-"); } 
    printf("\n");
    for (int y=0; y<map->height(); y++) {
        for (int x=0; x<map->width(); x++) {
            printf("%c", map->isOn(x, y) ? 'X' : ' ');
        }
        printf("|\n");
    }
    for (int x=0; x<map->width(); x++) { printf("-"); } 
    printf("\n");
    delete map;
}

void 
Letter::load(FILE* fp, int np)
{
    width = height = 0;
    while (np-- > 0) {
        Point p;
        int xx = fscanf(fp, "%d, %d\n", (int*)&p.x, (int*)&p.y);
        if (xx != 2) fprintf(stderr, "x&y not found\n");
        points.add(p);
        if (p.x > width) width = p.x;
        if (p.y > height) height = p.y;
    }
}

Letter::Letter(Region* list)
{
    Point p = list->get(0);
    int minw = p.x;
    int minh = p.y;
    width = 0;
    height = 0;
    for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        if (minw > p.x) minw = p.x;
        if (minh > p.y) minh = p.y;
    }
    for (std::vector<Point>::iterator it = list->begin(); it != list->end(); ++it) {
        Point p = *it;
        p.x -= minw;
        p.y -= minh;
        if (p.x > width) width = p.x;
        if (p.y > height) height = p.y;
        points.add(p);
    }
}

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:


