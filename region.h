#ifndef _REGION_H_
#define _REGION_H_

#include <vector>
#include <assert.h>
#include "hash.h"
#include "set.h"
#include "point.h"
class Font;

typedef std::vector<Point>::iterator RPIterator;
typedef int Nearest;
template <class Pixel> class Image;
class RegionList;

class Distance {
 public:
  Point start;
  Point end;
  double reldist;
 Distance(Point s, Point e, double d=0.0) : start(s), end(e), reldist(d) {}
};

class Region {
 private:
    Point _ulPoint;
    Point _ul;
    Point _lr;
    int _area;
    int valid;
    int lastPerimUpdate;
    double lineHeight;
 public:
    int id;
    std::vector<Point> list;
    Set<Region*> neighbors;
    Nearest closestRegions;
    std::vector<Distance> distances[4];
    int commonSides;
    std::vector<Point> boundary;

 Region(int _id=0): valid(0), id(_id), lastPerimUpdate(0), commonSides(0), lineHeight(0) {
    }
    ~Region() {
      //printf("Deleting %d\n", id);
    }
    RPIterator begin(void)  {
        return list.begin();
    }
    RPIterator end(void) {
        return list.end();
    }
    long int size(void) const {
        return list.size();
    }
    // set lineheight of this region
    void setLineHeight(double lh) {
        lineHeight = lh;
    }
    double getLineHeight(void) const {
        return lineHeight;
    }
    // add a point to this region
    void add(Point p) {
      allValid(false);
      if (0) {
	for (int i=0; i<list.size(); i++) {
	  Point q = list[i];
	  assert (q != p);
	}
      }
      list.push_back(p);
    }
    // add another region to this region
    void add(Region* r);
    // get a particular point
    Point get(int idx=0) {
        return list[idx];
    }
    // find all perimeter points and distances to other regions
    void findPerimeter(Image<int>* img, RegionList* allRegions);
    int countMultipleSides(Image<int>* img, std::vector<int>* tids = NULL);

    int getArea(void) {
      if (!isULvalid()) helpUL();
      return _area;
    }
    Point getUL(void) {
      if (!isULvalid()) helpUL();
      return _ul;
    }
    Point getLR(void) {
      if (!isULvalid()) helpUL();
      return _lr;
    }
    Point getULPoint(void) {
      if (!isULvalid()) helpUL();
      return _ulPoint;
    }
    Set<Region*>* getNeighbors(Image<int>* img, RegionList* allRegions) {
      if (!isNeighborvalid()) findNeighbors(img, allRegions);
      return &neighbors;
    }
    void label(Image<int>* img, Font* f, double scale);
    void addInfo(Image<int>* img, Font* f, const char* str, double scale=1.0);

 private:
    void helpUL(void);
    inline bool isULvalid() { return valid & 1; }
    inline bool isNeighborvalid() { return valid & 2; }
    inline bool isClosestvalid() { return valid & 4; }
    inline void ULValid(bool x) { if (x) valid |= 1; else valid &= ~1; }
    inline void NbrValid(bool x) { if (x) valid |= 2; else valid &= ~2; }
    inline void ClosestValid(bool x) { if (x) valid |= 4; else valid &= ~4; }
    void collect(Hash<int,int>& targets);
    // find all neighbors to a region
    void findNeighbors(Image<int>* img, RegionList* allRegions);
 public:
    inline void allValid(bool x) { if (x) valid = ~0; else valid = 0; }
    inline bool isPerimValid() { return valid & 8; }
    inline void PerimValid(bool x) { if (x) valid |= 8; else valid &= ~8; }
};

inline int hash(const Region* const p) 
{
  unsigned long int x = (unsigned long int)p;
  return x ^ (x>>16);
}

inline int isEqual(const Region* const p, const Region* const q) 
{
  return (p == q);
}


class RegionList {
 public:
    int iter;
    std::vector<Region*> all;
    int maxreg = 0;
    int numreg = 0;

    RegionList() { all.resize(10, NULL); }
    ~RegionList();
    int numRegions(void) {
        return numreg;
    }
    int size(void) {
        return all.size();
    }
    Region* get(int x) {
        return all[x];
    }
    void addPixel2Region(int x, int y, int rid) {
        all[rid]->add(Point(x, y));
    }
    void merge(Region* dest, int src, Image<int>* img) {
      merge(dest, get(src), img);
    }
    void merge(int dest, int src, Image<int>* img) {
      merge(get(dest), get(src), img);
    }
    void merge(int dest, Region* src, Image<int>* img) {
      merge(get(dest), src, img);
    }
    void merge(Region* dest, Region* src, Image<int>* img);

    void set(int x, Region* v) {
        while (x >= all.size()) all.resize(all.size()*2, NULL);
        all[x] = v;
        if (x > maxreg) maxreg = x;
        if (v != NULL) numreg++; else numreg--;
        if (0) {
            if (v != NULL) 
                printf("Adding %ld points to region %d\n", v->size(), x); 
            else 
                printf("got rid of %d\n", x);
        }
    }

    void start(void) {
        iter = -1;
        next();
    }
    bool hasMore(void) {
        return ((iter < all.size()) && (all[iter] != NULL));
    }
    void next(void) {
        iter++;
        while ((iter < all.size()) && (all[iter] == NULL)) iter++;
    }
    Region* current(void) {
        return all[iter];
    }

    void addLine2Region(int tx, int ty, int yx, int yy, int targetRegionId);
};

#endif

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

