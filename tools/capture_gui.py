# -*- coding: utf-8 -*-
"""
capture_gui.py —— 驱动 SdesGui.exe 完成真实操作并截取窗口图像
方法：Win32 API（ctypes）：WM_SETTEXT 填入控件文本、BM_CLICK 触发按钮、
     PrintWindow(PW_RENDERFULLCONTENT) 捕获窗口为 PNG。全程为真实程序运行。
"""
import subprocess
import time
import os
import ctypes
from ctypes import wintypes
from PIL import Image

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(BASE, "build", "sdes_gui.exe")
OUT = os.path.join(BASE, "docs", "screenshots")

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_SETTEXT = 0x000C
BM_CLICK = 0x00F5
PW_RENDERFULLCONTENT = 0x00000002

# 控件 ID（与 src/SdesGui.cpp 保持一致）
IDC_PLAIN_BITS = 1001
IDC_KEY_BITS = 1002
IDC_BTN_ENCRYPT = 1003
IDC_BTN_DECRYPT = 1004
IDC_PLAIN_STRING = 1006
IDC_BTN_ENC_STR = 1007
IDC_BTN_DEC_STR = 1009
IDC_PLAIN_BITS_BF = 1011
IDC_CIPHER_BITS_BF = 1012
IDC_BTN_BRUTE = 1013


def SetControlText(MainHwnd, ControlId, Text):
    """向指定 ID 的控件发送 WM_SETTEXT（宽字符）。"""
    hCtrl = user32.GetDlgItem(MainHwnd, ControlId)
    user32.SendMessageW(hCtrl, WM_SETTEXT, 0, ctypes.c_wchar_p(Text))


def ClickButton(MainHwnd, ControlId):
    """点击指定 ID 的按钮（BM_CLICK）。"""
    hCtrl = user32.GetDlgItem(MainHwnd, ControlId)
    user32.SendMessageW(hCtrl, BM_CLICK, 0, 0)


def CaptureWindow(Hwnd, OutPath):
    """用 PrintWindow 把窗口渲染到内存位图并保存为 PNG。"""
    rect = wintypes.RECT()
    user32.GetWindowRect(Hwnd, ctypes.byref(rect))
    width = rect.right - rect.left
    height = rect.bottom - rect.top

    hdcWindow = user32.GetWindowDC(Hwnd)
    hdcMem = gdi32.CreateCompatibleDC(hdcWindow)
    hbmp = gdi32.CreateCompatibleBitmap(hdcWindow, width, height)
    gdi32.SelectObject(hdcMem, hbmp)

    # PW_RENDERFULLCONTENT：即使窗口被遮挡也能正确捕获（DirectX 渲染内容）
    user32.PrintWindow(Hwnd, hdcMem, PW_RENDERFULLCONTENT)

    # 提取位图像素
    class BMIH(ctypes.Structure):
        _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
                    ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
                    ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                    ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                    ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
                    ("biClrImportant", wintypes.DWORD)]

    class BMI(ctypes.Structure):
        _fields_ = [("bmiHeader", BMIH), ("biColors", wintypes.DWORD * 3)]

    bmi = BMI()
    bmi.bmiHeader.biSize = ctypes.sizeof(BMIH)
    bmi.bmiHeader.biWidth = width
    bmi.bmiHeader.biHeight = -height          # 负值：自顶向下
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = 0           # BI_RGB

    buf = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(hdcMem, hbmp, 0, height, buf, ctypes.byref(bmi), 0)

    img = Image.frombytes("RGBA", (width, height), buf.raw)
    img = img.convert("RGB")
    img.save(OutPath)
    print("saved:", OutPath)

    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdcMem)
    user32.ReleaseDC(Hwnd, hdcWindow)


def Main():
    # 启动 GUI 程序并等待窗口出现
    proc = subprocess.Popen([EXE])
    hwnd = None
    for _ in range(50):
        time.sleep(0.1)
        hwnd = user32.FindWindowW(None, "S-DES 加解密系统")
        if hwnd:
            break
    if not hwnd:
        proc.kill()
        raise RuntimeError("未找到 GUI 窗口")

    # 把窗口移到固定位置，保证截图稳定
    user32.SetWindowPos(hwnd, 0, 40, 40, 0, 0, 0x0001 | 0x0004)  # SWP_NOSIZE|SWP_NOZORDER
    time.sleep(0.5)

    # ---- 截图 1：第1关 8bit 加密 + 解密回环 ----
    SetControlText(hwnd, IDC_PLAIN_BITS, "10101010")
    SetControlText(hwnd, IDC_KEY_BITS, "1010000010")
    time.sleep(0.2)
    ClickButton(hwnd, IDC_BTN_ENCRYPT)     # 加密 → 密文框出 10001101
    time.sleep(0.3)
    ClickButton(hwnd, IDC_BTN_DECRYPT)     # 解密回环 → 明文框还原
    time.sleep(0.5)
    CaptureWindow(hwnd, os.path.join(OUT, "第1关_GUI.png"))

    # ---- 截图 2：第3关 字符串加密 + 解密 ----
    SetControlText(hwnd, IDC_KEY_BITS, "0111110101")
    SetControlText(hwnd, IDC_PLAIN_STRING, "Hi你好World123")
    time.sleep(0.2)
    ClickButton(hwnd, IDC_BTN_ENC_STR)     # 加密字符串 → 十六进制密文
    time.sleep(0.3)
    ClickButton(hwnd, IDC_BTN_DEC_STR)     # 解密密文 → 还原明文
    time.sleep(0.5)
    CaptureWindow(hwnd, os.path.join(OUT, "第3关_GUI字符串.png"))

    # ---- 截图 3：第4关 暴力破解 ----
    SetControlText(hwnd, IDC_PLAIN_BITS_BF, "10101010")
    SetControlText(hwnd, IDC_CIPHER_BITS_BF, "01110100")
    time.sleep(0.2)
    ClickButton(hwnd, IDC_BTN_BRUTE)       # 遍历 1024 密钥查找
    time.sleep(0.8)
    CaptureWindow(hwnd, os.path.join(OUT, "第4关_GUI暴力破解.png"))

    proc.terminate()
    print("ALL DONE")


if __name__ == "__main__":
    Main()
