from pathlib import Path

import pygame
from pygame.sprite import Sprite

# 图片所在目录。使用 __file__ 生成绝对路径，
# 这样无论在哪个目录下运行游戏，都能正确找到图片。
IMAGE_DIR = Path(__file__).resolve().parent / "images"


class Ship(Sprite):
    """管理飞船的类。"""

    def __init__(self, ai_game):
        """初始化飞船，并设置它的初始位置。"""
        super().__init__()
        self.screen = ai_game.screen
        self.settings = ai_game.settings
        self.screen_rect = ai_game.screen.get_rect()

        # 加载飞船图像，并获取其外接矩形
        self.image = pygame.image.load(str(IMAGE_DIR / "ship.bmp"))
        self.rect = self.image.get_rect()

        # 每艘新飞船都放在屏幕底部的中央
        self.rect.midbottom = self.screen_rect.midbottom

        # 在属性 x 中存储一个浮点数，记录飞船的精确水平位置
        self.x = float(self.rect.x)

        # 移动标志；初始状态下飞船不移动
        self.moving_right = False
        self.moving_left = False

    def center_ship(self):
        """让飞船在屏幕上重新居中。"""
        self.rect.midbottom = self.screen_rect.midbottom
        self.x = float(self.rect.x)

    def update(self):
        """根据移动标志调整飞船的位置。"""
        # 更新飞船的 x 值（而不是 rect），以实现平滑移动
        if self.moving_right and self.rect.right < self.screen_rect.right:
            self.x += self.settings.ship_speed
        if self.moving_left and self.rect.left > 0:
            self.x -= self.settings.ship_speed

        # 根据 self.x 更新 rect 对象的位置
        self.rect.x = self.x

    def blitme(self):
        """在指定位置绘制飞船。"""
        self.screen.blit(self.image, self.rect)
