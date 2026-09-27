# -*- coding: utf-8 -*-
"""
gen_media.py —— 把控制台程序的真实运行输出渲染为截图 PNG 与暴力破解动图 GIF
说明：所有文字内容均来自 capture/ 目录下程序的真实运行输出（仅补回交互时键入的回显）。
"""
from PIL import Image, ImageDraw, ImageFont
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAP = os.path.join(BASE, "capture")
SHOT = os.path.join(BASE, "docs", "screenshots")
os.makedirs(SHOT, exist_ok=True)

FONT_PATH = r"C:\Windows\Fonts\msyh.ttc"
FONT_SIZE = 15
LINE_H = 24
TOP_BAR = 34
PAD = 18
WIDTH = 1060

Font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
TitleFont = ImageFont.truetype(FONT_PATH, 12)


def color_of(line):
    if any(k in line for k in ("失败", "错误", "FAIL")):
        return (244, 135, 113)
    if any(k in line for k in ("通过", "OK", "完成", "成功", "一致", "还原")):
        return (78, 201, 176)
    if line.strip().startswith("[") and "]" in line[:16]:
        return (220, 220, 170)
    if any(k in line for k in ("=====", "S-DES 加解密系统")):
        return (97, 174, 238)
    return (204, 204, 204)


def render(lines, out_path, total_lines=None, cursor=True, dur_ms=None):
    """渲染终端风格画面。
    total_lines：画布按"总行数"固定，保证动图各帧尺寸一致、不跳变；
    cursor：在已输出内容的下一行绘制终端光标块。"""
    total = total_lines or len(lines)
    height = TOP_BAR + PAD + total * LINE_H + PAD
    img = Image.new("RGB", (WIDTH, height), (12, 12, 12))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, WIDTH, TOP_BAR], fill=(42, 42, 44))
    d.text((12, 9), "命令提示符 - sdes_console  |  S-DES 加解密系统（真实运行输出）",
           font=TitleFont, fill=(190, 190, 190))
    for i, dx in enumerate((WIDTH - 66, WIDTH - 44, WIDTH - 22)):
        d.ellipse([dx, 12, dx + 10, 22], fill=(80, 80, 80))
    y = TOP_BAR + PAD
    for ln in lines:
        d.text((PAD, y), ln.rstrip("\n"), font=Font, fill=color_of(ln))
        y += LINE_H
    if cursor:
        cy = TOP_BAR + PAD + len(lines) * LINE_H
        d.rectangle([PAD + 1, cy + 3, PAD + 10, cy + LINE_H - 5], fill=(220, 220, 220))
    if out_path:      # out_path 为空时只返回图像（用于动图逐帧合成）
        img.save(out_path)
        print("saved:", out_path)
    return img


def load(name, echoes):
    """读取捕获输出，并按顺序把键入内容回显到相应提示符之后。
    使用游标顺序插入，避免同一提示符多次替换时错位。"""
    with open(os.path.join(CAP, name), encoding="utf-8") as f:
        text = f.read()
    cursor = 0
    for prompt, typed in echoes:
        idx = text.find(prompt, cursor)
        if idx == -1:
            continue
        insert_at = idx + len(prompt)
        text = text[:insert_at] + typed + text[insert_at:]
        cursor = insert_at + len(typed)
    return text.splitlines()


# ---------- 第1关：基本加解密 ----------
g1 = load("g1.txt", [
    ("请选择功能：", "1\n"),
    ("请输入 8bit 明文（如 10101010）：", "10101010\n"),
    ("请输入 10bit 密钥（如 1010000010）：", "1010000010\n"),
    ("请选择功能：", "0\n"),
])
render(g1, os.path.join(SHOT, "第1关_基本加解密.png"))

# ---------- 第2关：交叉测试 ----------
g2 = load("g2.txt", [("请选择功能：", "2\n"), ("请选择功能：", "0\n")])
render(g2, os.path.join(SHOT, "第2关_交叉测试.png"))

