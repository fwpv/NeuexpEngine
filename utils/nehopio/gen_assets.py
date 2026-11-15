"""Генерация ассетов и уровня для демо nehopio.

Запуск из любой папки: python utils/nehopio/gen_assets.py [папка_вывода]
Без аргумента пишет в assets/nehopio рядом с корнем проекта.
"""

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

PROJECT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT / "assets" / "nehopio"
OUT.mkdir(parents=True, exist_ok=True)

CAP = (214, 48, 48, 255)
CAP_DARK = (156, 32, 32, 255)
CAP_LIGHT = (240, 96, 88, 255)
SKIN = (247, 201, 160, 255)
SKIN_DARK = (204, 152, 112, 255)
HAIR = (58, 36, 24, 255)
SHIRT = (44, 104, 220, 255)
SHIRT_DARK = (30, 72, 160, 255)
OVERALL = (26, 58, 138, 255)
OVERALL_DARK = (16, 38, 98, 255)
BOOT = (150, 100, 54, 255)
BOOT_DARK = (52, 30, 16, 255)
WHITE = (255, 255, 255, 255)
BLACK = (28, 22, 20, 255)
YELLOW = (250, 204, 60, 255)
YELLOW_DARK = (208, 156, 32, 255)
GOLD_LIGHT = (255, 240, 168, 255)


def unit_canvas():
    return Image.new("RGBA", (32, 32), (0, 0, 0, 0))


def px(draw, x, y, w, h, color):
    """Прямоугольник в единицах 2x2 пикселя."""
    draw.rectangle([x * 2, y * 2, x * 2 + w * 2 - 1, y * 2 + h * 2 - 1], fill=color)


def boot(draw, x, y, width):
    """Башмак одной формы и цвета для обеих ног: светлый верх, тёмная подошва."""
    dot(draw, x, y, width, 2, BOOT)
    dot(draw, x, y + 2, width, 1, BOOT_DARK)


def dot(draw, x, y, w, h, color):
    """Прямоугольник в единицах 1 пиксель: арт игрока рисуется с пиксельной точностью."""
    draw.rectangle([x, y, x + w - 1, y + h - 1], fill=color)


def player_head(draw):
    """Голова в профиль лицом вправо: кепка с козырьком, глаз, нос и усы."""
    dot(draw, 9, 2, 14, 4, CAP)
    dot(draw, 11, 2, 8, 2, CAP_LIGHT)
    dot(draw, 9, 5, 14, 1, CAP_DARK)
    dot(draw, 9, 6, 19, 2, CAP_DARK)
    dot(draw, 9, 6, 19, 1, CAP)
    dot(draw, 9, 8, 15, 6, SKIN)
    dot(draw, 9, 8, 3, 6, HAIR)
    dot(draw, 18, 8, 4, 1, HAIR)
    dot(draw, 24, 10, 2, 3, SKIN)
    dot(draw, 18, 9, 3, 3, WHITE)
    dot(draw, 19, 10, 2, 2, BLACK)
    dot(draw, 16, 13, 9, 2, HAIR)


def player_arm(draw, x, y, swing, sleeve, hand, mirrored=False):
    """Рука от плеча (x, y): рукав и кисть в фазе взмаха.

    Координаты заданы для правой руки; зеркальная рисуется от того же плеча,
    поэтому обе руки качаются одинаково и выходят каждая из своего рукава."""
    def part(left, top, width, height, color):
        dot(draw, 30 - left - width if mirrored else left, top, width, height, color)

    if swing == "down":
        part(x, y, 3, 5, sleeve)
        part(x + 1, y + 5, 3, 2, hand)
    elif swing == "front":
        part(x, y + 1, 3, 3, sleeve)
        part(x + 2, y + 4, 3, 2, hand)
    elif swing == "up":
        part(x + 1, y - 5, 3, 4, sleeve)
        part(x + 3, y - 7, 3, 2, hand)
    else:  # "out"
        part(x, y, 3, 2, sleeve)
        part(x + 3, y, 3, 2, hand)


