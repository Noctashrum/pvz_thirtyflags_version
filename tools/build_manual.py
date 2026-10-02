# 手动构建：重编过期/改动的 cpp，然后链接出 exe（绕过被禁的 MSBuild）
#
# 两种构建模式：
#   默认            —— 调试版：保留全部日志/HUD（PvzDebug + TFLog + 调试 HUD）
#                     输出 Release\PlantsVsZombies.exe
#   --player        —— 游玩版：定义 TF_PLAYER_BUILD + PVZ_DEBUG_ENABLED=0，
#                     关闭文件日志（thirtyflags_flow.log / thirtyflags.log /
#                     pvzdebug.log）、左上角调试 HUD、TfLogWrite 阶段日志
#                     输出 Release\PlantsVsZombies_Play.exe（与调试版共享同目录游戏资源）
#   两模式的 obj 分开存放（Release / ReleasePlayer），因为编译宏不同不可混链。
import os, glob, subprocess, sys, re, shutil

PLAYER = '--player' in sys.argv

SRC = r'D:\dsh-project\LawnProject\src'
OBJDIR = os.path.join(SRC, 'Lawn', 'ReleasePlayer' if PLAYER else 'Release')
FWDIR = os.path.join(SRC, 'SexyAppFramework', 'Release')
OUTEXE = (r'D:\dsh-project\LawnProject\Release\PlantsVsZombies_Play.exe' if PLAYER
          else r'D:\dsh-project\LawnProject\Release\PlantsVsZombies.exe')
MSVC = r'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207'
SDKU = r'C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0'
SDKBASE = r'C:\Program Files (x86)\Windows Kits\10'
SDKL = SDKBASE
DX8 = os.path.join(SRC, 'dx8sdk')

env = dict(os.environ)
env['INCLUDE'] = ';'.join([
    MSVC + r'\include',
    SDKU + r'\ucrt', SDKU + r'\shared', SDKU + r'\um',
    DX8 + r'\include',          # 必须放末尾（basetsd.h 遮蔽问题）
])
env['LIB'] = ';'.join([
    MSVC + r'\lib\x86',
    SDKL + r'\Lib\10.0.26100.0\ucrt\x86',
    SDKL + r'\Lib\10.0.26100.0\um\x86',
    DX8 + r'\lib',
])

CL = os.path.join(MSVC, 'bin', 'Hostx64', 'x86', 'cl.exe')
LINK = os.path.join(MSVC, 'bin', 'Hostx64', 'x86', 'link.exe')
BASEFLAGS = ['-c', '-MT', '-O2', '-D_USE_WIDE_STRING', '-DWIN32', '-DNDEBUG', '-D_CONSOLE',
             '-D_CRT_SECURE_NO_WARNINGS']
if PLAYER:
    # 游玩版：关闭所有调试输出（详见 Lawn/ThirtyFlags.h 顶部的构建开关说明）
    BASEFLAGS += ['-DTF_PLAYER_BUILD', '-DPVZ_DEBUG_ENABLED=0']
    os.makedirs(OBJDIR, exist_ok=True)
    # 资源脚本产物（图标/清单段）从调试版 obj 目录复制过来
    for r in glob.glob(os.path.join(SRC, 'Lawn', 'Release', '*.res')):
        dst = os.path.join(OBJDIR, os.path.basename(r))
        if (not os.path.exists(dst)) or os.path.getmtime(r) > os.path.getmtime(dst):
            shutil.copy2(r, dst)
            print('复制资源脚本:', os.path.basename(r))
print('构建模式:', '游玩版 (PLAYER)' if PLAYER else '调试版 (DEBUG, 带日志)')
# 注意：dx8sdk\include 只放 INCLUDE 环境变量的末尾，绝不能加 -I（-I 优先级更高，
# 会让 1998 年的 basetsd.h 遮蔽系统头的 POINTER_64，导致 winnt.h 大片假错）

# ---- 1. 收集工程源文件（从 vcxproj 的 ClCompile）----
proj = open(os.path.join(SRC, 'Lawn', 'Lawn.vcxproj'), encoding='utf-8').read()
cpps = re.findall(r'<ClCompile Include="([^"]+)"', proj)
cpps = [os.path.normpath(os.path.join(SRC, 'Lawn', c)) for c in cpps]
print('工程源文件数:', len(cpps))

# ---- 2. 找出过期的 obj（obj 不存在或 cpp 更新，或任一头文件比 obj 新）----
headers = glob.glob(os.path.join(SRC, 'Lawn', '*.h')) + glob.glob(os.path.join(SRC, 'TodLib', '*.h'))
newest_header = max(os.path.getmtime(h) for h in headers)
stale = []
for c in cpps:
    obj = os.path.join(OBJDIR, os.path.splitext(os.path.basename(c))[0] + '.obj')
    if (not os.path.exists(obj)) or os.path.getmtime(c) > os.path.getmtime(obj) \
       or newest_header > os.path.getmtime(obj):
        stale.append((c, obj))
print('需重编:', len(stale))
for c, o in stale:
    print('  ', os.path.basename(c))

# ---- 3. 重编 ----
failed = []
for c, o in stale:
    r = subprocess.run([CL] + BASEFLAGS + ['-Fo' + o, c], cwd=SRC, env=env,
                       capture_output=True, text=True, errors='replace')
    errs = [l for l in (r.stdout or '').splitlines() if 'error C' in l or 'error LNK' in l]
    if r.returncode != 0 or errs:
        failed.append((os.path.basename(c), errs[:5]))
        print('  FAIL', os.path.basename(c))
        for e in errs[:5]:
            print('     ', e)
    else:
        print('  ok  ', os.path.basename(c))
if failed:
    print('编译失败，终止'); sys.exit(1)

# ---- 4. 链接 ----
objs = sorted(glob.glob(os.path.join(OBJDIR, '*.obj'))) + sorted(glob.glob(os.path.join(FWDIR, '*.obj')))
res = glob.glob(os.path.join(OBJDIR, '*.res'))
libs = ['kernel32.lib', 'user32.lib', 'gdi32.lib', 'winspool.lib', 'comdlg32.lib', 'advapi32.lib',
        'shell32.lib', 'ole32.lib', 'oleaut32.lib', 'uuid.lib', 'odbc32.lib', 'odbccp32.lib',
        'Winmm.lib', 'Ws2_32.lib', 'd3d8.lib', 'd3dx8.lib', 'dxguid.lib', 'dsound.lib',
        'dinput8.lib', 'DSETUP.lib']
cmd = [LINK, '/NOLOGO', '/MACHINE:X86', '/SUBSYSTEM:WINDOWS',
       '-OUT:' + OUTEXE] + objs + res + libs
print('链接: %d obj + %d res' % (len(objs), len(res)))
r = subprocess.run(cmd, cwd=SRC, env=env, capture_output=True, text=True, errors='replace')
out = (r.stdout or '') + (r.stderr or '')
errlines = [l for l in out.splitlines() if 'error' in l.lower()]
print('链接退出码:', r.returncode)
for l in errlines[:15]:
    print('  ', l)
if r.returncode == 0 and os.path.exists(OUTEXE):
    print('SUCCESS:', OUTEXE, os.path.getsize(OUTEXE), 'bytes')
else:
    sys.exit(1)
