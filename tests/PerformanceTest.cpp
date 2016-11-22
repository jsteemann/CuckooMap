#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include <cuckoomap/CuckooHelpers.h>
#include <cuckoomap/CuckooMap.h>
#include <qdigest.h>

#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))

#define KEY_PAD 4
#define VALUE_PAD 4

struct Key {
  int k;
  static constexpr char padding[KEY_PAD] = {0};
  Key() : k(0) {}
  Key(int i) : k(i) {}
  bool empty() { return k == 0; }
};

struct Value {
  int v;
  static constexpr char padding[VALUE_PAD] = {0};
  Value() : v(0) {}
  Value(int i) : v(i) {}
  bool empty() { return v == 0; }
};

namespace std {
template <>
struct equal_to<Key> {
  bool operator()(Key const& a, Key const& b) const { return a.k == b.k; }
};
}

class RandomNumber {
 private:
  uint64_t _current;

 public:
  RandomNumber(uint64_t seed) : _current(seed) {}
  uint64_t next() {
    // https://en.wikipedia.org/wiki/Linear_congruential_generator
    _current = (48271 * _current % 2147483647);
    return _current;
  }
  uint64_t nextInRange(int range) {
    if (range == 0) {
      return 0;
    }
    next();
    return next() % range;
  }
};

class WeightedSelector {
 private:
  RandomNumber _r;
  std::vector<uint64_t> _cutoffs;

 public:
  WeightedSelector(uint64_t seed, std::vector<double> weights) : _r(seed) {
    double totalWeight = 0.0;
    for (uint64_t i = 0; i < weights.size(); i++) {
      totalWeight += weights[i];
      _cutoffs.push_back(std::ceil(totalWeight * 2147483647.0));
    }
  }
  uint64_t next() {
    uint64_t sample = _r.next();
    uint64_t i = 0;
    for (; i < _cutoffs.size(); i++) {
      if (sample < _cutoffs[i]) break;
    }
    return i;
  }
};

typedef HashWithSeed<Key, 0xdeadbeefdeadbeefULL> KeyHash;
typedef std::unordered_map<Key, Value*, KeyHash> unordered_map_for_key;

class TestMap {
 private:
  int _useCuckoo;
  CuckooMap<Key, Value> _cuckoo;
  unordered_map_for_key _unordered;

 public:
  TestMap(int useCuckoo, size_t initialSize)
      : _useCuckoo(useCuckoo), _cuckoo(initialSize), _unordered(initialSize) {}
  Value* lookup(Key const& k) {
    if (_useCuckoo) {
      auto element = _cuckoo.lookup(k);
      return (element.found() ? element.value() : nullptr);
    } else {
      auto element = _unordered.find(k);
      return (element != _unordered.end()) ? (*element).second : nullptr;
    }
  }
  bool insert(Key const& k, Value* v) {
    if (_useCuckoo) {
      return _cuckoo.insert(k, v);
    } else {
      return _unordered.emplace(k, v).second;
    }
  }
  bool remove(Key const& k) {
    if (_useCuckoo) {
      return _cuckoo.remove(k);
    } else {
      return (_unordered.erase(k) > 0);
    }
  }
};

