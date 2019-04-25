#!/usr/bin/env python3
from argparse import ArgumentParser, FileType
import csv
import cvxpy as cp
import numpy as np
import multiprocessing
import heapq as hq
import itertools as it
import sys
import time

class Job:
    def __init__(self, path, cache_type="homogenous", set_size=16):
        self.path = path
        self.cache_type = cache_type
        self.set_size = set_size

    def __str__(self):
        return "{} ({}, {} ways)".format(self.path, self.cache_type, self.set_size)

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
def normalize_intervals(integer_interval_tuples):
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

def solve_problem(prob):
    prob.solve(solver=cp.CBC, verbose=True, allowableFractionGap=0.01, CbcAllowableFractionGap=0.01)

    epsilon = 0.3
    nearest = round(prob.value)
    # for now, it appears to be floating point issue, and can't fix
    # going to round around it and hope that works
    # see: https://stackoverflow.com/questions/43194615/force-a-variable-to-be-an-integer-cvxpy
    # see: https://stackoverflow.com/questions/55423345/cvxpy-integer-programming-returning-non-integer-solution
    if abs(prob.value - nearest) > epsilon:
        raise RuntimeError("Floating point imprecision too large to ignore")
    return int(nearest)

def homogenous_optimal(intervals, set_size=16):
    """
    Computes the homogenous cache optimal for a set of usage intervals and set size (in ways). Uses an indicator
    variable per interval, and ensures that the total number of 'used' ways (based on current indicator values) is
    always less than or equal to the set size.
    """
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
    open_intervals = []
    for rowIdx in range(len(sorted_intervals)):
        row = sorted_intervals[rowIdx]
        hq.heappush(open_intervals, (row.end, row.cf, indicator[rowIdx]))
        for quanta in range(row.start, sorted_intervals[rowIdx+1].start if rowIdx + 1 < len(intervals) else number_of_timesteps):
            # close everything before this
            while len(open_intervals) > 0 and quanta >= open_intervals[0][0]:
                hq.heappop(open_intervals)

            # output constraints at current time
            # equations 1x, 2x, and 4x
            var_constraints.append(x1[quanta] == cp.sum([x[2] for x in open_intervals if x[1] == 1]))
            var_constraints.append(x2[quanta] == cp.sum([x[2] for x in open_intervals if x[1] == 2]))
            var_constraints.append(x4[quanta] == cp.sum([x[2] for x in open_intervals if x[1] == 4]))

    objective = cp.Maximize(cp.sum(indicator))
    constraints = it.chain(var_constraints,
        # equation s
        [x1 + q2 + q4 <= set_size],
        # equations 4c and 2c
        [x4 <= 4 * q4, 4 * q4 <= x4 + 3],
        [x2 <= 2 * q2, 2 * q2 <= x2 + 1],
    )

    return solve_problem(cp.Problem(objective, constraints))

def heterogenous_optimal(intervals, set_size=16):
    """
    Computes the heterogenous (i.e., no restriction on where to place cache lines) optimal for a set of usage intervals
    and set size.
    """
    # The number of "sub-blocks" (i.e., 16-byte chunks) is 4 times the raw set size.
    set_block_size = set_size * 4
    open_intervals = []

    sorted_intervals = sorted(intervals, key=lambda k: k.start)
    number_of_timesteps = max(map(lambda k: k.end, intervals))

    indicator = cp.Variable(len(intervals), boolean=True, name="indicator")

    constraints = []
    for rowIdx in range(len(sorted_intervals)):
        row = sorted_intervals[rowIdx]
        hq.heappush(open_intervals, (row.end, row.cf, indicator[rowIdx]))
        for quanta in range(row.start, sorted_intervals[rowIdx+1].start if rowIdx + 1 < len(intervals) else number_of_timesteps):
            # close everything before this
            while len(open_intervals) > 0 and quanta >= open_intervals[0][0]:
                hq.heappop(open_intervals)

            # output constraints at current time; only restriction is that sum of line sizes < set block size.
            constraints.append(cp.sum([x[1] * x[2] for x in open_intervals]) <= set_block_size)

    objective = cp.Maximize(cp.sum(indicator))
    return solve_problem(cp.Problem(objective, constraints))

def compute_optimal(intervals, cache_type, set_size = 16):
    """
    Computes an optimal on the set of usage intervals; the valid cache type options are "homogenous" or "heterogenous".
    """
    if cache_type == "homogenous":
        return homogenous_optimal(intervals, set_size)
    elif cache_type == "heterogenous":
        return heterogenous_optimal(intervals, set_size)
    else:
        raise RuntimeError("Unrecognized cache type {}".format(cache_type))

def process_file(job):
    with open(job.path, "r") as csv_file:
        interval_reader = csv.reader(csv_file)
        
        start_time = time.time()
        headers = next(interval_reader, None)
        assert(headers[0] == 'start' and headers[1] == 'end' and headers[2] == 'cf' and len(headers) == 3)

        intervals = [[int(y) for y in x] for x in interval_reader]
        if len(intervals) < job.set_size:
            return (job.path, job.set_size, len(intervals), time.time() - start_time)

        intervals, last_quanta = normalize_intervals(intervals)
        value = compute_optimal(intervals, job.cache_type, job.set_size)

        return (job.path, value, len(intervals), time.time() - start_time)


if __name__ == '__main__':
    parser = ArgumentParser(description="Creates an ILP from a CSV")
    parser.add_argument('csvs', nargs="+", help='The source intervals to create the ILP from. Must be readable files.')
    parser.add_argument("--model", default="homogenous", help="The type of cache to compute the optimal for; homogenous, heterogenous, or superblock")
    parser.add_argument("--size", default=16, help="The size of the cache (in uncompressed ways)")
    args = parser.parse_args()

    overall_start_time = time.time()
    with multiprocessing.Pool(processes=max(1, multiprocessing.cpu_count()//2)) as pool:
        jobs = [Job(path, args.model, args.size) for path in args.csvs]
        for file_name, optimal, interval_count, execution_time in pool.imap_unordered(process_file, jobs, 1):
            print("{0}: {1}/{2} ({3:.2f}s)".format(file_name, optimal, interval_count, execution_time))
            sys.stdout.flush()

    overall_finish_time = time.time()
    print("\nTotal Execution Time: {0:.2f}s".format(overall_finish_time - overall_start_time))
