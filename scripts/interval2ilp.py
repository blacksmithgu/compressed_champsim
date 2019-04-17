from argparse import ArgumentParser, FileType
import csv
import cvxpy as cp
import numpy as np
import heapq as hq
import itertools as it

parser = ArgumentParser(description="Creates an ILP from a CSV")
parser.add_argument('csv', type=FileType('r'), help='The source intervals to create the ILP from. Must be a readable file.')

args = parser.parse_args()

intervalreader = csv.reader(args.csv)
headers = next(intervalreader, None)
assert(headers[0] == 'start' and headers[1] == 'end' and headers[2] == 'cf' and len(headers) == 3)

#start = row[0]
#end = row[1]
#cf = row[2]

def rankIntervals(x, y):
    if x[0] == y[0]:
        return x[1] > y[1]
    return x[0] > y[0]

# returns gapless access numbers
# we get this by default, we don't need it
def transformToNormalized(intervals):
    current_quanta = 0
    current_quanta_time = 0
    retIntervals = []
    # a priority queue for the intervals
    # (end_time, start_time, cf) tuples to sort correctly on the minheap
    open_intervals = []
    for interval in intervals:
        start = interval[0]
        end = interval[1]
        cf = interval[2]
        while len(open_intervals) > 0 and start >= open_intervals[0][0]:
            current_end_time, start_quanta, current_cf = hq.heappop(open_intervals)
            if current_end_time > current_quanta_time:
                current_quanta_time = current_end_time
                current_quanta += 1

            retIntervals.append(start_quanta, current_quanta, current_cf)
        if start > current_quanta_time:
            current_quanta_time = start
            current_quanta += 1
        hq.heappush(open_intervals, (end, current_quanta, cf))

    return retIntervals, current_quanta

intervals = sorted(intervalreader, rankIntervals)
biggest_interval = max(intervals, lambda x, y: x[1] > y[1])

# this is run per set
# indicator variable per interval
# interval is [start, end]
# number variable per line type
# a priority queue for the intervals
# (end time, cf, indicator) tuples to sort correctly on the minheap
open_intervals = []

indicator = cp.Variable(len(intervals))
q1 = cp.Variable(len(total_quanta))
q2 = cp.Variable(len(total_quanta))
x1 = cp.Variable(len(total_quanta))
x2 = cp.Variable(len(total_quanta))
x4 = cp.Variable(len(total_quanta))

indicators_per_quanta = np.zeros((len(),), dt=np.obj)

# transform into normalized access times
for rowIdx in range(len(intervals)):
    row = intervals[rowIdx]
    # no validation on input
    start = row[0]
    end = row[1]
    cf = row[2]
    # close everything before this
    while len(open_intervals) > 0 and start >= open_intervals[0][0]:
        current_end_time, current_cf = hq.heappop(open_intervals)

    hq.heappush(open_intervals, (end, cf, indicator[rowIdx]))
    # output constraints at current time
    onesum = cp.sum([x[2] for x in open_intervals if x[1] == 1])
    twosum = cp.sum([x[2] for x in open_intervals if x[1] == 2])
    foursum = cp.sum([x[2] for x in open_intervals if x[1] == 4])

    step_constraints.append([onesum + q2 + q4 <= SET_SIZE])

indicator_constraint = [0 <= indicator, indicator <= 1]
objective = cp.Maximize(cp.sum(indicator))

constraints = it.chain(indicator_constraint, step_constraints)

prob = cp.Problem(objective, constraints)

