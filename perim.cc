#include "image.h"
#include "arg.h"
#include <unordered_set>
#include <set>
#include "histogram.h"
#include "font.h"

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

// find the perimeter of a region, returning a region with the list of points.

template <class Pixel>
Region*
makePerimeter(Image<Pixel>* src, Region* list)
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
        return NULL;
    }
    // The created region
    Region* perim = new Region();

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
            return perim;
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
                perim->add(Point(special.x, special.y));
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
                perim->add(Point(special.x, special.y));
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
            perim->add(Point(walker.x+1, walker.y));
            walker.y--;
            break;

        case DOWN:
            perim->add(Point(walker.x, walker.y+1));
            walker.y++;
            break;

        case LEFT:
            perim->add(Point(walker.x, walker.y));
            walker.x--;
            break;

        case RIGHT:
            perim->add(Point(walker.x+1, walker.y+1));
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
    return perim;
}    



// find the perimeter of the region, list.

template<class Pixel>
void
oldMakePerimeter(Image<Pixel>* dest, Image<Pixel>* src, Region* list)
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
        Region* perim = makePerimeter(img, list);
        if (perim == NULL) continue;
        for (RPIterator it = perim->begin(); it != perim->end(); ++it) {
            Point p = *it;
            target->setCircle(p.x, p.y, list->id, 4);
        }
        delete perim;
    }
    return target;
}

//template <class Pixel> 
//extern Image<int>* makePerimImage(Image<int>* img, RegionList* allRegions);

void
bogus(Image<int>* x, RegionList* rl)
{
  makePerimImage(x, rl);
}


// place to keep some old stuff in case I decide I want it

#if 0
void
oldfill() {
            if (0) {
                // fill regions in x direction
                for (int y=0; y<regions->getHeight(); y++) {
                    for (int x=0; x<regions->getWidth(); x++) {
                        int c = regions->getPixel(x,y);
                        int lastcx = x;
                        if (c != 0) {
                            for (int i=x+1; i<regions->getWidth(); i++) {
                                int d = regions->getPixel(i,y);
                                if (d == c) {
                                    lastcx = i;
                                    continue;
                                }
                                if (d != 0) break;
                            }
                            int xx;
                            for (xx=x+1; xx<=lastcx; xx++) {
                                if (regions->getPixel(xx, y) != c) {
                                    regions->setPixel(xx, y, c);
                                    //if (c == 2) printf("Adding %d,%d to %d [%ld]\n", xx, y, c, allRegions->get(c)->size());
                                    allRegions->addPixel2Region(xx, y, c);
                                }
                            }
                            x = lastcx;
                        }
                    }
                }
                checkmatch("after X", allRegions, regions);

                // fill regions in y direction
                for (int x=0; x<regions->getWidth(); x++) {
                    for (int y=0; y<regions->getHeight(); y++) {
                        int c = regions->getPixel(x,y);
                        int lastcy = y;
                        if (c != 0) {
                            for (int i=y+1; i<regions->getHeight(); i++) {
                                int d = regions->getPixel(x,i);
                                if (d == c) {
                                    lastcy = i;
                                    continue;
                                }
                                if (d != 0) break;
                            }
                            int yy;
                            for (yy=y+1; yy<=lastcy; yy++) {
                                if (regions->getPixel(x, yy) != c) {
                                    regions->setPixel(x, yy, c);
                                    //if (c == 2) printf("ADDING %d,%d to %d [%ld]\n", x, yy, c, allRegions->get(c)->size());
                                    allRegions->addPixel2Region(x, yy, c);
                                }
                            }
                            y = lastcy;
                        }
                    }
                }
                checkmatch("after Y", allRegions, regions);
            }

            if (0 && (i > 10)) {
                // fill regions in y direction
                for (int x=0; x<regions->getWidth(); x++) {
                    for (int y=0; y<regions->getHeight(); y++) {
                        int c = regions->getPixel(x,y);
                        int lastcy = y;
                        if (c != 0) {
                            for (int i=y+1; i<regions->getHeight(); i++) {
                                int d = regions->getPixel(x,i);
                                if (d == c) {
                                    lastcy = i;
                                    continue;
                                }
                                if (d != 0) break;
                            }
                            int yy;
                            for (yy=y+1; yy<=lastcy; yy++) {
                                if (regions->getPixel(x, yy) != c) {
                                    int minx, maxx;
                                    getHorzBounds(regions, yy, c, minx, maxx);
                                    if ((x >= minx)&&(x <= maxx)) { 
                                        regions->setPixel(x, yy, c);
                                        //if (c == 2) printf("ADDING %d,%d to %d [%ld]\n", x, yy, c, allRegions->get(c)->size());
                                        allRegions->addPixel2Region(x, yy, c);
                                    }
                                }
                            }
                            y = lastcy;
                        }
                    }
                }
                checkmatch("after Y", allRegions, regions);
            }

            //sprintf(filename, "fill-%d.ppm", i);
            //regions->PPMout(filename);
            if (intermediateFiles) {
                char filename[128];
                Image<int>* newregions = recolor(regions, allRegions);

                sprintf(filename, "%s-fregions-%d.ppm", intermediateFiles, i);
                newregions->PPMout(filename);
                delete newregions;
            }


}
#endif
