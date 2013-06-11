#ifndef _HASH_H_
#define _HASH_H_

#include <stdio.h>
#include <string.h>

inline int hash(const int p) { return p; }
inline int isEqual(const int x, const int y) { return x == y; }

template <class Key, class Value> class Hash;
template <class Key, class Value> class HashIterator;

template <class Key, class Value>
class HashElem {
 private:
  Key key;
  Value val;
  HashElem<Key, Value>* next;
  HashElem<Key, Value>(Key _key, Value _val) : key(_key), val(_val) {}
  friend Hash<Key,Value>;
  friend HashIterator<Key,Value>;
};

template <class Key, class Value> 
class Hash {
 private:
  int _size;
  int mask;
  int accCount;
  int accLength;
  int uid;

 public:
  HashElem<Key, Value>** buckets;
  int capacity;

 public:
  Hash();
  ~Hash();
  int size(void) { return _size; }
 bool exists(Key key) {
   HashElem<Key, Value>* p = find(key);
   if (p) return true;
   return false;
 }
 Value* get(Key key) {
   HashElem<Key, Value>* p = find(key);
   if (p) return &(p->val);
   return NULL;
 }
 void empty(void);
 bool isEmpty(void) { return (_size == 0); }
 void insert(Key key, Value v);
 void addall(Hash<Key, Value>& other);
 bool remove(Key key);		/* true if removed, false if not there in the first place */
 void showStats(const char* prompt);

 private:
 HashElem<Key, Value>* find(Key key) {
   accCount++;
   int idx = hash(key) & mask;
   HashElem<Key, Value>* p;
   for (p = buckets[idx]; p && !isEqual(p->key, key); p = p->next) accLength++;
   return p;
 }
  void rebalance(void);
};

template <class Key, class Value> 
class HashIterator {
  Hash<Key, Value>* base;
  int index;
  HashElem<Key, Value>* ptr;
  bool done;

 public:
 HashIterator(Hash<Key, Value>& table) : base(&table), index(0), ptr(NULL), done(false) { next(); }
  bool isEnd(void);
  void next(void);
  Key& current(void);
};

#define HashTemplate(k,v) template class Hash<k,v>; template class HashIterator<k,v>

#endif
