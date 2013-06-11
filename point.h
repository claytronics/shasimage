#ifndef _POINT_H_
#define _POINT_H_

#include <stdlib.h>

class Point {
 public:
    int x;
    int y;
 Point(int _x, int _y) : x(_x), y(_y) {}
 Point() {}
 Point& operator-=(const Point& rhs)
  {
    x-rhs.x;
    y-rhs.y;
    return *this;
  }
 int distance(const Point& rhs) {
   int dist = (x-rhs.x)+(y-rhs.y);
   if (dist < 0) return -dist;
   return dist;
 }
 double dot(const Point& rhs) const {
   return x*rhs.x+y*rhs.y;
 }
};
inline bool operator==(const Point& lhs, const Point& rhs){ return lhs.x==rhs.x && lhs.y==rhs.y; }
inline bool operator!=(const Point& lhs, const Point& rhs){ return lhs.x!=rhs.x || lhs.y!=rhs.y; }
inline Point operator-(const Point& lhs, const Point& rhs){ return Point(lhs.x-rhs.x, lhs.y-rhs.y); }
inline int hash(const Point* const p) 
{
  return p->x ^ p->y;
}

inline int isEqual(const Point* const p, const Point* const q) 
{
  return (p->x==q->x) && (p->y==q->y);
}

inline int hash(const Point p) 
{
  return p.x ^ p.y;
}

inline int isEqual(const Point p, const Point q) 
{
  return (p.x==q.x) && (p.y==q.y);
}

struct hash_PointP {
    size_t operator()(const Point* const &p ) const
    {
        return p->x ^ p->y;
    }
};

struct eq_PointP {
    bool operator ()(const Point* const &a, const Point* const &b) const 
    {
        return ((a->x == b->x)&&(a->y == b->y));
    }
};

extern Point directions[];

#endif
