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
        return "(start: {} end: {} x{})".format(self.start, self.end, self.cf)

    def __repr__(self):
        return str(self)

    def to_tuple(self):
        return (self.start, self.end, self.cf)

# returns gapless access numbers
# we get this by default, we don't need it
def transformToNormalizedIntervals(integer_interval_tuples):
    intervals = [Interval(x, y, z) for x, y, z in integer_interval_tuples]

    current_quanta = 0
    startTimes = sorted(intervals, key=lambda x: x.start)
    endTimes = sorted(intervals, key=lambda x: x.end)
    print("Start times: ", end='')
    print(startTimes)
    print("End times: ", end='')
    print(endTimes)

    startIndex = 0
    endIndex = 0
    while startIndex < len(startTimes) or endIndex < len(endTimes):
        # starts are earlier, set these first
        while startIndex < len(startTimes) and (endIndex == len(endTimes) or startTimes[startIndex].start <= endTimes[endIndex].end):
            startTimes[startIndex].start = current_quanta
            startIndex += 1
            current_quanta += 1

        while endIndex < len(endTimes) and (startIndex == len(startTimes) or startTimes[startIndex].start >= endTimes[endIndex].end):
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
    number_of_timesteps = 2 * len(intervals) - 1

    indicator = cp.Variable(len(intervals), integer=True)
    q4        = cp.Variable(number_of_timesteps, integer=True)
    q2        = cp.Variable(number_of_timesteps, integer=True)
    x1        = cp.Variable(number_of_timesteps, integer=True)
    x2        = cp.Variable(number_of_timesteps, integer=True)
    x4        = cp.Variable(number_of_timesteps, integer=True)

    # transform into normalized access times
    var_constraints = []
    for rowIdx in range(len(intervals)):
        row = intervals[rowIdx]
        # close everything before this
        while len(open_intervals) > 0 and row.start >= open_intervals[0][0]:
            current_end_time, current_cf, current_indicator = hq.heappop(open_intervals)

        hq.heappush(open_intervals, (row.end, row.cf, indicator[rowIdx]))
        # output constraints at current time
        # equations 1x, 2x, and 4x
        var_constraints.append(x1[row.start] == cp.sum([x[2] for x in open_intervals if x[1] == 1]))
        var_constraints.append(x2[row.start] == cp.sum([x[2] for x in open_intervals if x[1] == 2]))
        var_constraints.append(x4[row.start] == cp.sum([x[2] for x in open_intervals if x[1] == 4]))


    indicator_constraint = [0 <= indicator, indicator <= 1]
    objective = cp.Maximize(cp.sum(indicator))

    constraints = it.chain(indicator_constraint,
        # rest of variable constraints
        var_constraints,
        # equation s
        [x1 + q2 + q4 <= set_size],
        # equations 4c and 2c
        [x4 <= 4 * q4, 4 * q4 <= x4 + 3],
        [x2 <= 2 * q2, 2 * q2 <= x2 + 1],
    )

    prob = cp.Problem(objective, constraints)
    prob.solve();
    print(prob)
    print([x.value for x in indicator])
    print(prob.status)
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
    intervals = transformToNormalizedIntervals(intervals)

    value = optimalForNormalizedIntervals(intervals)
    #print("status:", prob.status)
    print("optimal value", value)
