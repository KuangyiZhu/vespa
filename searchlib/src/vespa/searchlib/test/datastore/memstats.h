// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// Copyright 2017 Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

namespace search {
namespace datastore {
namespace test {

/*
 * Class representing expected memory stats in unit tests.
 */
struct MemStats
{
    size_t _used;
    size_t _hold;
    size_t _dead;
    MemStats() : _used(0), _hold(0), _dead(0) {}
    MemStats(const MemoryUsage &usage)
        : _used(usage.usedBytes()),
          _hold(usage.allocatedBytesOnHold()),
          _dead(usage.deadBytes()) {}
    MemStats &used(size_t val) { _used += val; return *this; }
    MemStats &hold(size_t val) { _hold += val; return *this; }
    MemStats &dead(size_t val) { _dead += val; return *this; }
    MemStats &holdToDead(size_t val) {
        decHold(val);
        _dead += val;
        return *this;
    }
    MemStats &decHold(size_t val) {
        assert(_hold >= val);
        _hold -= val;
        return *this;
    }
};

}
}
}
