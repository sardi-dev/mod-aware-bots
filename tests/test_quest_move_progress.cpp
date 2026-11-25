#include "TestHarness.h"
#include "QuestMoveProgress.h"

TEST(quest_move_fixed_leg_progress_and_bounded_detours)
{
    QuestMoveProgress::Watch watch;
    QuestMoveProgress::DestinationProgress(watch, 1000);
    CHECK(!QuestMoveProgress::LegProgress(watch, 100, 0, 0, 100, 1000));
    CHECK(!QuestMoveProgress::LegProgress(watch, 100, 0, 0, 96, 2000));
    CHECK(QuestMoveProgress::LegProgress(watch, 100, 0, 0, 94, 3000));
    CHECK(!QuestMoveProgress::LegProgress(watch, 100, 0, 0, 96, 4000));
    CHECK(!QuestMoveProgress::LegProgress(watch, 200, 0, 0, 100, 5000));
    CHECK(!QuestMoveProgress::LegProgress(watch, 200, 0, 0, 80, 181000));
    QuestMoveProgress::DestinationProgress(watch, 181000);
    CHECK(QuestMoveProgress::LegProgress(watch, 200, 0, 0, 70, 182000));
}
