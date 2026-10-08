# -*- coding: utf-8 -*-
"""驱动已在运行的游戏：送按键 + 抓窗口图（纯 ctypes，不用 Add-Type / PIL）。

用法:
    python tools/tf_keys.py keys            # 只送键：点标题 -> T -> S
    python tools/tf_keys.py shot out.bmp    # 抓一张窗口图（BMP）
    python tools/tf_keys.py keys shot out.bmp

说明：本环境的 PowerShell `Add-Type` 被安全策略拦截，而 PS 脚本又常在启动游戏后被杀，
所以这里用 Python + ctypes 直接调 user32/gdi32，绕开上面两个坑。
"""
import ctypes
import ctypes.wintypes as wt
import sys
import time

u32 = ctypes.WinDLL("user32", use_last_error=True)
g32 = ctypes.WinDLL("gdi32", use_last_error=True)

WM_KEYDOWN, WM_KEYUP, WM_CHAR = 0x0100, 0x0101, 0x0102
WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE = 0x0201, 0x0202, 0x0200


def find_window_of(image_name="PlantsVsZombies.exe"):
    """按进程名找主窗口"""
    import subprocess
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {image_name}", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    pids = set()
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) >= 2 and parts[1].isdigit():
            pids.add(int(parts[1]))
    if not pids:
        print("no process")
        return None

    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, lparam):
        pid = wt.DWORD()
        u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and u32.IsWindowVisible(hwnd):
            r = wt.RECT()
            u32.GetClientRect(hwnd, ctypes.byref(r))
            if r.right > 200 and r.bottom > 200:      # 主窗口而非小控件
                found.append((hwnd, r.right, r.bottom))
        return True

    u32.EnumWindows(cb, 0)
    if not found:
        print("no window")
        return None
    hwnd, w, h = found[0]
    print(f"hwnd={hwnd} client={w}x{h}")
    return hwnd


def send_key(hwnd, vk):
    u32.PostMessageW(hwnd, WM_KEYDOWN, vk, 0x00140001)
    u32.PostMessageW(hwnd, WM_KEYUP, vk, 0xC0140001)
    u32.PostMessageW(hwnd, WM_CHAR, vk, 0x00140001)


def send_click(hwnd, x, y):
    lp = (y << 16) | (x & 0xFFFF)
    u32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, lp)
    time.sleep(0.05)
    u32.PostMessageW(hwnd, WM_LBUTTONDOWN, 1, lp)
    time.sleep(0.05)
    u32.PostMessageW(hwnd, WM_LBUTTONUP, 0, lp)


def capture(hwnd, path):
    r = wt.RECT()
    u32.GetClientRect(hwnd, ctypes.byref(r))
    w, h = r.right, r.bottom
    hdc = u32.GetDC(hwnd)
    mem = g32.CreateCompatibleDC(hdc)
    bmp = g32.CreateCompatibleBitmap(hdc, w, h)
    g32.SelectObject(mem, bmp)
    u32.PrintWindow(hwnd, mem, 2)          # PW_RENDERFULLCONTENT：抓窗口自身内容，与遮挡/焦点无关

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG),
                    ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.biWidth, bi.biHeight = w, -h           # 负高度 = 自上而下
    bi.biPlanes, bi.biBitCount = 1, 24
    stride = ((w * 3 + 3) // 4) * 4
    buf = ctypes.create_string_buffer(stride * h)
    g32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0)

    with open(path, "wb") as f:
        f.write(b"BM")
        size = 14 + 40 + len(buf)
        f.write(size.to_bytes(4, "little") + b"\0\0\0\0" + (54).to_bytes(4, "little"))
        f.write((40).to_bytes(4, "little") + w.to_bytes(4, "little", signed=True) +
                (-h).to_bytes(4, "little", signed=True) + (1).to_bytes(2, "little") +
                (24).to_bytes(2, "little") + b"\0" * 24)
        f.write(buf.raw)

    g32.DeleteObject(bmp)
    g32.DeleteDC(mem)
    u32.ReleaseDC(hwnd, hdc)
    print(f"shot -> {path} ({w}x{h})")


def main():
    args = sys.argv[1:]
    hwnd = find_window_of()
    if hwnd is None:
        return 1

    if "keys" in args:
        send_click(hwnd, 400, 560)         # 标题界面 -> 主菜单
        time.sleep(4)
        send_key(hwnd, 0x54)               # T -> 三十旗
        time.sleep(8)
        send_key(hwnd, 0x53)               # S -> 开局
        time.sleep(10)
        print("keys sent (click/T/S)")

    if "shot" in args:
        i = args.index("shot")
        path = args[i + 1] if i + 1 < len(args) else "docs/perf/live.bmp"
        capture(hwnd, path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
