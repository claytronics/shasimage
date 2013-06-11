#ifndef _HISTOGRAM_H_
#define _HISTOGRAM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class Histogram {
 private:
  int maxbucket;
  int* counters;
  int total;
  
 public:
  Histogram(int buckets=16);
  ~Histogram(void);
  void set(int x, int cnt=1);
  void print(const char* prompt);
  void print(const char* prompt, int numranges);
  int getTotal(void) { return total; }
  int numBuckets(void) { return maxbucket; }
  int get(int x) { return counters[x]; }
  void rescale(int maxlen);
  int remin(int leave);
  int mode();
  double avg();
  double median();
  double stddev();

 private:
  void resize(int x);
};

#endif
