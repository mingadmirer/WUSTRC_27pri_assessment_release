class Settings:
    """存储《外星人入侵》全部设置的类。"""

    def __init__(self):
        """初始化游戏中的静态设置（不随游戏进程变化）。"""
        # 屏幕设置
        self.screen_width = 1200
        self.screen_height = 800
        self.bg_color = (230, 230, 230)  # 浅灰色背景

        # 飞船设置
        self.ship_limit = 3  # 初始生命值（飞船数量）

        # 子弹设置
        self.bullet_width = 3
        self.bullet_height = 15
        self.bullet_color = (60, 60, 60)  # 深灰色子弹
        self.bullets_allowed = 3  # 屏幕上同时允许存在的子弹数量

        # 外星人设置
        self.fleet_drop_speed = 10  # 外星人舰队碰到屏幕边缘后下移的像素数

        # 游戏提速倍率：每过一关，速度乘以该系数
        self.speedup_scale = 1.1
        # 外星人分值增长倍率：每过一关，分值乘以该系数
        self.score_scale = 1.5

        self.initialize_dynamic_settings()

    def initialize_dynamic_settings(self):
        """初始化会随游戏进程变化的设置（重新开始游戏时也会调用）。"""
        self.ship_speed = 1.5
        self.bullet_speed = 2.5
        self.alien_speed = 1.0

        # fleet_direction 为 1 表示向右移动，为 -1 表示向左移动
        self.fleet_direction = 1

        # 计分设置：消灭一个外星人的基础分值
        self.alien_points = 50

    def increase_speed(self):
        """提高飞船、子弹和外星人的速度，并提高外星人的分值。"""
        self.ship_speed *= self.speedup_scale
        self.bullet_speed *= self.speedup_scale
        self.alien_speed *= self.speedup_scale

        self.alien_points = int(self.alien_points * self.score_scale)