// Usage: PerformanceTest [cuckoo] [nInitial] [nTotal] [nWorking] [pInsert]
//          [pLookup] [pRemove] [pWorking] [pMiss] [seed]
//    [cuckoo]: 1 = use CuckooMap; 0 = Use std::unordered_map
//    [nOpCount]: Number of operations to run
//    [nInitialSize]: Initial number of elements
//    [nMaxSize]: Maximum number of elements
//    [nWorking]: Size of working set
//    [pInsert]: Probability of insert
//    [pLookup]: Probability of lookup
//    [pRemove]: Probability of remove
//    [pWorking]: Probability of operation staying in working set
//    [pMiss]: Probability of lookup for missing element
//    [seed]: Seed for PRNG
int main(int argc, char* argv[]) {
  if (argc < 12) {
    std::cerr << "Incorrect number of parameters." << std::endl;
    return -1;
  }

  uint64_t useCuckoo = atoll(argv[1]);
  uint64_t nOpCount = atoll(argv[2]);
  uint64_t nInitialSize = atoll(argv[3]);
  uint64_t nMaxSize = atoll(argv[4]);
  uint64_t nWorking = atoll(argv[5]);
  double pInsert = atof(argv[6]);
  double pLookup = atof(argv[7]);
  double pRemove = atof(argv[8]);
  double pWorking = atof(argv[9]);
  double pMiss = atof(argv[10]);
  uint64_t seed = atoll(argv[11]);

  uint64_t nMaxTime = 3600;

  uint64_t nChunkSize = 1000000;  // will keep only the most recent X opcounts
                                  // to calculate percentiles, where nChunkSize
                                  // <= X <= 2*nChunkSize

  if (nInitialSize > nMaxSize || nWorking > nMaxSize) {
    std::cerr << "Invalid initial/total/working numbers." << std::endl;
    return -1;
  }

  if (pWorking < 0.0 || pWorking > 1.0) {
    std::cerr << "Keep 0 < pWorking < 1." << std::endl;
    return -1;
  }

  if (pMiss < 0.0 || pMiss > 1.0) {
    std::cerr << "Keep 0 < pMiss < 1." << std::endl;
    return -1;
  }

  RandomNumber r(seed);

  qdigest::QDigest* oldDigestI = nullptr;
  qdigest::QDigest* oldDigestL = nullptr;
  qdigest::QDigest* oldDigestR = nullptr;
  qdigest::QDigest* digestI = new qdigest::QDigest(10000);
  qdigest::QDigest* digestL = new qdigest::QDigest(10000);
  qdigest::QDigest* digestR = new qdigest::QDigest(10000);

  std::vector<double> opWeights;
  opWeights.push_back(pInsert);
  opWeights.push_back(pLookup);
  opWeights.push_back(pRemove);
  WeightedSelector operations(seed, opWeights);

  std::vector<double> workingWeights;
  workingWeights.push_back(1.0 - pWorking);
  workingWeights.push_back(pWorking);
  WeightedSelector working(seed, workingWeights);

  std::vector<double> missWeights;
  missWeights.push_back(1.0 - pMiss);
  missWeights.push_back(pMiss);
  WeightedSelector miss(seed, missWeights);

  TestMap map(useCuckoo, nInitialSize);
  uint64_t minElement = 1;
  uint64_t maxElement = 1;
  uint64_t opCode;
  uint64_t current;
  uint64_t barrier, nHot, nCold;
  bool success;
  Key* k;
  Value* v;

  // populate table to nInitialSize;
  for (uint64_t i = 0; i < nInitialSize; i++) {
    current = maxElement++;
    k = new Key(current);
    v = new Value(current);
    success = map.insert(*k, v);
    if (!success) {
      std::cout << "Failed to insert " << current << " with range ("
                << minElement << ", " << maxElement << ")" << std::endl;
      return -1;
    }
  }

  auto overallStart = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto currentStart = std::chrono::high_resolution_clock::now();
  auto currentFinish = std::chrono::high_resolution_clock::now();

  for (uint64_t i = 0; i < MAX((nOpCount / nChunkSize), 1); i++) {
    if (oldDigestI != nullptr) {
      delete oldDigestI;
      delete oldDigestL;
      delete oldDigestR;
    };
    oldDigestI = digestI;
    oldDigestL = digestL;
    oldDigestR = digestR;
    digestI = new qdigest::QDigest(10000);
    digestL = new qdigest::QDigest(10000);
    digestR = new qdigest::QDigest(10000);

    for (uint64_t j = 0; j < MIN(nChunkSize, (nOpCount - (i * nChunkSize)));
         j++) {
      opCode = operations.next();
      now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - overallStart)
              .count() > nMaxTime) {
        std::cout << "Took too long at " << j << " " << i << std::endl;
        break;
      }

      switch (opCode) {
        case 0:
          // insert if allowed
          if (maxElement - minElement >= nMaxSize) {
            break;
          }

          current = maxElement++;
          k = new Key(current);
          v = new Value(current);
          currentStart = std::chrono::high_resolution_clock::now();
          success = map.insert(*k, v);
          currentFinish = std::chrono::high_resolution_clock::now();
          if (!success) {
            std::cout << "Failed to insert " << current << " with range ("
                      << minElement << ", " << maxElement << ")" << std::endl;
            return -1;
          } else {
            digestI->insert(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    currentFinish - currentStart)
                    .count(),
                1);
            // std::cout << "Inserted " << current << std::endl;
          }
          delete k;
          delete v;
          break;
        case 1:
          // lookup
          barrier = MIN(minElement + nWorking, maxElement);
          nHot = barrier - minElement;
          nCold = maxElement - barrier;
          if (miss.next()) {
            current = maxElement + r.next();
          } else if (working.next()) {
            current = minElement + r.nextInRange(nHot);
          } else {
            current = nCold ? barrier + r.nextInRange(nCold)
                            : minElement + r.nextInRange(nHot);
          }

          k = new Key(current);
          currentStart = std::chrono::high_resolution_clock::now();
          v = map.lookup(current);
          currentFinish = std::chrono::high_resolution_clock::now();
          digestL->insert(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              currentFinish - currentStart)
                              .count(),
                          1);
          delete k;
          break;
        case 2:
          // remove if allowed
          if (minElement >= maxElement) {
            break;
          }
          current = working.next() ? minElement++ : --maxElement;

          k = new Key(current);
          currentStart = std::chrono::high_resolution_clock::now();
          success = map.remove(current);
          currentFinish = std::chrono::high_resolution_clock::now();
          if (!success) {
            std::cout << "Failed to remove " << current << " with range ("
                      << minElement << ", " << maxElement << ")" << std::endl;
            return -1;
          } else {
            digestR->insert(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    currentFinish - currentStart)
                    .count(),
                1);
            // std::cout << "Removed " << current << std::endl;
          }
          delete k;
          break;
        default:
          break;
      }
    }
  }

  if (oldDigestI != nullptr) {
    digestI->merge(*oldDigestI);
    digestL->merge(*oldDigestL);
    digestR->merge(*oldDigestR);
    delete oldDigestI;
    delete oldDigestL;
    delete oldDigestR;
  }

  std::cout << digestI->percentile(0.500) << "," << digestI->percentile(0.950)
            << "," << digestI->percentile(0.990) << ","
            << digestI->percentile(0.999) << "," << digestL->percentile(0.500)
            << "," << digestL->percentile(0.950) << ","
            << digestL->percentile(0.990) << "," << digestL->percentile(0.999)
            << "," << digestR->percentile(0.500) << ","
            << digestR->percentile(0.950) << "," << digestR->percentile(0.990)
            << "," << digestR->percentile(0.999) << std::endl;

  delete digestI;
  delete digestL;
  delete digestR;

  return 0;
}
