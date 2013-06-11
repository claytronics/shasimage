#ifndef _FONT_H_
#define _FONT_H_

#include "image.h"
#include "stdio.h"

class Pmap {
 public:
    char** rows;
    int maxrow;
    int maxcol;
    Pmap() : maxcol(0), maxrow(0), rows(NULL) {}
    Pmap(int x, int y) : maxcol(x), maxrow(y) { 
        rows = new char*[maxrow]; 
        for (int i=0; i<maxrow; i++) {
            rows[i] = new char[maxcol];
            for (int j=0; j<maxcol; j++) rows[i][j] = 0;
        }
    }
    int height() { return maxrow; }
    int width() { return maxcol; }
    bool isOn(int x, int y) {
        return rows[y][x] != 0;
    }
    ~Pmap() {
        if (rows) {
            for (int i=0; i<maxrow; i++) delete[] rows[i];
            delete[] rows;
        }
    }
};

class Letter {
 private:
  Region points;
  int width;
  int height;

 public:
 Letter():width(0),height(0) {}
  Letter(Region* list);
  void load(FILE*, int np);
  void show(double scale=1.0);
  int draw(Image<unsigned char>* target, unsigned char color, int x, int y, double scale=1.0);
  int draw(Image<int>* target, int color, int x, int y, double scale=1.0);
  int getWidth() { return width; }
  int getHeight() { return height; }
  int getSize() { return points.size(); }
  Region* getPoints() { return &points; }
  Pmap* scaleit(double scale);
};

class Font {
 private:
    Letter* chars;
    int base;
    int maxchar;
    int maxWidth;
    int maxHeight;
 public:
    Font() : base(0), maxchar(0), maxWidth(0), maxHeight(0), chars(NULL) {}
    Font(const char* filename);
    ~Font();
    void load(const char* filename);
    int draw(Image<unsigned char>* target, char c, unsigned char color, int x, int y, double scale=1.0);
    int draw(Image<unsigned char>* target, const char* s, unsigned char color, int x, int y, double scale=1.0);
    int draw(Image<int>* target, char c, int color, int x, int y, double scale=1.0);
    int draw(Image<int>* target, const char* s, int color, int x, int y, double scale=1.0);
    int getWidth(char c) { return chars[c].getWidth(); }
    int getHeight(char c) { return chars[c].getHeight(); }
    int getBaseWidth() { return maxWidth; }
    int getBaseHeight() { return maxHeight; }

    FILE* saveSetup(const char* filename, int size, char base);
    void save(FILE* fp, char c, Letter* ltr);
};

#endif

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

