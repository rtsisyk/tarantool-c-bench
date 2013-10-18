#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Nicely formats results generated by example.lua

import os
from sys import argv
import json

def main():
    TEST_JUST = 8
    RPS_JUST=9
    PERCENT_JUST=10

    if len(argv) == 1:
        print 'Usage:', argv[0], ' files'
        return -1;

    results = []
    names = []
    for fname in argv[1:]:
        with open(fname) as f:
            results.append(json.loads(f.read()))
            name = os.path.splitext(os.path.basename(fname))[0]
            names.append(name)
    print ' vs '.join(names)
    print

    # Check sizes
    wlnum = len(results[0])
    for res in results:
        assert (wlnum == len(res))

    for wls in zip(*results):
        for (wlname, _wldata) in wls[1:]:
            assert(wls[0][0] == wlname)

        wlname = wls[0][0]
        print wlname
        print '-' * len(wlname)
        print

        # Print headers
        print '|', 'Test'.center(TEST_JUST), '|', names[0].center(RPS_JUST), '|',
        for name in names[1:]:
            print name.center(RPS_JUST + PERCENT_JUST + 1), '|',
        print

        # Print hline
        print '|', '-' * TEST_JUST, '|', '-' * RPS_JUST, '|',
        for k in wls[1:]:
            print '-' * (RPS_JUST + PERCENT_JUST + 1), '|',
        print

        for tests in zip(*[x[1] for x in wls]):
            for test in tests[1:]:
                assert(tests[0][0] == test[0])

            print '|', tests[0][0].rjust(TEST_JUST), '|',
            base = tests[0][1]
            print ("%.0f" % base).rjust(RPS_JUST), '|',
            for test in tests[1:]:
                rps = test[1]
                delta = (((rps / base) - 1.0) * 100)
                print ("%.f" % rps).rjust(RPS_JUST),
                print ("(%.2f%%)" % delta).rjust(PERCENT_JUST), '|',
            print
        print

if __name__ == '__main__':
    main()
