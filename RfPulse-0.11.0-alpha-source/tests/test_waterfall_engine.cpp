#include "waterfall/WaterfallEngine.h"

#include <gtest/gtest.h>

#include <vector>

using rfpulse::waterfall::WaterfallEngine;

TEST(WaterfallEngine, CurrentRowAdvancesCircularly)
{
    WaterfallEngine engine(3, 4);
    EXPECT_EQ(engine.currentRow(), 0u);

    std::vector<float> row(4, 1.0f);
    engine.pushRow(row.data(), row.size());
    EXPECT_EQ(engine.currentRow(), 1u);
    engine.pushRow(row.data(), row.size());
    EXPECT_EQ(engine.currentRow(), 2u);
    engine.pushRow(row.data(), row.size());
    EXPECT_EQ(engine.currentRow(), 0u); // ha dado la vuelta
}

TEST(WaterfallEngine, PushRowStoresValuesAtCurrentRowOffset)
{
    constexpr std::size_t rows = 2;
    constexpr std::size_t bins = 3;
    WaterfallEngine engine(rows, bins);

    const std::vector<float> rowA = {1.0f, 2.0f, 3.0f};
    const std::vector<float> rowB = {4.0f, 5.0f, 6.0f};

    engine.pushRow(rowA.data(), rowA.size());
    ASSERT_EQ(engine.currentRow(), 1u);
    const float* storedA = engine.data() + engine.currentRow() * bins;
    EXPECT_FLOAT_EQ(storedA[0], 1.0f);
    EXPECT_FLOAT_EQ(storedA[1], 2.0f);
    EXPECT_FLOAT_EQ(storedA[2], 3.0f);

    engine.pushRow(rowB.data(), rowB.size());
    ASSERT_EQ(engine.currentRow(), 0u);
    const float* storedB = engine.data() + engine.currentRow() * bins;
    EXPECT_FLOAT_EQ(storedB[0], 4.0f);
    EXPECT_FLOAT_EQ(storedB[1], 5.0f);
    EXPECT_FLOAT_EQ(storedB[2], 6.0f);
}

TEST(WaterfallEngine, IgnoresPushWithMismatchedBinCount)
{
    WaterfallEngine engine(4, 8);
    const std::vector<float> wrongSize(4, 1.0f);

    engine.pushRow(wrongSize.data(), wrongSize.size());
    EXPECT_EQ(engine.currentRow(), 0u); // no avanzo: la llamada se ignoro
}

TEST(WaterfallEngine, FilledRowCountSaturatesAtRowCount)
{
    constexpr std::size_t rows = 3;
    WaterfallEngine engine(rows, 2);
    const std::vector<float> row = {1.0f, 2.0f};

    EXPECT_EQ(engine.filledRowCount(), 0u);

    engine.pushRow(row.data(), row.size());
    EXPECT_EQ(engine.filledRowCount(), 1u);

    engine.pushRow(row.data(), row.size());
    EXPECT_EQ(engine.filledRowCount(), 2u);

    engine.pushRow(row.data(), row.size()); // da la vuelta (currentRow() vuelve a 0)
    EXPECT_EQ(engine.filledRowCount(), rows);

    engine.pushRow(row.data(), row.size()); // ya lleno: no debe superar rowCount
    EXPECT_EQ(engine.filledRowCount(), rows);
}

TEST(WaterfallEngine, IgnoredPushDoesNotAdvanceFilledRowCount)
{
    WaterfallEngine engine(4, 8);
    const std::vector<float> wrongSize(4, 1.0f);

    engine.pushRow(wrongSize.data(), wrongSize.size());
    EXPECT_EQ(engine.filledRowCount(), 0u);
}
