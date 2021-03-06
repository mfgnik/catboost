#include <catboost/libs/data_new/objects_grouping.h>

#include <catboost/libs/index_range/index_range.h>
#include <catboost/libs/helpers/vector_helpers.h>

#include <util/generic/xrange.h>

#include <library/unittest/registar.h>


using namespace NCB;


Y_UNIT_TEST_SUITE(TObjectsGrouping) {
    Y_UNIT_TEST(Basic) {
        // trivial
        {
            TObjectsGrouping grouping(10);

            UNIT_ASSERT_VALUES_EQUAL(grouping.GetObjectCount(), 10);
            UNIT_ASSERT_VALUES_EQUAL(grouping.GetGroupCount(), 10);
            UNIT_ASSERT(grouping.IsTrivial());

            for (auto i : xrange(10)) {
                UNIT_ASSERT_EQUAL(grouping.GetGroup(i), TGroupBounds(i, i + 1));
            }

            UNIT_ASSERT_EXCEPTION(grouping.GetNonTrivialGroups(), TCatboostException);
        }

        // non-trivial
        {
            TVector<TGroupBounds> groupsBounds = {{0, 1}, {1, 3}, {3, 10}};

            TObjectsGrouping grouping{TVector<TGroupBounds>(groupsBounds)};

            UNIT_ASSERT_VALUES_EQUAL(grouping.GetObjectCount(), 10);
            UNIT_ASSERT_VALUES_EQUAL(grouping.GetGroupCount(), (ui32)groupsBounds.size());
            UNIT_ASSERT(!grouping.IsTrivial());

            for (auto i : xrange(groupsBounds.size())) {
                UNIT_ASSERT_EQUAL(grouping.GetGroup(i), groupsBounds[i]);
            }

            UNIT_ASSERT(Equal(grouping.GetNonTrivialGroups(), groupsBounds));
        }

        // bad
        {
            TVector<TGroupBounds> groupsBounds = {{0, 1}, {2, 5}};

            UNIT_ASSERT_EXCEPTION(TObjectsGrouping(std::move(groupsBounds)), TCatboostException);
        }
    }

    Y_UNIT_TEST(GetSubset) {
        TVector<TGroupBounds> groups1 = {
            {0, 1},
            {1, 3},
            {3, 10},
            {10, 12},
            {12, 13},
            {13, 20},
            {20, 25},
            {25, 27},
            {27, 33},
            {33, 42}
        };

        TVector<TObjectsGrouping> objectGroupings;
        objectGroupings.emplace_back(10); // trivial
        objectGroupings.emplace_back(TVector<TGroupBounds>(groups1));


        TVector<TArraySubsetIndexing<ui32>> subsetVector;
        subsetVector.emplace_back(TFullSubset<ui32>(10));

        {
            TVector<TIndexRange<ui32>> indexRanges{{7, 10}, {2, 3}, {4, 6}};
            TSavedIndexRanges<ui32> savedIndexRanges(std::move(indexRanges));
            subsetVector.emplace_back(TRangesSubset<ui32>(savedIndexRanges));
        }

        subsetVector.emplace_back(TIndexedSubset<ui32>{8, 9, 0, 2});


        using TExpectedMapIndex = std::pair<ui32, ui32>;

        // (objectGroupings idx, subsetVector idx) -> expectedSubset
        THashMap<TExpectedMapIndex, TObjectsGroupingSubset> expectedSubsets;

        for (auto subsetIdx : xrange(subsetVector.size())) {
            expectedSubsets.emplace(
                TExpectedMapIndex(0, subsetIdx),
                TObjectsGroupingSubset(
                    MakeIntrusive<TObjectsGrouping>(subsetVector[subsetIdx].Size()),
                    TArraySubsetIndexing<ui32>(subsetVector[subsetIdx])
                )
            );
        }

        {
            TSavedIndexRanges<ui32> savedIndexRanges{TVector<TIndexRange<ui32>>(groups1)};
            expectedSubsets.emplace(
                TExpectedMapIndex(1, 0),
                TObjectsGroupingSubset(
                    MakeIntrusive<TObjectsGrouping>(TVector<TGroupBounds>(groups1)),
                    TArraySubsetIndexing<ui32>(subsetVector[0]),
                    MakeHolder<TArraySubsetIndexing<ui32>>(TFullSubset<ui32>(42))
                )
            );
        }

        {
            TVector<TIndexRange<ui32>> indexRanges{{25, 42}, {3, 10}, {12, 20}};
            TSavedIndexRanges<ui32> savedIndexRanges(std::move(indexRanges));
            expectedSubsets.emplace(
                TExpectedMapIndex(1, 1),
                TObjectsGroupingSubset(
                    MakeIntrusive<TObjectsGrouping>(
                        TVector<TGroupBounds>{{0, 2}, {2, 8}, {8, 17}, {17, 24}, {24, 25}, {25, 32}}
                    ),
                    TArraySubsetIndexing<ui32>(subsetVector[1]),
                    MakeHolder<TArraySubsetIndexing<ui32>>(TRangesSubset<ui32>(savedIndexRanges))
                )
            );
        }

        {
            TVector<TIndexRange<ui32>> indexRanges{{27, 33}, {33, 42}, {0, 1}, {3, 10}};
            TSavedIndexRanges<ui32> savedIndexRanges(std::move(indexRanges));
            expectedSubsets.emplace(
                TExpectedMapIndex(1, 2),
                TObjectsGroupingSubset(
                    MakeIntrusive<TObjectsGrouping>(
                        TVector<TGroupBounds>{{0, 6}, {6, 15}, {15, 16}, {16, 23}}
                    ),
                    TArraySubsetIndexing<ui32>(subsetVector[2]),
                    MakeHolder<TArraySubsetIndexing<ui32>>(TRangesSubset<ui32>(savedIndexRanges))
                )
            );
        }

        for (auto objectGroupingIdx : xrange(objectGroupings.size())) {
            for (auto subsetIdx : xrange(subsetVector.size())) {
                TObjectsGroupingSubset subset = GetSubset(
                    MakeIntrusive<TObjectsGrouping>(objectGroupings[objectGroupingIdx]),
                    TArraySubsetIndexing<ui32>(subsetVector[subsetIdx])
                );
                const auto& expectedSubset = expectedSubsets.at(TExpectedMapIndex(objectGroupingIdx, subsetIdx));
                UNIT_ASSERT_EQUAL(subset, expectedSubset);
            }
        }

    }
}