def player_far_arm(draw, swing):
    """Дальняя рука выходит из левого рукава и рисуется до головы."""
    player_arm(draw, 21, 15, swing, SHIRT_DARK, SKIN_DARK, mirrored=True)


def player_near_arm(draw, swing):
    """Ближняя рука выходит из правого рукава; её рисуют поверх головы открытой кистью."""
    player_arm(draw, 21, 15, swing, SHIRT, SKIN)


def player_torso(draw):
    """Корпус в профиль: рубашка с рукавами, грудка и пояс комбинезона."""
    dot(draw, 13, 14, 6, 1, SKIN)
    dot(draw, 8, 15, 14, 5, SHIRT)
    dot(draw, 8, 15, 2, 5, SHIRT_DARK)
    dot(draw, 12, 14, 2, 2, OVERALL)
    dot(draw, 13, 15, 7, 5, OVERALL)
    dot(draw, 8, 19, 14, 3, OVERALL)


def legs_stand(draw):
    """Ноги одинакового цвета и с башмаками; башмаки доходят до низа кадра."""
    dot(draw, 9, 22, 4, 7, OVERALL)
    boot(draw, 8, 29, 6)
    dot(draw, 16, 22, 4, 7, OVERALL)
    boot(draw, 16, 29, 7)


def legs_run(draw, offset):
    """Четыре кадра шага: опора, проход, опора, проход с поднятой дальней ногой."""
    if offset == 0:
        # Опора: ближняя нога впереди, дальняя позади.
        dot(draw, 9, 22, 4, 7, OVERALL)
        boot(draw, 8, 29, 6)
        dot(draw, 17, 22, 4, 7, OVERALL)
        boot(draw, 17, 29, 7)
    elif offset == 1:
        # Проход: дальняя нога опорная, ближняя поднята вперёд.
        dot(draw, 9, 22, 4, 7, OVERALL)
        boot(draw, 8, 29, 6)
        dot(draw, 17, 22, 4, 4, OVERALL)
        boot(draw, 18, 25, 6)
    elif offset == 2:
        # Опора: та же стойка, что и в первом кадре.
        dot(draw, 9, 22, 4, 7, OVERALL)
        boot(draw, 8, 29, 6)
        dot(draw, 17, 22, 4, 7, OVERALL)
        boot(draw, 17, 29, 7)
    else:
        # Проход: опорная ближняя нога, дальняя поднята.
        dot(draw, 11, 22, 4, 4, OVERALL)
        boot(draw, 11, 25, 6)
        dot(draw, 17, 22, 4, 7, OVERALL)
        boot(draw, 17, 29, 7)


def legs_jump(draw):
    dot(draw, 9, 22, 4, 5, OVERALL)
    boot(draw, 8, 27, 6)
    dot(draw, 17, 22, 4, 4, OVERALL)
    boot(draw, 18, 25, 6)


def legs_fall(draw):
    dot(draw, 7, 22, 4, 6, OVERALL)
    boot(draw, 6, 28, 6)
    dot(draw, 17, 22, 4, 6, OVERALL)
    boot(draw, 18, 28, 6)


def player_frame(pose):
    image = unit_canvas()
    draw = ImageDraw.Draw(image)
    if pose in ("idle0", "idle1"):
        player_torso(draw)
        player_far_arm(draw, "down")
        legs_stand(draw)
        player_head(draw)
        player_near_arm(draw, "down")
    elif pose == "run0":
        player_torso(draw)
        player_far_arm(draw, "down")
        legs_run(draw, 0)
        player_head(draw)
        player_near_arm(draw, "down")
    elif pose == "run1":
        player_torso(draw)
        player_far_arm(draw, "front")
        legs_run(draw, 1)
        player_head(draw)
        player_near_arm(draw, "front")
    elif pose == "run2":
        player_torso(draw)
        player_far_arm(draw, "down")
        legs_run(draw, 2)
        player_head(draw)
        player_near_arm(draw, "down")
    elif pose == "run3":
        player_torso(draw)
        player_far_arm(draw, "front")
        legs_run(draw, 3)
        player_head(draw)
        player_near_arm(draw, "front")
    elif pose == "jump":
        player_torso(draw)
        player_far_arm(draw, "up")
        legs_jump(draw)
        player_head(draw)
        player_near_arm(draw, "up")
    elif pose == "fall":
        player_torso(draw)
        player_far_arm(draw, "out")
        legs_fall(draw)
        player_head(draw)
        player_near_arm(draw, "out")
    elif pose == "win":
        player_torso(draw)
        player_far_arm(draw, "up")
        legs_stand(draw)
        player_head(draw)
        player_near_arm(draw, "up")
    return image


PLAYER_POSES = ["idle0", "idle1", "run0", "run1", "run2", "run3", "jump", "fall", "win"]
PLAYER_FRAME_NAMES = ["idle.0", "idle.1", "run.0", "run.1", "run.2", "run.3", "jump", "fall", "win"]


def write_atlas(filename, entries):
    """Описание атласа: имя участка, затем x y ширина высота. Формат engine/atlas.h."""
    lines = [f"{name} {x} {y} {width} {height}" for name, (x, y, width, height) in entries]
    (OUT / filename).write_text("\n".join(lines) + "\n", encoding="utf-8")


def strip_entries(names, frame_width, frame_height):
    """Участки подряд слева направо."""
    return [(name, (index * frame_width, 0, frame_width, frame_height)) for index, name in enumerate(names)]


def mirrored_strip_entries(names, frame_width, frame_height):
    """Участки слева направо, затем зеркальная половина листа с суффиксом .left."""
    entries = strip_entries(names, frame_width, frame_height)
    offset = len(names) * frame_width
    for index, name in enumerate(names):
        entries.append((f"{name}.left", (offset + index * frame_width, 0, frame_width, frame_height)))
    return entries


def build_player():
    frames = [player_frame(pose) for pose in PLAYER_POSES]
    mirrored = [frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT) for frame in frames]
    sheet = Image.new("RGBA", (32 * len(frames) * 2, 32), (0, 0, 0, 0))
    for index, frame in enumerate(frames + mirrored):
        sheet.paste(frame, (index * 32, 0))
    sheet.save(OUT / "player.png")
    write_atlas("player.atlas", mirrored_strip_entries(PLAYER_FRAME_NAMES, 32, 32))


def build_player_big():
    big = [frame.resize((48, 48), Image.Resampling.NEAREST) for frame in (player_frame(pose) for pose in PLAYER_POSES)]
    mirrored = [frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT) for frame in big]
    sheet = Image.new("RGBA", (48 * len(big) * 2, 48), (0, 0, 0, 0))
    for index, frame in enumerate(big + mirrored):
        sheet.paste(frame, (index * 48, 0))
    sheet.save(OUT / "player_big.png")
    write_atlas("player_big.atlas", mirrored_strip_entries(PLAYER_FRAME_NAMES, 48, 48))


