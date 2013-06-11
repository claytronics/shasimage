#include "region.h"
#include "image.h"
#include "font.h"

void
Region::helpUL(void) {
    _ul = list[0];
    _lr = list[0];
    _ulPoint = list[0];
    for (RPIterator it = list.begin(); it != list.end(); ++it) {
        Point p = *it;
        if (p.x < _ulPoint.x) {_ulPoint.x = p.x; _ulPoint.y = p.y; }
        else if ((p.x == _ulPoint.x)&&(p.y < _ulPoint.y)) { _ulPoint.y = p.y; }
        if (p.x < _ul.x) _ul.x = p.x; 
        if (p.x > _lr.x) _lr.x = p.x; 
        if (p.y < _ul.y) _ul.y = p.y; 
        if (p.y > _lr.y) _lr.y = p.y; 
    }
    _area = (_lr.y-_ul.y)*(_lr.x-_ul.x);
    ULValid(true);
}

void
Region::add(Region* r)
{
  ULValid(false);
  ClosestValid(false);
  neighbors.addall(r->neighbors);
  neighbors.remove(this);
  neighbors.remove(r);
}

Point directions[] = {
  {-1, 0},
  {1, 0},
  {0, -1},
  {0, 1}
};

void
Region::findNeighbors(Image<int>* img, RegionList* allRegions)
{
  if (isNeighborvalid()) return;
  //printf("FN: %d <- ", id);
  neighbors.empty();
  for (RPIterator it = begin(); it != end(); ++it) {
    Point p = *it;
    //printf("\t(%d,%d)\n", p.x, p.y);
    for (int d=0; d<4; d++) {
      Point delta = directions[d];
      //printf("\t\t(%d,%d)\n", delta.x, delta.y);
      int y = p.y + delta.y;
      int x = p.x + delta.x;
      if (delta.y) {
	// looking up and down
	while ((y>=0)&&(y<img->getHeight())) {
	  int c = img->getPixel(x,y);
	  //printf("\t\t\t%d, %d, %d\n", x, y, c);
	  if (c != 0) {
	    if (c != id) {
	      Region* r = allRegions->get(c);
	      assert(r != NULL);
	      if (neighbors.exists(r)) break;
	      // not in yet, so add
	      //printf(" %d", r->id);
	      neighbors.insert(r);
	    }
	    break;
	  }
	  y += delta.y;
	} 
      } else {
	// looking left and right
	while ((x>=0)&&(x<img->getWidth())) {
	  int c = img->getPixel(x,y);
	  //printf("\t\t\t%d, %d, %d\n", x, y, c);
	  if (c != 0) {
	    if (c != id) {
	      Region* r = allRegions->get(c);
	      assert(r != NULL);
	      if (neighbors.exists(r)) break;
	      // not in yet, so add
	      //printf(" %d", r->id);
	      neighbors.insert(r);
	    }
	    break;
	  }
	  x += delta.x;
	} 
      }
    }
  }
  //printf("\n");
  //neighbors.showStats("fn");
  NbrValid(true);
}

void
Region::findPerimeter(Image<int>* img, RegionList* allRegions)
{
  if (isPerimValid()) return;
  //lastPerimUpdate = 0;
  // go over old ones first
  for (int d=0; d<4; d++) {
    Point delta = directions[d];
    //distances[d].clear();	// for now
    for (int i=0; i<distances[d].size(); i++) {
      Point p = distances[d][i].start;
      int y = p.y + delta.y;
      int x = p.x + delta.x;
      bool keepGoing = true;
      while (keepGoing && (y!=-1)&&(y!=img->getHeight())) {
	while (keepGoing && (x!=-1)&&(x!=img->getWidth())) {
	  int c = img->getPixel(x,y);
	  if (c != 0) {
	    keepGoing = false;
	    if (c != id) {
	      // get distance from this point
	      distances[d][i] = Distance(p, Point(x,y));
	    } else {
	      distances[d][i] = Distance(p, p);
	    }
	    break;
	  } else if (delta.x == 0) break;
	  x += delta.x;
	}
	if (delta.y == 0) break;
	y += delta.y;
      }
    }
  }
  // add any new points since we last did this
  for (int i=lastPerimUpdate; i<list.size(); i++) {
    Point p = list[i];
    for (int d=0; d<4; d++) {
      Point delta = directions[d];
      int y = p.y + delta.y;
      int x = p.x + delta.x;
      bool keepGoing = true;
      while (keepGoing && (y!=-1)&&(y!=img->getHeight())) {
	int c;
	while (keepGoing && (x!=-1)&&(x!=img->getWidth())) {
	  c = img->getPixel(x,y);
	  if (c != 0) {
	    keepGoing = false;
	    if (c != id) {
	      // get distance from this point
	      distances[d].push_back(Distance(p, Point(x,y)));
	    }
	    break;
	  } else if (delta.x == 0) break;
	  x += delta.x;
	}
	if (delta.y == 0) break;
	y += delta.y;
      }
    }
  }
  lastPerimUpdate = list.size();
  PerimValid(true);
}


