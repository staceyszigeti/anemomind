/*
 * ReduceTree.h
 *
 *  Created on: 18 Aug 2016
 *      Author: jonas
 */

#ifndef SERVER_COMMON_REDUCETREE_H_
#define SERVER_COMMON_REDUCETREE_H_

#include <server/common/Array.h>

namespace sail {


// TODO: Implement ProportionateIndexer using this
template <typename T>
class ReduceTree {
public:
  static int left(int i) {return 2*i + 1;}
  static int right(int i) {return 2*i + 2;}
  static int parent(int i) {return (i - 1)/2;}

  ReduceTree(std::function<T(T, T)> reducer,
      Array<T> initialData) :
        _reducer(reducer) {
    int n = initialData.size();
    int l = 0;
    int r = 0;
    while (r - l + 1 < n) {
      l = left(l);
      r = right(r);
    }
    int totalSize = r + 1;
    _allData = Array<T>(totalSize);
    _leafOffset = l;
    _leaves = _allData.sliceFrom(_leafOffset);
    assert(_leaves.size() == n);
    initialData.copyToSafe(_leaves);
    initializeTree(0);
  }

  const T &top() const {
    return _allData[0];
  }

  Array<T> allData() const {
    return _allData;
  }

  void setLeaf(int index0, const T &value) {
    assert(0 <= index0);
    assert(index0 < _leaves.size());
    int index = _leafOffset + index0;
    _allData[index] = value;
    while (index != 0) {
      index = parent(index);
      auto result = _allData[left(index)];
      int r = right(index);
      if (r < _allData.size()) {
        result = _reducer(result, _allData[r]);
      }
      _allData[index] = result;
    }
  }
private:
  std::function<T(T, T)> _reducer;
  Array<T> _allData;
  int _leafOffset;
  Array<T> _leaves;

  T initializeTree(int index) {
    if (index < _leafOffset) {
      auto result = initializeTree(left(index));
      int r = right(index);
      if (r < _allData.size()) {
        result = _reducer(result, initializeTree(r));
      }
      _allData[index] = result;
      return result;
    } else {
      return _allData[index];
    }
  }

};

}



#endif /* SERVER_COMMON_REDUCETREE_H_ */
