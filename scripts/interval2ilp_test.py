# run with python3 -m pytest interval2ilp_test.py

from interval2ilp import transformToNormalizedIntervals, optimalForNormalizedIntervals

def test_identity_transform():
    test_identity = [
        (0, 5, 4)
    ]
    resultIntervals, resultQuanta = transformToNormalizedIntervals(test_identity)
    assert(len(resultIntervals) == 1)
    assert((0, 1, 4) == resultIntervals[0].to_tuple())
    assert(1 == resultQuanta)
    assert(1 == optimalForNormalizedIntervals(resultIntervals, set_size=2))
    assert(1 == optimalForNormalizedIntervals(resultIntervals, set_size=1))
    assert(0 == optimalForNormalizedIntervals(resultIntervals, set_size=0))


def test_overlapping_start_end():
    test_overlapping = [
        (0, 1, 4),
        (1, 3, 4),
        (3, 7, 4),
        (2, 6, 1)
    ]
    resultIntervals, resultQuanta = transformToNormalizedIntervals(test_overlapping)
    assert(len(resultIntervals) == 4)
    assert(resultIntervals[0].to_tuple() == (0, 1, 4))
    assert(resultIntervals[1].to_tuple() == (1, 3, 4))
    assert(resultIntervals[2].to_tuple() == (3, 5, 4))
    assert(resultIntervals[3].to_tuple() == (2, 4, 1))

def test_nearly_intact_transform():
    test_intervals = [
        (0, 2, 4),
        (3, 5, 2),
        (4, 12, 1), # the only one that should be changed
        (6, 8, 2),
        (7, 10, 4),
        (1, 9, 4)
    ]
    retIntervals, end_quanta = transformToNormalizedIntervals(test_intervals)
    assert(end_quanta == 11)
    assert(len(test_intervals) == len(retIntervals))
    for interval in retIntervals:
        if interval.start == 4:
            assert(interval.end == 11 and interval.cf == 1)
        else:
            # it should be unchanged
            tv = interval.to_tuple()
            assert(tv in test_intervals)
    #assert(4 == optimalForNormalizedIntervals(retIntervals, set_size=1))

def test_enhanced_reordering():
    test_intervals = [
        (3, 12, 4),
        (5, 39, 2),
        (18, 22, 1),
        (4, 32, 2)
    ]
    solutions = [
        (0, 3, 4),
        (2, 7, 2),
        (4, 5, 1),
        (1, 6, 2)
    ]
    retIntervals, end_quanta = transformToNormalizedIntervals(test_intervals)
    # true as a general rule
    assert(end_quanta == len(test_intervals) * 2 - 1)
    assert(len(test_intervals) == len(retIntervals))
    for interval, solution in zip(retIntervals, solutions):
        assert(interval.to_tuple() == solution)
    assert(2 == optimalForNormalizedIntervals(retIntervals, set_size=1))
