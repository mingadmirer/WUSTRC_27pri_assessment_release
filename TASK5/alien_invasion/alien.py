from pathlib import Path

import pygame
from pygame.sprite import Sprite

# 图片所在目录。使用 __file__ 生成绝对路径，
# 这样无论在哪个目录下运行游戏，都能正确找到图片。
IMAGE_DIR = Path(__file__).resolve().parent / "images"


class Alien(Sprite):
    """表示单个外星人的类。"""

    def __init__(self, ai_game):
        """初始化外星人，并设置它的初始位置。"""
        super().__init__()
        self.screen = ai_game.screen
        self.settings = ai_game.settings

        # 加载外星人图像，并设置其 rect 属性
        self.image = pygame.image.load(str(IMAGE_DIR / "alien.bmp"))
        self.rect = self.image.get_rect()

        # 每个新外星人都从屏幕左上角附近开始
        self.rect.x = self.rect.width
        self.rect.y = self.rect.height

        # 用浮点数存储外星人的精确水平位置
        self.x = float(self.rect.x)

    def check_edges(self):
        """如果外星人位于屏幕边缘，就返回 True。"""
        screen_rect = self.screen.get_rect()
        return (self.rect.right >= screen_rect.right) or (self.rect.left <= 0)

    def update(self):
        """让外星人向左或向右移动。"""
        self.x += self.settings.alien_speed * self.settings.fleet_direction
        self.rect.x = self.x
