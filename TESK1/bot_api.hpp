#pragma once

#include <cstdint>
#include <optional>

namespace merlin {

enum class Direction : std::uint8_t { North, East, South, West };

enum class RobotStatus : std::uint8_t { 
    Normal, 
    TippedOver, // 翻倒
    Foul,       // 犯规（拾取/抛甩假KFS）
};

enum class KfsType : std::uint8_t {
    None,       
    R2Kfs,      
    R1Kfs,      
    FakeKfs     
};

// 动作定义
struct Action {
    // 1. 在结构体内部定义枚举类型 Type，并声明变量 type
    enum class Type : std::uint8_t { Move, Turn, Pickup, Throw, Recover } type;
    Direction direction; 
    
    // 2. 静态构造函数（使用 Type:: 来访问）
    static constexpr Action move() noexcept { 
        return {Type::Move, Direction::North}; 
    }
    static constexpr Action turn(Direction dir) noexcept { 
        return {Type::Turn, dir}; 
    }
    static constexpr Action pickup() noexcept { 
        return {Type::Pickup, Direction::North}; 
    }
    static constexpr Action throw_kfs() noexcept { 
        return {Type::Throw, Direction::North}; 
    }
    static constexpr Action recover() noexcept { 
        return {Type::Recover, Direction::North}; 
    }
};

struct CellInfo {
    std::int16_t height; // 200, 400, 600 (mm)
    KfsType kfs;
};

struct RobotState {
    std::int8_t x;
    std::int8_t y;
    Direction facing;
    RobotStatus status;
};

struct MatchStart {
    std::int8_t start_x;
    std::int8_t start_y;
    Direction start_facing;
    std::uint8_t max_actions; // 20
};

struct Observation {
    std::uint8_t actions_taken;                 // 已经消耗的行动次数
    RobotState self;                            // 自身状态（通过对比上一帧推导结果）
    std::optional<CellInfo> vision_front;       // 前方1格信息 (越界为空)
};

class Player {
public:
    virtual ~Player() = default;
    virtual void on_match_start(const MatchStart&) {}
    virtual Action act(const Observation& observation) = 0;
};

} // namespace merlin

extern "C" merlin::Player* create_player();