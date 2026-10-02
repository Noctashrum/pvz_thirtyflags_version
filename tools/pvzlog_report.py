#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
汇总 Release/pvzdebug.log，输出一份可快速判断的报告。

用法:
    python tools/pvzlog_report.py                    # 读 Release/pvzdebug.log
    python tools/pvzlog_report.py <日志文件>          # 指定文件

报告内容:
  * 会话概况 / 崩溃 / 断言总数
  * 断言按"文件:行号 + 条件"聚合的 Top N
  * 实体池用量峰值（来自崩溃时的 game state 转储）
  * DataArray 溢出告警
  * 各通道消息数

报告同时写到 <日志文件>.report.txt（UTF-8）。
"""
import os
import re
import sys
from collections import Counter

try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

DEFAULT_LOG = os.path.join('Release', 'pvzdebug.log')
TOP_N = 25


def main(argv):
    path = argv[1] if len(argv) > 1 else DEFAULT_LOG
    if not os.path.exists(path):
        print('找不到日志文件: %s' % path)
        return 1

    sessions = 0
    crashes = []
    asserts = Counter()          # (file, line, cond) -> count
    assert_total = 0
    levels = Counter()
    channels = Counter()
    pool_full = Counter()
    pool_peak = {}               # name -> (size, max)
    error_lines = []
    warn_lines = []
    repeated = 0

    # 断言块形如:
    #   HH:MM:SS.mmm [E] [mem ] 123 ASSERT file.cpp(LINE)
    #         condition: xxx
    #         detail:    yyy
    #         (total asserts: N[, REPEATED])
    re_assert = re.compile(r'ASSERT\s+([^\(]+)\((\d+)\)')
    re_cond = re.compile(r'condition:\s*(.*)')
    re_total = re.compile(r'total asserts:\s*(\d+)(,\s*REPEATED)?')
    # 通道标签是定宽的 4 字符，可能带尾空格（如 "[mem ]"），所以不能用 \w+
    re_level = re.compile(r'\[([EWIV])\]\s*\[([^\]]+)\]')
    re_full = re.compile(r"DataArray FULL: '([^']+)'")
    re_pools = re.compile(r'(\w+)=(\d+)/(\d+)')
    re_crash = re.compile(r'UNHANDLED EXCEPTION\s+(0x[0-9A-Fa-f]+)\s*\(([^)]*)\)')
    re_fault = re.compile(r'fault address:\s*(\S+)')

    pending_file = None
    pending_line = None

    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for raw in f:
            line = raw.rstrip('\n')

            if 'PvzDebug session start' in line:
                sessions += 1
                continue

            m = re_crash.search(line)
            if m:
                crashes.append({'code': m.group(1), 'name': m.group(2), 'addr': None})
                error_lines.append(line)
                continue
            if crashes and crashes[-1].get('addr') is None:
                m = re_fault.search(line)
                if m:
                    crashes[-1]['addr'] = m.group(1)
                    continue

            m = re_level.search(line)
            if m:
                levels[m.group(1)] += 1
                channels[m.group(2)] += 1

            m = re_full.search(line)
            if m:
                pool_full[m.group(1)] += 1

            if 'pools:' in line:
                for name, size, mx in re_pools.findall(line):
                    size, mx = int(size), int(mx)
                    if name not in pool_peak or size > pool_peak[name][0]:
                        pool_peak[name] = (size, mx)

            m = re_assert.search(line)
            if m:
                pending_file, pending_line = m.group(1).strip(), m.group(2)
                continue

            if pending_file is not None:
                m = re_cond.search(line)
                if m:
                    cond = m.group(1).strip()
                    asserts[(pending_file, pending_line, cond)] += 1
                    assert_total += 1
                    pending_file = pending_line = None
                    continue

            m = re_total.search(line)
            if m and m.group(2):
                repeated += 1

            if line.startswith('!!!') or ' minidump ' in line:
                error_lines.append(line)

    out = []
    w = out.append

    w('=' * 78)
    w('PvzDebug 日志报告: %s' % path)
    w('=' * 78)
    w('')
    w('会话数        : %d' % sessions)
    w('崩溃/未处理异常: %d' % len(crashes))
    for c in crashes:
        w('   - %s (%s) @ %s' % (c['code'], c['name'], c['addr'] or '?'))
    w('')
    w('断言触发总数  : %d   (其中被节流折叠的重复条目: %d)' % (assert_total, repeated))
    w('消息级别分布  : ' + ', '.join('%s=%d' % (k, v) for k, v in sorted(levels.items())))
    w('')

    if pool_peak:
        w('--- 实体池用量峰值 ---')
        for name in sorted(pool_peak, key=lambda n: -pool_peak[n][0] / max(pool_peak[n][1], 1)):
            size, mx = pool_peak[name]
            pct = 100.0 * size / mx if mx else 0.0
            flag = '  <== 接近/已满!' if pct >= 90 else ''
            w('   %-14s %5d / %-5d  (%5.1f%%)%s' % (name, size, mx, pct, flag))
        w('')

    if pool_full:
        w('--- DataArray 溢出（拿到 sink，实体被丢弃） ---')
        for name, n in pool_full.most_common():
            w('   %-14s %d 次' % (name, n))
        w('')

    if asserts:
        w('--- 断言热点 Top %d ---' % TOP_N)
        for (fname, lineno, cond), n in asserts.most_common(TOP_N):
            w('   %6d 次  %s:%s' % (n, fname, lineno))
            w('             %s' % cond[:120])
        w('')

    if error_lines:
        w('--- 崩溃相关行 ---')
        for l in error_lines[:20]:
            w('   %s' % l[:150])
        w('')

    w('=' * 78)
    w('提示: 断言默认 action=continue，所以这些不变量一直是失败的，')
    w('      只是 Release 下没人记录。优先看"断言热点"和"池用量峰值"。')
    w('=' * 78)

    text = '\n'.join(out)
    print(text)

    rep = path + '.report.txt'
    with open(rep, 'w', encoding='utf-8') as f:
        f.write(text + '\n')
    print('\n报告已写入: %s' % rep)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