int
Region::countMultipleSides(Image<int>* img, std::vector<int>* tids)
{
  assert(isPerimValid());
  Hash<int,int> targets;
  for (int d=0; d<4; d++) {
    for (int i=0; i<distances[d].size(); i++) {
      Distance* dist = &(distances[d][i]);
      if (dist->start == dist->end) continue; // merged point, ignore
      int tid = img->getPixel(dist->end.x, dist->end.y);
      if (!targets.exists(tid)) {
	targets.insert(tid, 0);
      } 
      int* x = targets.get(tid); // track how many sides this region touches target
      *x = *x | (1 << d);
    }
  }
  int sides[5] = { 0,0,0,0,0};
  for (HashIterator<int,int> it(targets); !it.isEnd(); it.next()) {
    int key = it.current();
    int val = *(targets.get(key));
    assert((1<=val)&&(val<=15));
    int count = 0;
    while (val) {
      if (val & 1) count++;
      val = val >> 1;
    }
    assert(count <= 4);
    sides[count]++;
    if ((count > 1)&&(tids != NULL)) tids->push_back(key);
  }
  int count = 0;
  int ns = 0;
  for (int j=2; j<=4; j++) {
    if (sides[j]>0) { count++; ns=j; }
  }
  if (count == 0) commonSides=1;
  else {
    if (count == 1) commonSides=ns;
    else commonSides=5;
  }
  return commonSides;
}

extern int ldebug;

void 
RegionList::merge(Region* dest, Region* src, Image<int>* img)
{
  assert(!((dest==NULL)||(src==NULL)));
  //if (ldebug || (numRegions() < 300)) printf("Merge: %d <- %d\n", dest->id, src->id);
  int targetRegionId = dest->id;
  for (RPIterator it = src->begin(); it != src->end(); ++it) {
    Point p = *it;
    //printf("Adding (%d,%d) to %d\n", p.x, p.y, targetRegionId);
    dest->add(p);
    img->setPixel(p.x, p.y, targetRegionId);
  }
  int sid = src->id;
  if (src->getLineHeight() > 0.0) {
      // we had line info in src, so transfer to dest
      if (dest->getLineHeight() > 0.0) {
          double newlh = (dest->getLineHeight()+src->getLineHeight())/2;
          if (abs(newlh-dest->getLineHeight()) > 3) {
              printf("Merging %d:%g into %d:%g\n", src->id, src->getLineHeight(), dest->id, dest->getLineHeight());
              dest->setLineHeight(newlh);
          }
      } else {
          dest->setLineHeight(src->getLineHeight());
      }
  }
  delete src;
  set(sid, NULL);
  dest->allValid(false);
}

void 
Region::addInfo(Image<int>* img, Font* f, const char* str, double scale)
{
  Point p;
    p = getULPoint();
    p.y += 10;

  f->draw(img, str, 0x0ffffff, p.x, p.y, scale);
}

void 
Region::label(Image<int>* img, Font* f, double scale)
{
  char buffer[128];
  sprintf(buffer, "%d", id);
  addInfo(img, f, buffer, scale);
}

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:


