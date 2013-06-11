#include "histogram.h"
#include "math.h"
#include <assert.h>

Histogram::Histogram(int buckets) : maxbucket(buckets), total(0) {
  counters = new int[buckets];
  //printf("max=%d counters = %p\n", maxbucket, counters);
  memset(counters, 0, sizeof(int)*buckets);
}

Histogram::~Histogram() {
  //printf("COUNTERS = %p\n", counters);
  delete[] counters;
}

void
Histogram::resize(int x) {
  int oldmax = maxbucket;
  while (maxbucket <= x) maxbucket *= 2;
  int* newcounters = new int[maxbucket];
  for (int i=0; i<oldmax; i++) {
    newcounters[i] = counters[i];
  }
  delete[] counters;
  memset(newcounters+oldmax, 0, sizeof(int)*(maxbucket-oldmax));
  counters = newcounters;
}

void 
Histogram::set(int x, int cnt) {
  if (x >= maxbucket) resize(x);
  assert(x>=0);
  counters[x] += cnt;
  assert (counters[x] >= 0);
  total += cnt;
}

void
Histogram::rescale(int maxlen)
{
  int maxv = 0;
  for (int i=0; i<maxbucket; i++)
    if (counters[i] > maxv) maxv = counters[i];
  double scale = (double)maxv/(double)maxlen;
  for (int i=0; i<maxbucket; i++) 
    counters[i] = floor((double)counters[i]/scale);
}

// chop off the (min count - leave)
int 
Histogram::remin(int leave)
{
  int minv = counters[0];
  for (int i=0; i<maxbucket; i++)
    if (counters[i] < minv) minv = counters[i];
  minv -= leave;
  if (minv <= 0) return 0;
  for (int i=0; i<maxbucket; i++) 
    counters[i] -= minv;
  return minv;
}

void 
Histogram::print(const char* prompt) {
  printf("%s:\n", prompt);
  for (int i=0; i<maxbucket; i++) {
    if (counters[i] > 0) printf("\t%3d:\t%5d\n", i, counters[i]);
  }
  printf("\n");
}

void 
Histogram::print(const char* prompt, int numranges) {
  printf("%s:\n", prompt);
  double perrange = (double)total/(double)numranges;
  if (perrange < 1) perrange = 1;
  perrange = floor(perrange);
  int start = 0;
  int thisrange = 0;
  for (int i=0; i<maxbucket; i++) {
    if (counters[i] == 0) continue;
    thisrange += counters[i];
    if (thisrange >= perrange) {
      printf("\t%3d-%3d:\t%5d\n", start, i, thisrange);
      thisrange = 0;
      start = i+1;
    }
  }
  if (thisrange > 0) {
      printf("\t%3d-%3d:\t%5d\n", start, maxbucket, thisrange);
  }
  printf("\n");
}

int 
Histogram::mode()
{
  int modei = -1;
  int mode = 0;
  for (int i=0; i<maxbucket; i++) {
    if (counters[i] > mode) {
      mode = counters[i];
      modei = i;
    }
  }
  return modei;
}

double 
Histogram::avg()
{
  double sum = 0;
  for (int i=0; i<maxbucket; i++) {
    sum += (i*counters[i]);
  }
  return sum/total;
}

double 
Histogram::median()
{
  int sum = 0;
  int stop = total/2;
  if (total & 1) {
    // odd number
    for (int i=0; i<maxbucket; i++) {
      sum += counters[i];
      if (sum > stop) {
	return i;
      }
    }
  } else {
    // even number
    int i=0;
    for (i; i<maxbucket; i++) {
      sum += counters[i];
      if (sum == stop) break;
      if (sum > stop) {
	return i;
      }
    }
    double result = i;
    for (i++; i<maxbucket; i++) {
      if (counters[i]) break;
    }
    assert(i<maxbucket);
    return (result+i)/2.0;
  }
  assert(0);
}

double 
Histogram::stddev()
{
  double mean = avg();
  double sum = 0;
  for (int i=0; i<maxbucket; i++) {
    assert(counters[i] >= 0);
    sum += counters[i]*((i-mean)*(i-mean));
  }
  sum = sum/total;
  double result = sqrt(sum);
  assert(!isnan(result));
  return result;
}