# ---------- 第3关：字符串加解密 ----------
g3 = load("g3.txt", [
    ("请选择功能：", "3\n"),
    ("请输入明文字符串：", "Hi你好World123\n"),
    ("请输入 10bit 密钥：", "0111110101\n"),
    ("请选择功能：", "6\n"),
    ("请输入十六进制密文：", "C1DC8408D55480089FC30F4C02A59BC6\n"),
    ("请输入 10bit 密钥：", "0111110101\n"),
    ("请选择功能：", "0\n"),
])
render(g3, os.path.join(SHOT, "第3关_字符串加解密.png"))

# ---------- 第3关：TCP 传输（服务端 + 客户端并排） ----------
def read_raw(name):
    with open(os.path.join(CAP, name), encoding="utf-8") as f:
        return f.read().splitlines()

srv = ["========== TCP 服务端（127.0.0.1:8888） =========="] + read_raw("tcp_server.txt")
cli = ["========== TCP 客户端 ==========",
       "请输入 10bit 密钥：0111110101",
       "请输入要加密发送的明文（支持中文）：Hello中国S-DES"] + read_raw("tcp_client.txt")[1:]

half = WIDTH // 2
h = TOP_BAR + PAD + max(len(srv), len(cli)) * LINE_H + PAD
tcp = Image.new("RGB", (WIDTH, h), (12, 12, 12))
td = ImageDraw.Draw(tcp)
td.rectangle([0, 0, WIDTH, TOP_BAR], fill=(42, 42, 44))
td.text((12, 9), "TCP 传输演示 - 左：服务端（解密方）  右：客户端（加密发送方）  |  真实运行输出",
        font=TitleFont, fill=(190, 190, 190))
for pane, x0 in ((srv, 0), (cli, half)):
    y = TOP_BAR + PAD
    for ln in pane:
        td.text((x0 + PAD, y), ln, font=ImageFont.truetype(FONT_PATH, 13), fill=color_of(ln))
        y += LINE_H
    td.line([half - 1, TOP_BAR, half - 1, h], fill=(60, 60, 60), width=1)
tcp.save(os.path.join(SHOT, "第3关_TCP传输.png"))
print("saved: 第3关_TCP传输.png")

# ---------- 第4关：暴力破解（截图 + 动图） ----------
g4 = load("g4.txt", [
    ("请选择功能：", "4\n"),
    ("请输入 8bit 明文：", "10101010\n"),
    ("请输入 8bit 密文：", "01110100\n"),
])
render(g4, os.path.join(SHOT, "第4关_暴力破解.png"))

# 动图：所有帧共用同一画布高度（内容逐行追加，尺寸恒定、不跳变），
# 帧尾绘制终端光标，结尾光标闪烁模拟等待输入。
marks = [i for i, ln in enumerate(g4) if ln.startswith("[")]
segments = [g4[:m + 1] for m in marks] + [g4]
frames = [(seg, 900, True) for seg in segments]          # 进度推进帧
frames += [(g4, 420, True), (g4, 420, False)] * 3        # 光标闪烁
frames.append((g4, 2400, True))                          # 末帧停留

gif_frames = [render(seg, "", total_lines=len(g4), cursor=cur) for seg, _, cur in frames]
gif_frames[0].save(
    os.path.join(BASE, "docs", "暴力破解动图.gif"), save_all=True,
    append_images=gif_frames[1:], duration=[d for _, d, _ in frames], loop=0)
print("saved: 暴力破解动图.gif  (%d frames)" % len(gif_frames))

# ---------- 第5关：封闭性分析 + 全量扫描 ----------
g5 = load("g5.txt", [
    ("请选择功能：", "5\n"),
    ("请输入 8bit 明文：", "00000000\n"),
    ("请输入 8bit 密文：", "11101000\n"),
    ("请选择功能：", "0\n"),
])
render(g5, os.path.join(SHOT, "第5关_封闭性分析.png"))

g5b = load("g5b.txt", [("请选择功能：", "7\n"), ("请选择功能：", "0\n")])
render(g5b, os.path.join(SHOT, "第5关_全量封闭性扫描.png"))

print("ALL DONE")
