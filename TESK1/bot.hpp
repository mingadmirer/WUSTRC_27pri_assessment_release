#pragma once

#include "bot_api.hpp"

// 实现文件
//
// 调用顺序：
//   on_match_start(start)   // 比赛开始，给一次初始信息
//   act(observation)        // 反复调用，直到成功 / 失败 / 步数用尽
//
// 注意：
//   1. 每次调用 act 都会消耗 1 次行动；返回非法动作同样计数。
//   2. observation.self 是当前状态；observation.vision_front 是
//      正前方 1 格的信息（越界为 std::nullopt）。
//   3. 已携带的 KFS 需要自己记录（储存上限 2）。
//   4. 异常状态（TippedOver / Foul）下需要调用 Action::recover()。
//   5. 坐标：x = 1..3 向右，y = 1..4 向梅林里增加（North）；
//      入口 (2,0) 朝 North，出口 (2,5)，两者都在梅林格子外。

class MyR2Bot final : public merlin::Player {
public:
    void on_match_start(const merlin::MatchStart& start) override {
        // TODO: 记录起点、初始朝向、行动上限
        (void)start;
    }

    merlin::Action act(const merlin::Observation& observation) override {
        // TODO: 在这里实现你的算法
        //
        // 常用信息：
        //   observation.self.x / .y / .facing / .status
        //   observation.vision_front->height   // 200 / 400 / 600
        //   observation.vision_front->kfs      // None / R2Kfs / R1Kfs / FakeKfs
        //   observation.actions_taken
        //
        // 返回示例：
        //   return merlin::Action::move();
        //   return merlin::Action::turn(merlin::Direction::East);
        //   return merlin::Action::pickup();
        //   return merlin::Action::throw_kfs();
        //   return merlin::Action::recover();
        (void)observation;
        return merlin::Action::move();  // 占位，保证接口完整
    }

private:
    // 示例：需要跨 act 保存的状态
    // merlin::MatchStart start_{};
    // std::uint8_t carried_r2_ = 0;
};

// 引擎加载器调用的工厂函数。
// 返回的指针由引擎负责 delete。
//
// 本文件作为头文件使用，因此用 inline 避免被多个翻译单元包含时重复定义。
// 若引擎以动态库方式加载该符号，请改为非 inline，并按平台加导出宏
// （如 __declspec(dllexport) 或 -Wl,--export-all-symbols）。
extern "C" inline merlin::Player* create_player() {
    return new MyR2Bot();
}
