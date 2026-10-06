# -*- coding: utf-8 -*-
# 一键切换精灵合批开关：python tools/flip_batch.py 1|0
import sys

val = sys.argv[1] if len(sys.argv) > 1 else '1'
p = 'src/SexyAppFramework/D3DInterface.cpp'
d = open(p, 'rb').read()

olds = [b'#define TF_SPRITE_BATCH 1\r\n', b'#define TF_SPRITE_BATCH 0\r\n']
cur = None
for o in olds:
    if d.count(o) == 1:
        cur = o
        break
assert cur is not None, 'TF_SPRITE_BATCH define not found'

new = ('#define TF_SPRITE_BATCH %s\r\n' % val).encode('gbk')
if cur == new:
    print('already', val)
else:
    d = d.replace(cur, new)
    open(p, 'wb').write(d)
    print('TF_SPRITE_BATCH ->', val)
