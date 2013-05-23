/* Copyright (C) 2013 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef SOLUTION2_H
#define SOLUTION2_H

#ifndef __cplusplus
#error a C++ compiler is required
#endif

#include <cstring>
#include <sstream>
#include "Solution.h"

using namespace std;

const int INF = 1073741824;
const int MAX_DIM = 1025;

class Solution2 : public Solution {
public:
  Solution2();
  ~Solution2();

  virtual int process(const string& b, const string& a, int qsc, int qec,
                 int mm, int mi, int o, int e, int dir,
                 int *opt, int *te, int *qe, int *n_best);
private:
  int **M;
  int **H;
  int **V;
  int mem;
  void resize(int n);
};

#endif // SOLUTION2_H