def enemy_frame(kind):
    image = unit_canvas()
    draw = ImageDraw.Draw(image)
    body = (138, 82, 44, 255)
    body_dark = (96, 54, 28, 255)
    body_light = (176, 116, 68, 255)
    foot = (58, 40, 30, 255)
    if kind == "squash":
        px(draw, 2, 11, 12, 3, body)
        px(draw, 2, 11, 12, 1, body_light)
        px(draw, 1, 14, 14, 1, body_dark)
        px(draw, 5, 12, 1, 1, WHITE)
        px(draw, 10, 12, 1, 1, WHITE)
        return image
    px(draw, 3, 3, 10, 7, body)
    px(draw, 2, 4, 12, 5, body)
    px(draw, 4, 2, 8, 2, body_light)
    px(draw, 2, 9, 12, 2, body_dark)
    px(draw, 5, 4, 2, 1, body_dark)
    px(draw, 9, 5, 2, 1, body_dark)
    px(draw, 4, 6, 3, 3, WHITE)
    px(draw, 9, 6, 3, 3, WHITE)
    px(draw, 5, 7, 1, 2, BLACK)
    px(draw, 10, 7, 1, 2, BLACK)
    px(draw, 4, 7, 1, 1, BLACK)
    px(draw, 11, 7, 1, 1, BLACK)
    if kind == "walk0":
        px(draw, 3, 11, 4, 3, body_dark)
        px(draw, 9, 11, 4, 3, body_dark)
        px(draw, 2, 14, 5, 1, foot)
        px(draw, 9, 14, 5, 1, foot)
    else:
        px(draw, 4, 11, 3, 3, body_dark)
        px(draw, 10, 11, 3, 3, body_dark)
        px(draw, 3, 14, 5, 1, foot)
        px(draw, 10, 14, 4, 1, foot)
    return image


def build_enemy():
    frames = [enemy_frame("walk0"), enemy_frame("walk1"), enemy_frame("squash")]
    mirrored = [frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT) for frame in frames]
    sheet = Image.new("RGBA", (32 * 6, 32), (0, 0, 0, 0))
    for index, frame in enumerate(frames + mirrored):
        sheet.paste(frame, (index * 32, 0))
    sheet.save(OUT / "enemy.png")
    write_atlas("enemy.atlas", mirrored_strip_entries(["walk.0", "walk.1", "squash"], 32, 32))


def flyer_frame(kind):
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    body = (128, 84, 196, 255)
    dark = (92, 56, 148, 255)
    light = (176, 136, 232, 255)
    if kind == "squash":
        draw.rectangle([6, 20, 25, 27], fill=body)
        draw.rectangle([6, 20, 25, 21], fill=light)
        draw.rectangle([9, 22, 11, 23], fill=WHITE)
        draw.rectangle([20, 22, 22, 23], fill=WHITE)
        return image
    draw.ellipse([9, 8, 22, 24], fill=body)
    draw.ellipse([11, 10, 20, 20], fill=light)
    draw.rectangle([12, 13, 13, 15], fill=WHITE)
    draw.rectangle([18, 13, 19, 15], fill=WHITE)
    draw.rectangle([12, 14, 13, 15], fill=(30, 20, 40, 255))
    draw.rectangle([18, 14, 19, 15], fill=(30, 20, 40, 255))
    if kind == "up":
        draw.polygon([(9, 12), (1, 4), (3, 14)], fill=dark)
        draw.polygon([(22, 12), (30, 4), (28, 14)], fill=dark)
    else:
        draw.polygon([(9, 14), (1, 22), (5, 10)], fill=dark)
        draw.polygon([(22, 14), (30, 22), (26, 10)], fill=dark)
    draw.rectangle([12, 24, 14, 27], fill=dark)
    draw.rectangle([17, 24, 19, 27], fill=dark)
    return image


def build_flyer():
    frames = [flyer_frame("up"), flyer_frame("down"), flyer_frame("squash")]
    mirrored = [frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT) for frame in frames]
    sheet = Image.new("RGBA", (32 * 6, 32), (0, 0, 0, 0))
    for index, frame in enumerate(frames + mirrored):
        sheet.paste(frame, (index * 32, 0))
    sheet.save(OUT / "flyer.png")
    write_atlas("flyer.atlas", mirrored_strip_entries(["walk.0", "walk.1", "squash"], 32, 32))


