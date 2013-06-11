#ifndef _SET_H_
#define _SET_H_

#include "hash.h"

template <class Key>
class Set : public Hash<Key, bool>
{
 public:
  void insert(Key key) { Hash<Key,bool>::insert(key, true); }
};

template <class Key>
class SetIterator : public HashIterator<Key, bool> 
{
 public:
 SetIterator(Set<Key>& table) : HashIterator<Key,bool>(table) {}
};

#define SetTemplate(k) template class Set<k>; template class SetIterator<k>; HashTemplate(k,bool)

#endif

