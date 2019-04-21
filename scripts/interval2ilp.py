#!/usr/bin/env python3
from argparse import ArgumentParser, FileType
import csv
import cvxpy as cp
import numpy as np
import heapq as hq
import itertools as it

class Interval:
    def __init__(self, start, end, cf):
        self.start = start
        self.end = end
        self.cf = cf

    def __str__(self):
        return "({} - {}, x{})".format(self.start, self.end, self.cf)

    def __repr__(self):
        return str(self)

    def to_tuple(self):
        return (self.start, self.end, self.cf)

# returns gapless access numbers
def transformToNormalizedIntervals(integer_interval_tuples):
    intervals = [Interval(x, y, z) for x, y, z in integer_interval_tuples]

    current_quanta = 0
    startTimes = sorted(intervals, key=lambda x: x.start)
    endTimes = sorted(intervals, key=lambda x: x.end)

    startIndex = 0
    endIndex = 0
    while startIndex < len(startTimes) or endIndex < len(endTimes):
        # Three cases: start < end, start > end, start == end. Third case only occurs if a reuse interval was both
        # ended/started by a reuse.
        if startIndex < len(startTimes) and endIndex < len(endTimes) and startTimes[startIndex].start == endTimes[endIndex].end:
            startTimes[startIndex].start = current_quanta
            endTimes[endIndex].end = current_quanta
            startIndex += 1
            endIndex += 1
        else:
            start_quanta = startTimes[startIndex].start if startIndex < len(startTimes) else float('inf')
            end_quanta = endTimes[endIndex].end if endIndex < len(endTimes) else float('inf')

            if start_quanta < end_quanta:
                startTimes[startIndex].start = current_quanta
                startIndex += 1
            else:
                endTimes[endIndex].end = current_quanta
                endIndex += 1

        current_quanta += 1

    return intervals, current_quanta - 1

def optimalForNormalizedIntervals(intervals, set_size=16):
    # this is run per set
    # indicator variable per interval
    # interval is [start, end]
    # number variable per line type
    # a priority queue for the intervals
    # (end time, cf, indicator) tuples to sort correctly on the minheap
    open_intervals = []
    sorted_intervals = sorted(intervals, key=lambda k: k.start)
    number_of_timesteps = max(map(lambda k: k.end, intervals))

    indicator = cp.Variable(len(intervals), boolean=True, name="indicator")
    q4        = cp.Variable(number_of_timesteps, integer=True, name="q4")
    q2        = cp.Variable(number_of_timesteps, integer=True, name="q2")
    x1        = cp.Variable(number_of_timesteps, integer=True, name="x1")
    x2        = cp.Variable(number_of_timesteps, integer=True, name="x2")
    x4        = cp.Variable(number_of_timesteps, integer=True, name="x4")

    # transform into normalized access times
    var_constraints = []
    for rowIdx in range(len(sorted_intervals)):
        row = sorted_intervals[rowIdx]
        hq.heappush(open_intervals, (row.end, row.cf, indicator[rowIdx]))
        for time in range(row.start, sorted_intervals[rowIdx+1].start if rowIdx + 1 < len(intervals) else number_of_timesteps):
            # close everything before this
            while len(open_intervals) > 0 and time >= open_intervals[0][0]:
                hq.heappop(open_intervals)

            # output constraints at current time
            # equations 1x, 2x, and 4x
            var_constraints.append(x1[time] == cp.sum([x[2] for x in open_intervals if x[1] == 1]))
            var_constraints.append(x2[time] == cp.sum([x[2] for x in open_intervals if x[1] == 2]))
            var_constraints.append(x4[time] == cp.sum([x[2] for x in open_intervals if x[1] == 4]))

    objective = cp.Maximize(cp.sum(indicator))
    constraints = it.chain(var_constraints,
        # equation s
        [x1 + q2 + q4 <= set_size],
        # equations 4c and 2c
        [x4 <= 4 * q4, 4 * q4 <= x4 + 3],
        [x2 <= 2 * q2, 2 * q2 <= x2 + 1],
    )

    prob = cp.Problem(objective, constraints)
    prob.solve(solver='GLPK_MI')
    epsilon = 0.3
    nearest = round(prob.value)
    # for now, it appears to be floating point issue, and can't fix
    # going to round around it and hope that works
    # see: https://stackoverflow.com/questions/43194615/force-a-variable-to-be-an-integer-cvxpy
    # see: https://stackoverflow.com/questions/55423345/cvxpy-integer-programming-returning-non-integer-solution
    if abs(prob.value - nearest) > epsilon:
        raise RuntimeError("Floating point imprecision too large to ignore")
    return int(nearest)


if __name__ == '__main__':
    parser = ArgumentParser(description="Creates an ILP from a CSV")
    parser.add_argument('csv', type=FileType('r'), help='The source intervals to create the ILP from. Must be a readable file.')

    args = parser.parse_args()

    intervalreader = csv.reader(args.csv)
    headers = next(intervalreader, None)
    assert(headers[0] == 'start' and headers[1] == 'end' and headers[2] == 'cf' and len(headers) == 3)

    intervals = [[int(y) for y in x] for x in intervalreader]
    intervals, last_quanta = transformToNormalizedIntervals(intervals)

    value = optimalForNormalizedIntervals(intervals)
    print("optimal value: ", value)