def build_mushroom():
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([4, 6, 27, 25], fill=(206, 58, 52, 255))
    draw.ellipse([6, 8, 25, 22], fill=(232, 84, 70, 255))
    for spot in ((9, 10, 4, 3), (19, 12, 3, 3), (14, 18, 4, 3)):
        draw.ellipse([spot[0], spot[1], spot[0] + spot[2], spot[1] + spot[3]],
            fill=(255, 244, 224, 255))
    draw.rectangle([10, 20, 21, 29], fill=(238, 214, 178, 255))
    draw.rectangle([10, 20, 21, 21], fill=(252, 238, 208, 255))
    draw.rectangle([13, 24, 15, 25], fill=(40, 28, 24, 255))
    draw.rectangle([17, 24, 19, 25], fill=(40, 28, 24, 255))
    image.save(OUT / "mushroom.png")


def coin_frame(width):
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    left = (16 - width) // 2
    if width >= 8:
        draw.ellipse([left, 1, left + width - 1, 14], fill=YELLOW_DARK)
        draw.ellipse([left + 1, 2, left + width - 2, 13], fill=YELLOW)
        draw.ellipse([left + 3, 4, left + width - 4, 11], fill=GOLD_LIGHT)
        draw.rectangle([left + width // 2 - 1, 4, left + width // 2, 11], fill=YELLOW_DARK)
    else:
        draw.rectangle([left, 2, left + width - 1, 13], fill=YELLOW_DARK)
        draw.rectangle([left, 3, left + width - 1, 12], fill=YELLOW)
    return image


def build_coin():
    sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    for index, width in enumerate([14, 10, 5, 10]):
        sheet.paste(coin_frame(width), (index * 16, 0))
    sheet.save(OUT / "coin.png")
    write_atlas("coin.atlas", strip_entries(["spin.0", "spin.1", "spin.2", "spin.3"], 16, 16))


def tile_grass():
    image = Image.new("RGBA", (32, 32), (120, 76, 40, 255))
    draw = ImageDraw.Draw(image)
    for y in range(32):
        for x in range(32):
            shade = ((x * 7 + y * 13) % 11) * 3
            draw.point((x, y), fill=(104 + shade, 64 + shade // 2, 34 + shade // 3, 255))
    draw.rectangle([0, 0, 31, 9], fill=(64, 150, 66, 255))
    draw.rectangle([0, 0, 31, 3], fill=(96, 188, 92, 255))
    for x in range(0, 32, 6):
        draw.point((x + 1, 11), fill=(64, 150, 66, 255))
        draw.point((x + 4, 12), fill=(64, 150, 66, 255))
    draw.rectangle([0, 30, 31, 31], fill=(84, 52, 28, 255))
    return image


def tile_dirt():
    image = Image.new("RGBA", (32, 32), (120, 76, 40, 255))
    draw = ImageDraw.Draw(image)
    for y in range(32):
        for x in range(32):
            shade = ((x * 5 + y * 11) % 9) * 3
            draw.point((x, y), fill=(108 + shade, 70 + shade // 2, 38 + shade // 3, 255))
    for point in ((5, 6), (19, 9), (11, 18), (26, 22), (7, 27)):
        draw.rectangle([point[0], point[1], point[0] + 3, point[1] + 2], fill=(84, 54, 30, 255))
    draw.rectangle([0, 30, 31, 31], fill=(92, 58, 32, 255))
    return image


def tile_brick():
    image = Image.new("RGBA", (32, 32), (168, 74, 54, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 31, 31], fill=(168, 74, 54, 255))
    for row in range(4):
        y = row * 8
        draw.rectangle([0, y, 31, y + 1], fill=(120, 48, 36, 255))
        draw.rectangle([0, y, 31, y], fill=(210, 110, 86, 255))
        shift = 0 if row % 2 == 0 else 8
        for x in range(shift, 32, 16):
            draw.rectangle([x, y, x + 1, y + 7], fill=(120, 48, 36, 255))
    draw.rectangle([0, 0, 0, 31], fill=(120, 48, 36, 255))
    return image


def tile_block():
    image = Image.new("RGBA", (32, 32), (226, 160, 58, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 31, 31], fill=(226, 160, 58, 255))
    draw.rectangle([0, 0, 31, 2], fill=(255, 214, 120, 255))
    draw.rectangle([0, 29, 31, 31], fill=(160, 104, 30, 255))
    draw.rectangle([0, 0, 2, 31], fill=(255, 214, 120, 255))
    draw.rectangle([29, 0, 31, 31], fill=(160, 104, 30, 255))
    for corner in ((4, 4), (25, 4), (4, 25), (25, 25)):
        draw.rectangle([corner[0], corner[1], corner[0] + 2, corner[1] + 2], fill=(120, 76, 22, 255))
    draw.rectangle([11, 8, 20, 10], fill=(120, 76, 22, 255))
    draw.rectangle([18, 10, 20, 16], fill=(120, 76, 22, 255))
    draw.rectangle([14, 14, 19, 16], fill=(120, 76, 22, 255))
    draw.rectangle([15, 16, 17, 20], fill=(120, 76, 22, 255))
    draw.rectangle([15, 22, 17, 24], fill=(120, 76, 22, 255))
    draw.rectangle([11, 8, 20, 8], fill=(255, 232, 160, 255))
    return image


def tile_stone():
    image = Image.new("RGBA", (32, 32), (120, 124, 132, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 31, 31], fill=(120, 124, 132, 255))
    draw.rectangle([0, 0, 31, 2], fill=(158, 162, 170, 255))
    draw.rectangle([0, 29, 31, 31], fill=(88, 92, 100, 255))
    draw.rectangle([0, 0, 2, 31], fill=(158, 162, 170, 255))
    draw.rectangle([29, 0, 31, 31], fill=(88, 92, 100, 255))
    for point in ((8, 8), (20, 13), (13, 23)):
        draw.rectangle([point[0], point[1], point[0] + 4, point[1] + 2], fill=(104, 108, 116, 255))
    return image


def tile_used():
    image = Image.new("RGBA", (32, 32), (150, 110, 52, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 31, 31], fill=(150, 110, 52, 255))
    draw.rectangle([0, 0, 31, 2], fill=(186, 146, 82, 255))
    draw.rectangle([0, 29, 31, 31], fill=(108, 78, 36, 255))
    draw.rectangle([0, 0, 2, 31], fill=(186, 146, 82, 255))
    draw.rectangle([29, 0, 31, 31], fill=(108, 78, 36, 255))
    for corner in ((5, 5), (24, 5), (5, 24), (24, 24)):
        draw.rectangle([corner[0], corner[1], corner[0] + 3, corner[1] + 3], fill=(120, 88, 40, 255))
    return image


def build_tiles():
    names = ["ground", "soil", "brick", "question", "stone", "used"]
    tiles = [tile_grass(), tile_dirt(), tile_brick(), tile_block(), tile_stone(), tile_used()]
    sheet = Image.new("RGBA", (32 * len(tiles), 32), (0, 0, 0, 0))
    for index, tile in enumerate(tiles):
        sheet.paste(tile, (index * 32, 0))
    sheet.save(OUT / "tiles.png")
    write_atlas("tiles.atlas", strip_entries(names, 32, 32))


def build_flag():
    image = Image.new("RGBA", (32, 128), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle([14, 8, 17, 118], fill=(156, 160, 168, 255))
    draw.rectangle([14, 8, 15, 118], fill=(196, 200, 208, 255))
    draw.ellipse([11, 0, 20, 9], fill=(226, 190, 64, 255))
    draw.ellipse([13, 2, 17, 6], fill=(255, 236, 160, 255))
    draw.polygon([(18, 12), (31, 24), (18, 36)], fill=(46, 168, 96, 255))
    draw.polygon([(18, 12), (26, 19), (18, 26)], fill=(86, 208, 132, 255))
    draw.rectangle([8, 116, 23, 121], fill=(120, 124, 132, 255))
    draw.rectangle([8, 116, 23, 117], fill=(158, 162, 170, 255))
    image.save(OUT / "flag.png")


def build_cloud(variant):
    image = Image.new("RGBA", (96, 48), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if variant == 0:
        circles = [(28, 30, 18), (48, 26, 22), (68, 32, 16), (16, 36, 12), (80, 38, 10)]
    else:
        circles = [(30, 32, 20), (54, 26, 18), (74, 34, 14), (14, 38, 10)]
    for cx, cy, radius in circles:
        draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=WHITE)
    for cx, cy, radius in circles:
        draw.ellipse([cx - radius, cy - radius + 5, cx + radius, cy + radius], fill=(232, 242, 255, 255))
    return image


def build_clouds():
    sheet = Image.new("RGBA", (192, 48), (0, 0, 0, 0))
    sheet.paste(build_cloud(0), (0, 0))
    sheet.paste(build_cloud(1), (96, 0))
    sheet.save(OUT / "clouds.png")
    write_atlas("clouds.atlas", strip_entries(["cloud.0", "cloud.1"], 96, 48))


def build_hill():
    image = Image.new("RGBA", (160, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([0, 4, 159, 140], fill=(72, 158, 86, 255))
    draw.ellipse([0, 4, 159, 132], fill=(86, 178, 98, 255))
    draw.ellipse([22, 18, 138, 120], fill=(104, 196, 112, 255))
    for center in ((50, 44, 12), (96, 36, 10), (118, 52, 14)):
        draw.ellipse([center[0] - center[2], center[1] - center[2] // 2,
                      center[0] + center[2], center[1] + center[2] // 2], fill=(122, 208, 128, 255))
    image.save(OUT / "hill.png")


def build_bush():
    # Куст из трёх долей: средняя выше крайних, верхушки не обрезаны.
    image = Image.new("RGBA", (96, 48), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for cx, cy, radius, light in ((22, 33, 16, 12), (74, 33, 16, 12), (48, 28, 20, 15)):
        draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=(56, 138, 72, 255))
        draw.ellipse([cx - light, cy - light - 2, cx + light, cy + light - 2], fill=(78, 166, 90, 255))
    draw.rectangle([0, 42, 95, 47], fill=(48, 118, 62, 255))
    image.save(OUT / "bush.png")


def build_sky():
    image = Image.new("RGBA", (8, 256), (0, 0, 0, 255))
    draw = ImageDraw.Draw(image)
    top = (78, 158, 236)
    bottom = (201, 234, 252)
    for y in range(256):
        amount = y / 255
        color = tuple(int(top[index] + (bottom[index] - top[index]) * amount) for index in range(3)) + (255,)
        draw.rectangle([0, y, 7, y], fill=color)
    image.save(OUT / "sky.png")


def build_abyss():
    image = Image.new("RGBA", (32, 32), (20, 16, 26, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, 32, 4):
        for x in range(0, 32, 4):
            shade = 18 + ((x * 3 + y * 5) % 7) * 3
            draw.rectangle([x, y, x + 3, y + 3], fill=(shade, shade - 2, shade + 4, 255))
    image.save(OUT / "abyss.png")


def build_panel():
    image = Image.new("RGBA", (16, 16), (10, 16, 26, 176))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 15, 15], fill=(10, 16, 26, 176))
    draw.rectangle([0, 0, 15, 0], fill=(120, 190, 255, 90))
    draw.rectangle([0, 15, 15, 15], fill=(4, 8, 14, 200))
    image.save(OUT / "panel.png")


def build_level():
    width = 180
    height = 14
    grid = [["." for _ in range(width)] for _ in range(height)]

    def fill(x, y, char):
        if 0 <= x < width and 0 <= y < height:
            grid[y][x] = char

    def ground(x0, x1):
        for x in range(x0, x1 + 1):
            fill(x, 12, "#")
            fill(x, 13, "=")

    ground(0, width - 1)
    pits = [(22, 24), (47, 49), (74, 76), (101, 103), (128, 130), (152, 155)]
    for start, end in pits:
        for x in range(start, end + 1):
            fill(x, 12, ".")
            fill(x, 13, ".")

    # Парящие кирпичные платформы: ряд 8, под ними есть запас для прыжка.
    platforms = [(15, 18), (38, 41), (62, 65), (88, 91), (112, 115), (138, 141), (144, 147)]
    for x0, x1 in platforms:
        for x in range(x0, x1 + 1):
            fill(x, 8, "B")

    # Обучающие блоки рядом со стартом: гриб и монета.
    fill(12, 8, "M")
    fill(13, 8, "Q")

    for x in range(26, 29):
        fill(x, 8, "M" if x == 27 else "Q")
    for x in range(58, 61):
        fill(x, 8, "B")
    for x in range(94, 97):
        fill(x, 8, "M" if x == 95 else "Q")
    for x in range(158, 161):
        fill(x, 8, "B")

    for x in range(118, 121):
        fill(x, 9, "S")
    for x in range(134, 137):
        fill(x, 9, "S")

    # Лестница перед флагом: камень не ломается.
    for step in range(5):
        x = 163 + step
        for y in range(11 - step, 12):
            fill(x, y, "S")

    for x in (5, 6, 7):
        fill(x, 11, "o")
    for x in (19, 20):
        fill(x, 11, "o")
    for start, end in pits:
        middle = (start + end) // 2
        fill(start - 1, 8, "o")
        fill(middle, 7, "o")
        fill(end + 1, 8, "o")
    for x0, x1 in platforms:
        middle = (x0 + x1) // 2
        fill(x0 + 1, 7, "o")
        fill(middle, 6, "o")
        fill(x1 - 1, 7, "o")
    for x in (33, 34, 68, 69, 105, 106, 122, 123):
        fill(x, 11, "o")

    for x in (31, 44, 56, 84, 93, 108, 121, 133, 159):
        fill(x, 11, "E")
    # Летающие враги держатся над ровными участками.
    for x in (38, 60, 97):
        fill(x, 6, "V")

    fill(3, 11, "P")
    fill(172, 11, "F")

    rows = ["".join(row) for row in grid]
    (OUT / "level.txt").write_text("\n".join(rows) + "\n", encoding="utf-8")
    return rows


def build_preview():
    folder = PROJECT / "build" / "nehopio_check"
    if not folder.is_dir():
        return
    sheet = Image.new("RGBA", (1100, 420), (40, 48, 64, 255))
    entries = [
        ("player.png", (10, 10), 2),
        ("enemy.png", (10, 90), 2),
        ("flyer.png", (10, 170), 2),
        ("mushroom.png", (230, 170), 2),
        ("coin.png", (330, 170), 3),
        ("tiles.png", (10, 240), 2),
        ("flag.png", (360, 240), 1),
        ("clouds.png", (420, 10), 2),
        ("hill.png", (420, 120), 2),
        ("bush.png", (780, 120), 2),
    ]
    for name, position, factor in entries:
        image = Image.open(OUT / name)
        resized = image.resize((image.width * factor, image.height * factor), Image.Resampling.NEAREST)
        sheet.alpha_composite(resized, position)
    sheet.convert("RGB").save(folder / "assets_preview.png")


build_player()
build_player_big()
build_enemy()
build_flyer()
build_mushroom()
build_coin()
build_tiles()
build_flag()
build_clouds()
build_hill()
build_bush()
build_sky()
build_abyss()
build_panel()
rows = build_level()
build_preview()
print("assets:", sorted(path.name for path in OUT.iterdir()))
print("level:", len(rows), "rows x", len(rows[0]), "cols")
