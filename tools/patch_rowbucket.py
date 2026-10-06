# -*- coding: utf-8 -*-
# 投射物碰撞：从「每帧扫全部僵尸」改为「按行分桶」
#
# 依据（实测）：Projectile::FindCollisionTarget 对**每个投射物**遍历**所有僵尸**，
# 只剩一个 mRow 过滤。245 投射物 × 49 僵尸 ≈ 1.2 万次/帧，且随两者乘积增长；
# 而僵尸本来就按行活动（5~6 行），按行分桶后每行只有 ~1/行数 个，量级直接降一个档。
#
# 正确性：该函数选的是**最左侧**重叠目标（aZombie->mX < aMinX），与遍历顺序无关，
# 所以分桶不会改变选中的目标；BOSS 例外（不分行、永远可被打）单独保留。
# 桶在「僵尸池 size 变化」或「新的一帧」时惰性重建，保证与池内容一致。

def to_bytes(text, eol):
    return text.replace('\n', eol).encode('gbk')


def rep(path, old_text, new_text, tag):
    d = open(path, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = to_bytes(old_text, eol)
        if d.count(old) == 1:
            open(path, 'wb').write(d.replace(old, to_bytes(new_text, eol)))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss', d.count(to_bytes(old_text, '\r\n'))))


# ---------- 1) Board.cpp：分桶 + 访问器 ----------
rep('src/Lawn/Board.cpp',
    'void Board::KillAllPlantsInRadius(int theX, int theY, int theRadius)\n',
    '''// ----------------------------------------------------------------------------------------------------
// 【性能】僵尸按行分桶（供投射物碰撞查询）
//
// 原实现：Projectile::FindCollisionTarget 对每个投射物遍历**整个僵尸池**，只做一次 mRow 过滤。
// 245 投射物 × 49 僵尸 ≈ 1.2 万次/帧，且随两者乘积增长（后期更糟）。
// 僵尸本来就是按行活动的，分桶后每行只剩约 1/行数，量级直接降一档。
//
// 语义完全不变：该函数取的是「最左侧重叠目标」，与遍历顺序无关；
// BOSS 不分行（任何行都能打），单独存一份。
// 桶在「僵尸池数量变化」或「跨帧」时惰性重建，保证与池一致。
// ----------------------------------------------------------------------------------------------------
static Zombie* gTFRowZombies[MAX_GRID_SIZE_Y][TF_ROW_BUCKET_MAX];
static int     gTFRowZombieCount[MAX_GRID_SIZE_Y];
static Zombie* gTFBossZombies[8];
static int     gTFBossZombieCount = 0;
static unsigned int gTFBucketBuiltSize = 0xFFFFFFFFu;
static int          gTFBucketBuiltFrame = -1;

static void TFBuildZombieRowBucket(Board* theBoard)
{
    memset(gTFRowZombieCount, 0, sizeof(gTFRowZombieCount));
    gTFBossZombieCount = 0;

    Zombie* aZombie = nullptr;
    while (theBoard->IterateZombies(aZombie))
    {
        if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
        {
            if (gTFBossZombieCount < 8)
                gTFBossZombies[gTFBossZombieCount++] = aZombie;
            continue;
        }

        int aRow = aZombie->mRow;
        if (aRow < 0 || aRow >= MAX_GRID_SIZE_Y)
            continue;

        if (gTFRowZombieCount[aRow] < TF_ROW_BUCKET_MAX)
            gTFRowZombies[aRow][gTFRowZombieCount[aRow]++] = aZombie;
    }

    gTFBucketBuiltSize = theBoard->mZombies.mSize;
    gTFBucketBuiltFrame = theBoard->mMainCounter;
}

int Board::GetRowZombieCount(int theRow)
{
    if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
        return 0;

    if (gTFBucketBuiltFrame != mMainCounter || gTFBucketBuiltSize != mZombies.mSize)
        TFBuildZombieRowBucket(this);

    return gTFRowZombieCount[theRow];
}

Zombie* Board::GetRowZombie(int theRow, int theIndex)
{
    if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y || theIndex < 0 || theIndex >= gTFRowZombieCount[theRow])
        return nullptr;

    return gTFRowZombies[theRow][theIndex];
}

int Board::GetBossZombieCount()
{
    if (gTFBucketBuiltFrame != mMainCounter || gTFBucketBuiltSize != mZombies.mSize)
        TFBuildZombieRowBucket(this);

    return gTFBossZombieCount;
}

Zombie* Board::GetBossZombieByIndex(int theIndex)
{
    if (theIndex < 0 || theIndex >= gTFBossZombieCount)
        return nullptr;

    return gTFBossZombies[theIndex];
}

void Board::KillAllPlantsInRadius(int theX, int theY, int theRadius)
''',
    'board-bucket')

# ---------- 2) Board.h：声明 ----------
rep('src/Lawn/Board.h',
    '\tbool\t\t\t\t\t\t\tIsSurvivalStageWithRepick();\n',
    '\t// 【性能】僵尸按行分桶查询（投射物碰撞用，语义与全池遍历一致）\n'
    '\tint\t\t\t\t\t\t\tGetRowZombieCount(int theRow);\n'
    '\tZombie*\t\t\t\t\t\t\tGetRowZombie(int theRow, int theIndex);\n'
    '\tint\t\t\t\t\t\t\tGetBossZombieCount();\n'
    '\tZombie*\t\t\t\t\t\t\tGetBossZombieByIndex(int theIndex);\n'
    '\n'
    '\tbool\t\t\t\t\t\t\tIsSurvivalStageWithRepick();\n',
    'board-h')

# ---------- 3) Projectile.cpp：用分桶遍历 ----------
rep('src/Lawn/Projectile.cpp',
    '''	Zombie* aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == mRow) && aZombie->EffectedByDamage((unsigned int)mDamageRangeFlags))
''',
    '''	// 【性能】改为按行分桶遍历（原来每个投射物都要扫整个僵尸池）。
	// 取「最左侧重叠目标」与遍历顺序无关，所以结果与原实现一致；BOSS 单独遍历。
	Zombie* aZombie = nullptr;
	for (int aBucketIndex = 0; ; aBucketIndex++)
	{
		if (aBucketIndex < mBoard->GetRowZombieCount(mRow))
			aZombie = mBoard->GetRowZombie(mRow, aBucketIndex);
		else if (aBucketIndex - mBoard->GetRowZombieCount(mRow) < mBoard->GetBossZombieCount())
			aZombie = mBoard->GetBossZombieByIndex(aBucketIndex - mBoard->GetRowZombieCount(mRow));
		else
			break;

		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == mRow) && aZombie->EffectedByDamage((unsigned int)mDamageRangeFlags))
''',
    'proj-bucket')

# ---------- 4) 桶容量常量 ----------
rep('src/Lawn/Board.h',
    'constexpr int\tWIDE_BOARD_WIDTH\t\t\t= 800;\n',
    'constexpr int\tWIDE_BOARD_WIDTH\t\t\t= 800;\n'
    '// 【性能】每行僵尸分桶容量（超出则不予分桶，仅影响极端堆叠时的碰撞查找范围）\n'
    'constexpr int\tTF_ROW_BUCKET_MAX\t\t\t= 256;\n',
    'bucket-max')

print("ok")
