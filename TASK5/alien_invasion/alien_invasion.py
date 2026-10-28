"""《外星人入侵》—— 主程序。

《Python 编程：从入门到实践》第 12～14 章的完整项目。
运行方式：在命令行中执行 `python alien_invasion.py`。
"""

import sys
from time import sleep

import pygame

from settings import Settings
from game_stats import GameStats
from scoreboard import Scoreboard
from button import Button
from ship import Ship
from bullet import Bullet
from alien import Alien


class AlienInvasion:
    """管理游戏资源和行为的总体类。"""

    def __init__(self):
        """初始化游戏，并创建游戏资源。"""
        pygame.init()
        self.clock = pygame.time.Clock()
        self.settings = Settings()

        # 创建游戏窗口，并设置窗口标题
        self.screen = pygame.display.set_mode(
            (self.settings.screen_width, self.settings.screen_height))
        pygame.display.set_caption("Alien Invasion")

        # 创建用于存储游戏统计信息的实例，并创建记分牌
        self.stats = GameStats(self)
        self.sb = Scoreboard(self)

        # 创建飞船、子弹编组和外星人编组
        self.ship = Ship(self)
        self.bullets = pygame.sprite.Group()
        self.aliens = pygame.sprite.Group()

        self._create_fleet()

        # 游戏一开始处于未激活状态，屏幕上显示“Play”按钮
        self.game_active = False

        # 创建“Play”按钮
        self.play_button = Button(self, "Play")

    def run_game(self):
        """启动游戏主循环。"""
        while True:
            # 监听键盘和鼠标事件
            self._check_events()

            # 只有游戏处于激活状态时，才更新游戏元素的位置
            if self.game_active:
                self.ship.update()
                self._update_bullets()
                self._update_aliens()

            # 重绘屏幕，并限制帧率为每秒 60 帧
            self._update_screen()
            self.clock.tick(60)

    def _check_events(self):
        """响应按键和鼠标事件。"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self._quit_game()
            elif event.type == pygame.KEYDOWN:
                self._check_keydown_events(event)
            elif event.type == pygame.KEYUP:
                self._check_keyup_events(event)
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mouse_pos = pygame.mouse.get_pos()
                self._check_play_button(mouse_pos)

    def _check_play_button(self, mouse_pos):
        """当玩家单击“Play”按钮时，开始新游戏。"""
        button_clicked = self.play_button.rect.collidepoint(mouse_pos)
        # 只有在游戏未激活时，单击按钮才有效
        if button_clicked and not self.game_active:
            # 重置游戏的动态设置（速度、分值等）
            self.settings.initialize_dynamic_settings()

            # 重置游戏统计信息
            self.stats.reset_stats()
            self.sb.prep_score()
            self.sb.prep_level()
            self.sb.prep_ships()
            self.game_active = True

            # 清空残留的子弹和外星人
            self.bullets.empty()
            self.aliens.empty()

            # 创建新的外星人舰队，并让飞船重新居中
            self._create_fleet()
            self.ship.center_ship()

            # 隐藏鼠标光标
            pygame.mouse.set_visible(False)

    def _check_keydown_events(self, event):
        """响应按键按下事件。"""
        if event.key == pygame.K_RIGHT:
            self.ship.moving_right = True
        elif event.key == pygame.K_LEFT:
            self.ship.moving_left = True
        elif event.key == pygame.K_q:
            self._quit_game()
        elif event.key == pygame.K_SPACE:
            self._fire_bullet()

    def _check_keyup_events(self, event):
        """响应按键松开事件。"""
        if event.key == pygame.K_RIGHT:
            self.ship.moving_right = False
        elif event.key == pygame.K_LEFT:
            self.ship.moving_left = False

    def _fire_bullet(self):
        """创建一颗新子弹，并将其加入子弹编组。"""
        # 只有屏幕上子弹数量未达到上限时才发射
        if len(self.bullets) < self.settings.bullets_allowed:
            new_bullet = Bullet(self)
            self.bullets.add(new_bullet)

    def _update_bullets(self):
        """更新子弹的位置，并删除已经消失的子弹。"""
        # 更新所有子弹的位置
        self.bullets.update()

        # 删除飞出屏幕顶部的子弹（用副本遍历，避免边遍历边修改编组）
        for bullet in self.bullets.copy():
            if bullet.rect.bottom <= 0:
                self.bullets.remove(bullet)

        self._check_bullet_alien_collisions()

    def _check_bullet_alien_collisions(self):
        """响应子弹和外星人的碰撞。"""
        # 删除发生碰撞的子弹和外星人
        collisions = pygame.sprite.groupcollide(
                self.bullets, self.aliens, True, True)

        # 如果有外星人被击中，就增加得分
        if collisions:
            for aliens in collisions.values():
                self.stats.score += self.settings.alien_points * len(aliens)
            self.sb.prep_score()
            self.sb.check_high_score()

        # 如果外星人舰队已被全部消灭，就进入下一关
        if not self.aliens:
            # 清空现有子弹，并创建新的外星人舰队
            self.bullets.empty()
            self._create_fleet()
            self.settings.increase_speed()

            # 关卡数加 1，并更新记分牌
            self.stats.level += 1
            self.sb.prep_level()

    def _ship_hit(self):
        """响应飞船被外星人撞到。"""
        if self.stats.ships_left > 0:
            # 剩余飞船数减 1，并更新记分牌
            self.stats.ships_left -= 1
            self.sb.prep_ships()

            # 清空残留的子弹和外星人
            self.bullets.empty()
            self.aliens.empty()

            # 创建新的外星人舰队，并让飞船重新居中
            self._create_fleet()
            self.ship.center_ship()

            # 暂停片刻，让玩家意识到飞船被撞到了
            sleep(0.5)
        else:
            # 没有剩余飞船，游戏结束，并重新显示鼠标光标
            self.game_active = False
            pygame.mouse.set_visible(True)

    def _update_aliens(self):
        """检查外星人舰队是否位于屏幕边缘，然后更新所有外星人的位置。"""
        self._check_fleet_edges()
        self.aliens.update()

        # 检查外星人与飞船是否发生碰撞
        if pygame.sprite.spritecollideany(self.ship, self.aliens):
            self._ship_hit()

        # 检查是否有外星人到达屏幕底部
        self._check_aliens_bottom()

    def _check_aliens_bottom(self):
        """检查是否有外星人到达了屏幕底部。"""
        for alien in self.aliens.sprites():
            if alien.rect.bottom >= self.settings.screen_height:
                # 把这种情况当作飞船被撞到一样处理
                self._ship_hit()
                break

    def _create_fleet(self):
        """创建外星人舰队。"""
        # 创建一个外星人，并不断添加，直到没有空间为止
        # 外星人之间的间距为一个外星人的宽度（列）和一个外星人的高度（行）
        alien = Alien(self)
        alien_width, alien_height = alien.rect.size

        current_x, current_y = alien_width, alien_height
        # 最后三行留给飞船活动，避免外星人一开始就压到飞船
        while current_y < (self.settings.screen_height - 3 * alien_height):
            while current_x < (self.settings.screen_width - 2 * alien_width):
                self._create_alien(current_x, current_y)
                current_x += 2 * alien_width

            # 一行排列完毕；重置 x 值，并增加 y 值
            current_x = alien_width
            current_y += 2 * alien_height

    def _create_alien(self, x_position, y_position):
        """创建一个外星人，并将其放入舰队中。"""
        new_alien = Alien(self)
        new_alien.x = x_position
        new_alien.rect.x = x_position
        new_alien.rect.y = y_position
        self.aliens.add(new_alien)

    def _check_fleet_edges(self):
        """如果外星人舰队到达了屏幕边缘，就作出相应处理。"""
        for alien in self.aliens.sprites():
            if alien.check_edges():
                self._change_fleet_direction()
                break

    def _change_fleet_direction(self):
        """让整个外星人舰队下移，并改变它们的移动方向。"""
        for alien in self.aliens.sprites():
            alien.rect.y += self.settings.fleet_drop_speed
        self.settings.fleet_direction *= -1

    def _update_screen(self):
        """更新屏幕上的图像，并切换到新屏幕。"""
        # 每次循环都重绘背景
        self.screen.fill(self.settings.bg_color)

        # 绘制子弹、飞船和外星人
        for bullet in self.bullets.sprites():
            bullet.draw_bullet()
        self.ship.blitme()
        self.aliens.draw(self.screen)

        # 绘制得分信息（得分、最高分、关卡、剩余飞船）
        self.sb.show_score()

        # 如果游戏未激活，就绘制“Play”按钮
        if not self.game_active:
            self.play_button.draw_button()

        # 让最近绘制的屏幕可见
        pygame.display.flip()

    def _quit_game(self):
        """清理 pygame 资源并退出程序。"""
        pygame.quit()
        sys.exit()


if __name__ == '__main__':
    # 创建游戏实例，并运行游戏
    ai = AlienInvasion()
    ai.run_game()
