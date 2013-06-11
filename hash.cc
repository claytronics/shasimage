#include "hash.h"
#include "assert.h"

template<class Key, class Value>
void Hash<Key, Value>::insert(Key key, Value v)
{
  accCount++;
  int idx = hash(key) & mask;
  HashElem<Key, Value>* p;
  for (p = buckets[idx]; p && !isEqual(p->key, key); p = p->next) accLength++;
  assert(p == NULL);
  p = new HashElem<Key, Value>(key, v);
  p->next = buckets[idx];
  buckets[idx] = p;
  _size++;
  if (_size > 2*capacity) rebalance();
}

template<class Key, class Value>
bool 
Hash<Key, Value>::remove(Key key)
{
  accCount++;
  int idx = hash(key) & mask;
  HashElem<Key, Value>* p;
  for (p = buckets[idx]; p && !isEqual(p->key, key); p = p->next) accLength++;
  if (p == NULL) return false;
  if (p == buckets[idx]) {
    buckets[idx] = p->next;
  } else {
    HashElem<Key, Value>* q;
    for (q = buckets[idx]; q->next != p; q=q->next);
    q->next = p->next;
  }
  delete p;
  _size--;
  return true;
}

int hashUid;

template<class Key, class Value>
Hash<Key, Value>::Hash() : accCount(0), accLength(0), _size(0), capacity(128)
{ 
  //printf("%d hash Created\n", hashUid);
  uid = hashUid++;
  mask = capacity-1;
  buckets = new HashElem<Key, Value>*[capacity]; 
  memset(buckets, 0, sizeof(HashElem<Key, Value>*)*capacity); 
}

template<class Key, class Value>
Hash<Key, Value>::~Hash() {
  //printf("%d hash Deleting\n", uid);
  for (int i=0; i<capacity; i++) {
    HashElem<Key, Value>* pnext;
    for (HashElem<Key, Value>* p = buckets[i]; p; p=pnext) {
      pnext = p->next;
      delete p;
    }
  }
  delete[] buckets;
}

template<class Key, class Value>
void
Hash<Key, Value>::empty(void) {
  for (int i=0; i<capacity; i++) {
    HashElem<Key, Value>* pnext;
    for (HashElem<Key, Value>* p = buckets[i]; p; p=pnext) {
      pnext = p->next;
      delete p;
    }
  }
  memset(buckets, 0, sizeof(HashElem<Key, Value>*)*capacity);
  _size = 0;
}

template<class Key, class Value>
void Hash<Key, Value>::rebalance(void)
{
  int oldcap = capacity;
  HashElem<Key, Value>** oldbuck = buckets;
  capacity *= 2;
  buckets = new HashElem<Key, Value>*[capacity];   
  memset(buckets, 0, sizeof(HashElem<Key, Value>*)*capacity);  
  mask = capacity-1;
  _size = 0;
  //printf("rebalancing %p -> %d\n", this, capacity);

  for (int i=0; i<oldcap; i++) {
    for (HashElem<Key, Value>* p = oldbuck[i]; p; p=p->next)
      insert(p->key, p->val);
  }
  delete[] oldbuck;
}

template<class Key, class Value>
void 
Hash<Key, Value>::showStats(const char* prompt)
{
  int minlen=capacity;
  int maxlen=0;
  for (int i=0; i<capacity; i++) {
    int cnt = 0;
    for (HashElem<Key, Value>* p = buckets[i]; p; p=p->next) cnt++;
    if (cnt > maxlen) maxlen = cnt;
    if (cnt < minlen) minlen = cnt;
  }
  printf("%s: cap:%d siz:%d load:%g min:%d max:%d avg:%g\n",
	 prompt, capacity, _size, (double)capacity/_size, minlen, maxlen, (accCount == 0) ? 0 : (double)accLength/accCount);
}

template<class Key, class Value>
void 
Hash<Key, Value>::addall(Hash<Key, Value>& other)
{
  for (int i=0; i<other.capacity; i++) {
    for (HashElem<Key, Value>* p = other.buckets[i]; p; p=p->next) {
      if (find(p->key)) continue;
      insert(p->key, p->val);
    }
  }
}

template<class Key, class Value>
bool 
HashIterator<Key, Value>::isEnd(void)
{
  return done;
}

template<class Key, class Value>
void 
HashIterator<Key, Value>::next(void)
{
    if (ptr) {
      ptr = ptr->next;
      if (ptr) return;
    }
    while (index < base->capacity) {
      ptr = base->buckets[index++];
      if (ptr) {
	return;
      }
    }
    done=true;
}

template<class Key, class Value>
Key& 
HashIterator<Key, Value>::current(void)
{
  assert(ptr != NULL);
  return ptr->key;
}





