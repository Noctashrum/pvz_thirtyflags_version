#include "ThirtyFlags.h"
#include "Board.h"
#include "Challenge.h"
#include "GridItem.h"
#include "LawnApp.h"
#include "LawnDialog.h"
#include "Plant.h"
#include "Projectile.h"
#include "LawnMower.h"
#include <time.h>
#include "PoolEffect.h"
#include "Resources.h"
#include "../SexyAppFramework/Common.h"
#include "../SexyAppFramework/Graphics.h"
#include "../TodLib/EffectSystem.h"
static int  gTFDeadCol[64], gTFDeadRow[64], gTFDeadSeed[64];   // NIRVANA: plants lost last flag
static int  gTFDeadCount = 0;

// -------------------------------------------------------------------------------------------
// [ThirtyFlags] run save -- full design notes in section "RUN SAVE" at the bottom of this file.
//   Board state (grid / plants / sun / mowers / wave table / Challenge::mSurvivalStage) is
//   stored by the ORIGINAL pipeline:
//     Board::TryToSaveGame() -> LawnSaveGame() -> userdata/game<mode>_<profile>.dat
//     LawnApp::PreNewGame(mode,true) -> TryLoadGame() -> Board::LoadGame()
//   The roguelite layer (upgrade stacks / mutations / intermission) lives outside Board on
//   purpose -- Board's layout is frozen because SyncBoard writes it as one raw block -- and is
//   appended to the save as a small sidecar "<savefile>.tf".
// -------------------------------------------------------------------------------------------
constexpr int TF_SAVE_MAGIC = 0x7F01;


#include <math.h>
#include <string.h>

using namespace Sexy;

// ====================================================================================================
// ¡¶Ö²Îï´óÕ½½©Ê¬£ºÈıÊ®Æì¡·ÊµÏÖ
//
// ³¡µØ²ÉÓÃ¡¸ÈÕ¼ä²¼¾Ö¡¹£¨5 ĞĞ x 100px£©+ ¾Ö²¿Ë®Â·£º
//   * ÈÕ¼ä²¼¾Ö²ÅÄÜ¸´ÓÃÔ­°æµÄ²İÆ¤¹ö¶¯¶¯»­£¨IMAGE_BACKGROUND1UNSODDED + IMAGE_SOD1ROW£©£»
//   * Ë®Â·²»ÔÙÊÇÕûÕÅÓ¾³Ø±³¾°£¬¶øÊÇ°´¸ñ´Ó IMAGE_POOL È¡Ë®¸ñ»æÖÆ£¨¸ÃÍ¼ÊÇ 15x5 µÄË®¸ñÕóÁĞ£©£»
//   * ĞĞ½âËøÊ±²¥·Å²İÆ¤¹ö³ö¶¯»­£»Î´¿ª·ÅµÄĞĞ»æÖÆºÚÄ»£¬ĞÎ³ÉÔ­°æ×ó²à²İÆºÈ±¿ÚÄÇÖÖ¹Û¸Ğ¡£
//
// ×¢Òâ£ºGridToPixelY µÄĞĞ¸ßÓÉ StageHasPool() È«¾Ö¶şÑ¡Ò»£¨ÈÕ¼ä 100px / Ó¾³Ø 85px£©£¬
// ÎŞ·¨ĞĞ¼¶»ìÓÃ¡£ÈÕ¼ä²¼¾ÖÏÂµÚ 6 ĞĞ y=580 ÒÑÔ½½ç£¬Òò´ËÈıÊ®Æì°´ 5 ĞĞÉè¼Æ¡£
// ====================================================================================================

ThirtyFlags gThirtyFlags;

static inline int TFClampI(int theValue, int theMin, int theMax)
{
    return theValue <= theMin ? theMin : (theValue >= theMax ? theMax : theValue);
}

bool ThirtyFlagsMode()
{
    return gThirtyFlags.mActive && gLawnApp && gLawnApp->mGameMode == GAMEMODE_THIRTY_FLAGS;
}

// ----------------------------------------------------------------------------------------------------
// 30 Æì²¨´Î±í£¨²ß»®°¸µÚ 2 / 4 ÕÂ£¬°´ÈÕ¼ä 5 ĞĞ²¼¾ÖÖØĞÂ±àÅÅ£©
//
// mUnlockedRows / mWaterRows Îª 5 Î»ÑÚÂë£¨bit0=µÚ 1 ĞĞ ¡­ bit4=µÚ 5 ĞĞ£©¡£
// ½âËø½Ú×à£º¿ª¾ÖÖ»¸øÖĞ¼ä 1 µÀ£¨Õæ 1-1 ¹Û¸Ğ£©¡ú Öğ²½À©µ½ 4 µÀ ¡ú Á½¶Ë×ªË®Â·¡£
// ----------------------------------------------------------------------------------------------------
#define TF_R2       (1 << 2)
#define TF_R1_3     ((1 << 1) | (1 << 3) | TF_R2)
#define TF_R0_4     ((1 << 0) | (1 << 4) | TF_R1_3)

// ÈÕ¼ä²¼¾ÖÖ»ÓĞ 5 ĞĞ¿ÉÓÃ£¨µÚ 6 ĞĞ y=580 ÒÑ³ö»­Ãæ£©£¬Òò´Ë°´ 5 ÌõµÀ±àÅÅ£º
//   ÖĞÂ· 1 µÀ¿ª¾Ö -> ÖğÆìÀ©µ½ 4 µÀÂ½µØ -> ÔÙÈÃÁ½¶Ë×ªË®Â·¡£
// Ë®Ãæ°´¡¸ÕûĞĞÂúË®¡¹»æÖÆ£¨²»À­ÉìÌùÍ¼£©£¬Òò´Ë mWaterCols ½ö×÷¡¸¸ÃĞĞÊÇ·ñÎªË®¡¹µÄ¿ª¹Ø£¬
// È¡ÖµÎª MAX_GRID_SIZE_X ±íÊ¾ÕûĞĞ½ÔË®¡£
static const ThirtyFlagsFlagDef gThirtyFlagsFlagDefs[TF_TOTAL_FLAGS] = {
    // Æì´Î  Ö÷Ìâ            µãÊı  Ë®Â·ĞĞ        Ë®ÁĞ      ÎíÁĞ  ¿ª·ÅĞĞ
    {  1,   "¶ÀÄ¾ÄÑÖ§",      1,    0,            0,        0,    TF_R2 },
    {  2,   "¶ÀÄ¾ÄÑÖ§",      1,    0,            0,        0,    TF_R2 },
    {  3,   "¶ÀÄ¾ÄÑÖ§",      1,    0,            0,        0,    TF_R2 },
    {  4,   "³¡µØ´ò¿ª",      2,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  5,   "³¡µØ´ò¿ª",      2,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  6,   "³¡µØ´ò¿ª",      3,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  7,   "³¡µØ´ò¿ª",      3,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  8,   "ÈıÏß³ÉĞÎ",      4,    0,            0,        0,    TF_R1_3 },
    {  9,   "ÈıÏß³ÉĞÎ",      4,    0,            0,        0,    TF_R1_3 },
    { 10,   "ÈıÏß³ÉĞÎ",      5,    0,            0,        0,    TF_R1_3 },
    { 11,   "ÈıÏß³ÉĞÎ",      5,    0,            0,        0,    TF_R1_3 },
    { 12,   "ÈıÏß³ÉĞÎ",      6,    0,            0,        0,    TF_R1_3 },
    { 13,   "ËÄÏßÆÌ¿ª",      6,    0,            0,        0,    (1 << 0) | TF_R1_3 },
    { 14,   "ËÄÏßÆÌ¿ª",      7,    0,            0,        0,    (1 << 0) | TF_R1_3 },
    { 15,   "ËÄÏßÆÌ¿ª",      7,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 16,   "ËÄÏßÆÌ¿ª",      8,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 17,   "ËÄÏßÆÌ¿ª",      8,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 18,   "Ë®Â·µÇ³¡",      9,    (1 << 4),      MAX_GRID_SIZE_X, 3,  TF_R0_4 },
    { 19,   "Ë®Â·µÇ³¡",      9,    (1 << 4),      MAX_GRID_SIZE_X, 3,  TF_R0_4 },
    { 20,   "Ë«ÏòË®Â·",      10,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 21,   "Ë«ÏòË®Â·",      11,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 22,   "Ë«ÏòË®Â·",      12,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 23,   "Ë«ÏòË®Â·",      13,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 24,   "Ë«ÏòË®Â·",      14,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 25,   "Ë«·½³¬Ä£",      15,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 26,   "Ë«·½³¬Ä£",      17,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 27,   "Ë«·½³¬Ä£",      19,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 28,   "Ë«·½³¬Ä£",      22,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 29,   "Ë«·½³¬Ä£",      25,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 30,   "½©ÍõÖÕÕ½",      0,    (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 }
};

// ----------------------------------------------------------------------------------------------------
// Ã¿ĞĞ²İÆ¤¹ö³öµÄ¶¯»­½ø¶È£¨0..1000£©¡£ĞĞ½âËøÊ±ÖÃ 0 ²¢ÖğÖ¡ÍÆ½øµ½ 1000¡£
// ----------------------------------------------------------------------------------------------------
static int gThirtyFlagsSodProgress[MAX_GRID_SIZE_Y];
static int gThirtyFlagsPrevUnlockedRows = 0;

// ¡¾ÈıÊ®Æì¡¿ÄÄĞ©ĞĞ¡¸ÒÑ¾­·¢¹ıĞ¡ÍÆ³µ¡¹¡£
// ÎªÊ²Ã´ÒªÎ»Í¼¶ø²»ÊÇ¡¸Ã¿Ö¡ÃÜµÈ²¹Æë¡¹£º
// Ã¿Ö¡ÃÜµÈ²¹Æë»áÈÃÍæ¼Ò¶ªµÄ³µÔÚÏÂÒ»Ö¡Æ¾¿Õ¸´»î£¬
// ¶øÎ»Í¼ÄÜÇø·Ö¡¸»¹Ã»·¢¡¹ºÍ¡¸ÒÑ¾­ÓÃµôÁË¡¹¡£
// Î»Í¼Ëæ .tf ¸½¼Ó´æµµÂäÅÌ£¬ËùÒÔ¶Á¾É´æµµÊ±Î»Í¼Îª 0£¬
// »á°Ñ¾ÉµµÀïÈ±Ê§µÄ³µ²¹»ØÀ´¡£
static int gThirtyFlagsMowerGranted = 0;

// Ã¿¸ñÊÇ·ñÒÑ±ä³ÉË®£¨ÓÃÓÚË®¸ñµ­Èë£¬±ÜÃâ»»ÆìÊ±Ë®Í»È»³öÏÖ£©
static int gThirtyFlagsWaterFade[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];

// ----------------------------------------------------------------------------------------------------
// µ±Ç°Æì´Î
//   mSurvivalStage È«³ÌµÈÓÚ¡¸ÒÑÍê³ÉÆìÊı¡¹£¬ÈÎºÎµ÷ÓÃÊ±»ú¶¼³ÉÁ¢£»
//   ²»ÄÜÖ»ÒÀÀµ mFlag£¨Board::InitSurvivalStage »áÔÚ½×¶Î×ÔÔöÇ°µ÷ÓÃ InitZombieWaves£©¡£
// ----------------------------------------------------------------------------------------------------
int ThirtyFlagsCurrentFlag(Board* theBoard)
{
    if (!theBoard || !theBoard->mChallenge)
        return 1;

    return TFClampI(theBoard->mChallenge->mSurvivalStage + 1, 1, TF_TOTAL_FLAGS);
}

// ----------------------------------------------------------------------------------------------------
// ½©Ê¬¸¨Öú
// ----------------------------------------------------------------------------------------------------
static inline void TFSetElite(Zombie* theZombie, int theKind)
{
    theZombie->mBossMode |= (1 << theKind);
}

static inline bool TFHasElite(Zombie* theZombie, int theKind)
{
    return (theZombie->mBossMode & (1 << theKind)) != 0;
}

// ¡¾ÈıÊ®Æì¡¿´¦¾öÌØĞ§£¨²ß»®°¸ 7.7£©£º¾«Ó¢ / ¾ŞÈË½©Ê¬ÕóÍöÊ±µÄ±¬·¢Ñİ³ö¡£
// ¶¨ÒåÔÚÎÄ¼şºó¶ÎµÄ±íÏÖ²ãÇø£¬´Ë´¦ÏÈÉùÃ÷£¬¹© ThirtyFlagsZombieKilled µ÷ÓÃ¡£
static void TFSpawnExecution(Board* theBoard, Zombie* theZombie, bool theGiant);

// Ê¬¶¾¶¾Ì¶£ºÃ¿¸ñÊ£ÓàÖ¡Êı
static int gThirtyFlagsPoison[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];

static void TFAddPoison(int theCol, int theRow, int theFrames)
{
    if (theCol < 0 || theCol >= MAX_GRID_SIZE_X || theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
        return;

    gThirtyFlagsPoison[theCol][theRow] = max(gThirtyFlagsPoison[theCol][theRow], theFrames);
}

// ----------------------------------------------------------------------------------------------------
// Èâ¸ëÇ¿»¯¾²Ì¬±í£¨²ß»®°¸ 7.2 / 7.3 / 7.4£¬¹² 36 Ìõ£¬È«²¿¿Éµş¼Ó£©
// ----------------------------------------------------------------------------------------------------
static const ThirtyFlagsUpgradeDef gThirtyFlagsUpgradeDefs[TF_UPG_COUNT] = {
    // ---- ÆÕÍ¨³Ø ----
    { TF_UPG_HASTE,                TF_RARITY_COMMON,    _S("¼±ËÙ"),         _S("È«Ö²Îï¹¥ËÙ +12%") },
    { TF_UPG_BARRAGE,              TF_RARITY_COMMON,    _S("µ¯Ä»"),         _S("ÉäÊÖÀà¶îÍâ·¢Éä 1 ·¢µ¯Íè") },
    { TF_UPG_REINFORCE,            TF_RARITY_COMMON,    _S("¼Ó¹Ì"),         _S("È«Ö²ÎïÉúÃü +35%") },
    { TF_UPG_ARMORPIERCE,          TF_RARITY_COMMON,    _S("´©¼×"),         _S("ÎŞÊÓ 25% »¤¼×") },
    { TF_UPG_MASS_PRODUCE,         TF_RARITY_COMMON,    _S("Á¿²ú"),         _S("ÖÖÖ²·ÑÓÃ -20%") },
    { TF_UPG_ABUNDANCE,            TF_RARITY_COMMON,    _S("·áÈÄ"),         _S("×ÔÈ»Ñô¹âµôÂä¼ä¸ô -25%") },

    // ---- Ï¡ÓĞ³Ø ----
    { TF_UPG_CRIT,                 TF_RARITY_RARE,      _S("±©»÷"),         _S("15% ¸ÅÂÊÔì³É 2 ±¶ÉËº¦") },
    { TF_UPG_COOLDOWN,             TF_RARITY_RARE,      _S("ÀäÈ´"),         _S("È«Ö²ÎïÀäÈ´ -20%") },
    { TF_UPG_SIPHON,               TF_RARITY_RARE,      _S("ºçÎü"),         _S("»÷É±»Ø 3 Ñô¹â") },
    { TF_UPG_THORNS,               TF_RARITY_RARE,      _S("¾£¼¬"),         _S("·´µ¯ 20% ËùÊÜÉËº¦") },
    { TF_UPG_HIGH_YIELD,           TF_RARITY_RARE,      _S("¸ß²ú"),         _S("ÏòÈÕ¿û²ú³ö +60%") },
    { TF_UPG_DRAIN,                TF_RARITY_RARE,      _S("¼³È¡"),         _S("Ö²ÎïÕóÍö·µ»¹ 50% ÖÖÖ²·Ñ") },

    // ---- Ê·Ê«³Ø£ºÁª¶¯Óë×ª»¯ ----
    { TF_UPG_RESONANCE_HASTE,      TF_RARITY_EPIC,      _S("¹²Ãù¡¤¹¥ËÙ"),    _S("Ã¿³ÖÓĞ 1 Ìõ¡¸¼±ËÙ¡¹£¬È«Ö²Îï¹¥ËÙ¶îÍâ +8%") },
    { TF_UPG_RESONANCE_BARRAGE,    TF_RARITY_EPIC,      _S("¹²Ãù¡¤µ¯Ä»"),    _S("Ã¿³ÖÓĞ 1 Ìõ¡¸µ¯Ä»¡¹£¬ÉäÊÖÀàÉËº¦ +15%") },
    { TF_UPG_RESONANCE_WEALTH,     TF_RARITY_EPIC,      _S("¹²Ãù¡¤²Æ¸»"),    _S("Ã¿³ÖÓĞ 1 Ìõ¾­¼ÃÀàÇ¿»¯£¬È«Ö²Îï¹¥»÷Á¦ +10%") },
    { TF_UPG_BLOOD_RAGE,           TF_RARITY_EPIC,      _S("ÑªÅ­×ª»¯"),     _S("È«Ö²Îïµ±Ç°ÉúÃüµÄ 30% ×ª»¯Îª¹¥»÷Á¦") },
    { TF_UPG_COMPOUND_SUN,         TF_RARITY_EPIC,      _S("¸´Àû¡¤Ñô¹â"),    _S("Ã¿Æì½áÊøÊ±£¬µ±Ç°Ñô¹â´¢±¸ x1.15") },
    { TF_UPG_CHAIN,                TF_RARITY_EPIC,      _S("Á¬Ëø"),         _S("±¬Õ¨ÀàÃüÖĞºó´¥·¢Ò»´Î°ë¶î¶ş´Î±¬Õ¨") },
    { TF_UPG_OVERLOAD,             TF_RARITY_EPIC,      _S("ÁÙ½ç¡¤³¬ÔØ"),    _S("¹¥ËÙ¼Ó³ÉÃ¿´ïµ½ +100%£¬ÉäÊÖ×·¼Ó 1 ·¢µ¯Íè") },
    { TF_UPG_SYMBIOSIS_WATER,      TF_RARITY_EPIC,      _S("¹²Éú¡¤Ë®Â·"),    _S("Ë¯Á«ÉÏµÄÖ²Îï¹¥ËÙ +30%£¬ÇÒË¯Á«²»ÔÙÕ¼¿¨²Û") },
    { TF_UPG_SCAVENGE,             TF_RARITY_EPIC,      _S("Ê°»Ä"),         _S("Ã¿ÀÛ¼Æ»÷É± 500 Ö»½©Ê¬£¬È«Ö²Îï¹¥»÷Á¦ +10%") },
    { TF_UPG_DESPERATION,          TF_RARITY_EPIC,      _S("¾ø¾³"),         _S("Ã¿Ê§È¥ 1 µã·ÀÏß£¨Ò»¸ñ±»Õ¼£©£¬È«Ö²Îï¹¥ËÙ +5%") },

    // ---- ´«Ëµ³Ø£ºÖ¸Êı¼¶Óë¹æÔò¸ÄĞ´ ----
    { TF_UPG_GOD_NUT,              TF_RARITY_LEGENDARY, _S("Éñ¸ñ¡¤¼á¹û"),    _S("ËùÓĞ¼á¹ûÀàÉúÃü x3£¬ÕóÍöÊ±¶ÔÕûĞĞÔì³É¾Ş¶îÉËº¦") },
    { TF_UPG_GOD_ICE,              TF_RARITY_LEGENDARY, _S("Éñ¸ñ¡¤º®±ù"),    _S("ËùÓĞ¼õËÙĞ§¹û¸½¼ÓÕæÊµÉËº¦") },
    { TF_UPG_NEURO_CRIT,           TF_RARITY_LEGENDARY, _S("Éñ¾­¡¤±©»÷"),    _S("±©»÷²»ÔÙÓĞ¸ÅÂÊ£¬Ã¿´Î¹¥»÷±Ø±©£¬±¶ÂÊ +1") },
    { TF_UPG_INFINITE,             TF_RARITY_LEGENDARY, _S("ÎŞÏŞ"),         _S("¿¨²ÛÎ» +2£¨¿É³¬¹ı 12£©") },
    { TF_UPG_NIRVANA,              TF_RARITY_LEGENDARY, _S("Äù˜„"),         _S("Ã¿Æì¿ªÊ¼Ê±¸´»îÉÏÒ»ÆìÕóÍöµÄÖ²Îï") },
    { TF_UPG_DEVOUR,               TF_RARITY_LEGENDARY, _S("ÍÌÊÉ"),         _S("Ö²ÎïÕóÍöÊ±°ÑÆäÈ«²¿ÊôĞÔ·Ö¸øÏàÁÚÖ²Îï") },
    { TF_UPG_LEFT_FOOT_RIGHT_FOOT, TF_RARITY_LEGENDARY, _S("×ó½Å²ÈÓÒ½Å"),   _S("Ã¿³ÖÓĞ 1 Ìõ´«ËµÇ¿»¯£¬ËùÓĞÇ¿»¯Ğ§¹û +30%£¨×ÔÉíÒ²¼ÆÈë£©") },
    { TF_UPG_OVERLOADED,           TF_RARITY_LEGENDARY, _S("³¬ÔØ"),         _S("È«Ö²Îï¹¥ËÙ +50%£¬ÉúÃü -30%") },
    { TF_UPG_TIME_STOP,            TF_RARITY_LEGENDARY, _S("Ê±¼äÍ£ÖÍ"),     _S("Ã¿ÆìµÄÇ° 10 Ãë£¬ËùÓĞ½©Ê¬ÎŞ·¨ÒÆ¶¯") },
    { TF_UPG_OVERCLOCK,            TF_RARITY_LEGENDARY, _S("³¬ÆµºËĞÄ"),     _S("È«Ö²Îï¹¥ËÙ +60%") },
    { TF_UPG_WARHEAD,              TF_RARITY_LEGENDARY, _S("»ÙÃğµ¯Í·"),     _S("È«Ö²ÎïÉËº¦ +50%") },
    { TF_UPG_GOLD_HARVEST,         TF_RARITY_LEGENDARY, _S("»Æ½ğÊÕ¸î"),     _S("»÷É±±ØµôÑô¹â£¬Ã¿²ãÃæ¶î +25") },
    { TF_UPG_BURST_SEED,           TF_RARITY_LEGENDARY, _S("±©ÁÒÖÖ×Ó"),     _S("Ö²ÎïÖÖÏÂÁ¢¼´¿ª»ğ") }
};

// ----------------------------------------------------------------------------------------------------
// ½©Ê¬µÇ³¡±í
// ----------------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¿Á÷³ÌÈÕÖ¾£ºÅÅ²é¡°¸Ä¶¯Ã»ÉúĞ§¡±ÀàÎÊÌâµÄ¶¨Î»¹¤¾ß£¨µÍÆµ½Úµã²Åµ÷ÓÃ£©¡£
// Êä³öµ½ Release\thirtyflags_flow.log£¨¾ø¶ÔÂ·¾¶£¬±ÜÃâ¹¤×÷Ä¿Â¼²»Ò»ÖÂ£©¡£
// ThirtyFlags: flag-zombie aura query (cached once per frame)
bool ThirtyFlagsHasFlagZombieAlive(Board* theBoard)
{
    if (!ThirtyFlagsMode() || theBoard == nullptr)
        return false;
    static bool aCached = false;
    static int aCachedFrame = -1;
    static Board* aCachedBoard = nullptr;
    if (theBoard != aCachedBoard || theBoard->mMainCounter != aCachedFrame)
    {
        aCachedFrame = theBoard->mMainCounter;
        aCachedBoard = theBoard;
        aCached = false;
        Zombie* aZombie = nullptr;
        while (theBoard->IterateZombies(aZombie))
        {
            if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_FLAG)
            {
                aCached = true;
                break;
            }
        }
    }
    return aCached;
}

void TFLog(const char* theLine)
{
#ifdef TF_PLAYER_BUILD
    (void)theLine;
    return;
#else
    // Ïà¶ÔÂ·¾¶£ºÂäÔÚ exe Í¬Ä¿Â¼£¬Óë LawnApp µÄ thirtyflags.log Ò»ÖÂ
    FILE* f = fopen("thirtyflags_flow.log", "a");
    if (f)
    {
        fprintf(f, "[%d] %s\n", (int)time(NULL), theLine);
        fclose(f);
    }

#endif
}

void ThirtyFlags::BuildZombieMask(int theFlag, bool* theAllowed)
{
    for (int i = 0; i < NUM_ZOMBIE_TYPES; i++)
    {
        theAllowed[i] = false;
    }

    theAllowed[(int)ZOMBIE_NORMAL] = true;
    theAllowed[(int)ZOMBIE_FLAG] = true;
    if (theFlag <= 1)
        return;

    if (theFlag >= 1)  theAllowed[(int)ZOMBIE_TRAFFIC_CONE] = true;
    if (theFlag >= 2)  theAllowed[(int)ZOMBIE_PAIL] = true;
    if (theFlag >= 2)
    {
        theAllowed[(int)ZOMBIE_POLEVAULTER] = true;
        theAllowed[(int)ZOMBIE_NEWSPAPER] = true;
    }
    if (theFlag >= 3)
    {
        theAllowed[(int)ZOMBIE_DOOR] = true;
        theAllowed[(int)ZOMBIE_DUCKY_TUBE] = true;
    }
    if (theFlag >= 5) theAllowed[(int)ZOMBIE_SNORKEL] = true;
    if (theFlag >= 4) theAllowed[(int)ZOMBIE_FOOTBALL] = true;
    if (theFlag >= 4) theAllowed[(int)ZOMBIE_DANCER] = true;
    if (theFlag >= 6)
    {
        theAllowed[(int)ZOMBIE_DOLPHIN_RIDER] = true;
        theAllowed[(int)ZOMBIE_BALLOON] = true;
    }
    if (theFlag >= 6) theAllowed[(int)ZOMBIE_DIGGER] = true;
    if (theFlag >= 6)
    {
        theAllowed[(int)ZOMBIE_JACK_IN_THE_BOX] = true;
        theAllowed[(int)ZOMBIE_LADDER] = true;
    }
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_CATAPULT] = true;
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_POGO] = true;
    if (theFlag >= 5) theAllowed[(int)ZOMBIE_GARGANTUAR] = true;
    if (theFlag >= 6) theAllowed[(int)ZOMBIE_ZAMBONI] = true;
    if (theFlag >= 7)
    {
        theAllowed[(int)ZOMBIE_PEA_HEAD] = true;
        theAllowed[(int)ZOMBIE_WALLNUT_HEAD] = true;
    }
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_REDEYE_GARGANTUAR] = true;
    if (theFlag >= 8)
    {
        theAllowed[(int)ZOMBIE_JALAPENO_HEAD] = true;
        theAllowed[(int)ZOMBIE_GATLING_HEAD] = true;
        theAllowed[(int)ZOMBIE_SQUASH_HEAD] = true;
        theAllowed[(int)ZOMBIE_TALLNUT_HEAD] = true;
    }
    // Final boss only ever appears on flag 30; ThirtyFlagsBossUpdate spawns it explicitly.
    if (theFlag >= TF_BOSS_FLAG)
        theAllowed[(int)ZOMBIE_BOSS] = true;
}

// ----------------------------------------------------------------------------------------------------
// ¹¹Ôì / ÉúÃüÖÜÆÚ
// ----------------------------------------------------------------------------------------------------
ThirtyFlags::ThirtyFlags()
{
    mActive = false;
    mFlag = 0;
    mUnlockedRows = TF_R2;
    mWaterRows = 0;
    mWaterCols = 0;
    mFogCols = 0;
    memset(mMutations, 0, sizeof(mMutations));
    mMutationPicksLeft = 0;
    memset(mUpgradeStacks, 0, sizeof(mUpgradeStacks));
    mUpgradeCount = 0;
    mLegendaryCount = 0;
    mIntermission = false;
    mUpgradeChosen = false;
    mUpgradeDialogShown = false;
    mPendingUpgradeChoices[0] = TF_UPG_HASTE;
    mPendingUpgradeChoices[1] = TF_UPG_HASTE;
    mPendingUpgradeChoices[2] = TF_UPG_HASTE;
    mPrepTimeLeft = 0;
    mPrepTimeStart = 1;
    mSunAtFlagStart = 0;
    mTotalKills = 0;
    mKillStreak = 0;
    mKillStreakTimer = 0;
    mLostLanes = 0;

    mBossPhase = 0;
    mBossPtr = NULL;
    mBossSkillTimer = 0;
    mBossWarnTimer = 0;
    mBossWarnSkill = 0;
    mBossWarnRow = 0;
    mBossSelfDestruct = 0;
    mBossEmpTimer = 0;
    mBossRoarTimer = 0;
    mBossDomainTimer = 0;
}

void ThirtyFlags::StartRun()
{
    *this = ThirtyFlags();
    mActive = true;
    mMutationPicksLeft = TF_MUTATION_PICKS;
    ApplyFlag(1);

    memset(gThirtyFlagsPoison, 0, sizeof(gThirtyFlagsPoison));
    memset(gThirtyFlagsWaterFade, 0, sizeof(gThirtyFlagsWaterFade));
    gThirtyFlagsMowerGranted = 0;

    // ¿ª¾Ö£ºÒÑ¿ª·ÅµÄĞĞÖ±½ÓÏÔÊ¾Îª²İÆ¤£¨²»²¥¶¯»­£©£¬ÆäÓàĞĞµÈ½âËøÊ±ÔÙ¹ö³ö
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        gThirtyFlagsSodProgress[aRow] = 0;
    }
    gThirtyFlagsPrevUnlockedRows = 0;
}

void ThirtyFlags::EndRun()
{
    mActive = false;
    mIntermission = false;
}

void ThirtyFlags::ApplyFlag(int theFlag)
{
    mFlag = TFClampI(theFlag, 1, TF_TOTAL_FLAGS);
    const ThirtyFlagsFlagDef& aDef = GetFlagDef(mFlag);
    mUnlockedRows = aDef.mUnlockedRows;
    mWaterRows = aDef.mWaterRows;
    mWaterCols = aDef.mWaterCols;
    mFogCols = aDef.mFogCols;
}

const ThirtyFlagsFlagDef& ThirtyFlags::GetFlagDef(int theFlag)
{
    return gThirtyFlagsFlagDefs[TFClampI(theFlag, 1, TF_TOTAL_FLAGS) - 1];
}

const ThirtyFlagsUpgradeDef& ThirtyFlags::GetUpgradeDef(ThirtyFlagsUpgrade theUpgrade)
{
    return gThirtyFlagsUpgradeDefs[TFClampI((int)theUpgrade, 0, TF_UPG_COUNT - 1)];
}

// ----------------------------------------------------------------------------------------------------
// ³¡µØ
// ----------------------------------------------------------------------------------------------------
bool ThirtyFlags::IsWaterCell(int theRow, int theCol) const
{
    if (!IsWaterRow(theRow) || mWaterCols <= 0)
        return false;

    return theCol >= MAX_GRID_SIZE_X - mWaterCols;
}

int ThirtyFlags::GetWalkableRowCount() const
{
    int aCount = 0;
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        if (IsRowUnlocked(aRow))
            aCount++;
    }
    return aCount;
}

// ----------------------------------------------------------------------------------------------------
// Ê¬³±½ø»¯£º³É³¤
// ----------------------------------------------------------------------------------------------------
bool ThirtyFlags::HasMutation(int theMutation) const
{
    if (theMutation < 0 || theMutation >= NUM_MUTATIONS)
        return false;
    return mMutations[theMutation];
}

float ThirtyFlags::GetHealthScale() const
{
    // ¡¾ÈıÊ®Æì¡¿È«¾Ö½©Ê¬Í¬²½ÔöÇ¿£¨ÓÃ»§ĞèÇó£©£º»ù´¡ÑªÁ¿ x1.3£¬ÓëÖ²Îï³¬Ä£µÈ±ÈÉÏÕÇ
    float aScale = powf(1.0f + TF_HP_GROWTH_PER_FLAG, (float)(mFlag - 1)) * 1.3f;
    return min(aScale, TF_HP_GROWTH_CAP * 1.3f);
}

float ThirtyFlags::GetSpeedScale() const
{
    float aBonus = TF_SPEED_GROWTH_PER_FLAG * (float)(mFlag - 1);
    aBonus = min(aBonus, TF_SPEED_GROWTH_CAP);
    if (HasMutation(MUTATION_SWIFT))
        aBonus += 0.25f;
    return 1.0f + aBonus;
}

float ThirtyFlags::GetCountScale() const
{
    return 1.0f + 0.03f * (float)max(0, mFlag - 9);
}

float ThirtyFlags::GetZombieDamageScale(Board* theBoard) const
{
    float aScale = 1.0f;

    if (HasMutation(MUTATION_RESONANCE) && theBoard)
    {
        int aCount = 0;
        Zombie* aZombie = nullptr;
        while (theBoard->IterateZombies(aZombie))
        {
            if (!aZombie->IsDeadOrDying())
                aCount++;
        }
        aScale += 0.01f * (float)min(aCount, 100);
    }

    return aScale;
}

float ThirtyFlags::GetZombieDamageReduction() const
{
    return HasMutation(MUTATION_ARMOR) ? 0.15f : 0.0f;
}

void ThirtyFlags::RollMutation()
{
    if (mMutationPicksLeft <= 0)
        return;

    int aCandidates[NUM_MUTATIONS];
    int aCount = 0;
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (!mMutations[i])
            aCandidates[aCount++] = i;
    }

    if (aCount == 0)
    {
        mMutationPicksLeft = 0;
        return;
    }

    mMutations[aCandidates[Rand(aCount)]] = true;
    mMutationPicksLeft--;
}

int ThirtyFlags::GetEliteChancePercent() const
{
    if (mFlag < TF_ELITE_START_FLAG)
        return 0;

    return min(30 + (mFlag - TF_ELITE_START_FLAG) * 5, 90);
}

// ----------------------------------------------------------------------------------------------------
// Èâ¸ëÇ¿»¯
// ----------------------------------------------------------------------------------------------------
float ThirtyFlags::GetLegendaryAmplifier() const
{
    int aStacks = GetStacks(TF_UPG_LEFT_FOOT_RIGHT_FOOT);
    if (aStacks <= 0)
        return 1.0f;

    return 1.0f + 0.30f * (float)(mLegendaryCount * aStacks);
}

int ThirtyFlags::CountRarity(ThirtyFlagsRarity theRarity) const
{
    int aCount = 0;
    for (int i = 0; i < TF_UPG_COUNT; i++)
    {
        if (mUpgradeStacks[i] > 0 && gThirtyFlagsUpgradeDefs[i].mRarity == theRarity)
            aCount += mUpgradeStacks[i];
    }
    return aCount;
}

int ThirtyFlags::CountEconomyUpgrades() const
{
    int aCount = 0;
    aCount += GetStacks(TF_UPG_MASS_PRODUCE);
    aCount += GetStacks(TF_UPG_ABUNDANCE);
    aCount += GetStacks(TF_UPG_SIPHON);
    aCount += GetStacks(TF_UPG_HIGH_YIELD);
    aCount += GetStacks(TF_UPG_DRAIN);
    aCount += GetStacks(TF_UPG_COMPOUND_SUN);
    return aCount;
}

int ThirtyFlags::GetBonusSeedSlots() const
{
    return 2 * GetStacks(TF_UPG_INFINITE);
}

void ThirtyFlags::RollUpgradeChoices()
{
    ThirtyFlagsRarity aAllowed[4];
    int aWeights[4];
    int aAllowedCount = 0;

    aAllowed[aAllowedCount] = TF_RARITY_COMMON;     aWeights[aAllowedCount++] = 40;
    aAllowed[aAllowedCount] = TF_RARITY_RARE;       aWeights[aAllowedCount++] = 30;
    if (mFlag >= 10)
    {
        aAllowed[aAllowedCount] = TF_RARITY_EPIC;
        aWeights[aAllowedCount++] = (mFlag >= 15) ? 30 : 12;
    }
    if (mFlag >= 20)
    {
        aAllowed[aAllowedCount] = TF_RARITY_LEGENDARY;
        aWeights[aAllowedCount++] = 12;
    }

    for (int aChoice = 0; aChoice < 3; aChoice++)
    {
        int aTotal = 0;
        for (int i = 0; i < aAllowedCount; i++)
            aTotal += aWeights[i];

        int aRoll = Rand(aTotal);
        int aAccum = 0;
        ThirtyFlagsRarity aRarity = aAllowed[0];
        for (int i = 0; i < aAllowedCount; i++)
        {
            aAccum += aWeights[i];
            if (aRoll < aAccum)
            {
                aRarity = aAllowed[i];
                break;
            }
        }

        int aCandidates[TF_UPG_COUNT];
        int aCount = 0;
        // ¡¾ÈıÊ®Æì¡¿È¥ÖØ£¨ÓÃ»§·´À¡ÈıÑ¡Ò»ÖØ¸´£©£ºÒÑÑ¡ÌõÄ¿´ÓºòÑ¡³ØÌŞ³ı£»
        // ¸ÃÏ¡ÓĞ¶ÈºòÑ¡ºÄ¾¡Ôò»ØÍËµ½È«ÌåÎ´Ñ¡ÌõÄ¿¡£
        for (int i = 0; i < TF_UPG_COUNT; i++)
        {
            if (gThirtyFlagsUpgradeDefs[i].mRarity != aRarity)
                continue;
            bool aDup = false;
            for (int p = 0; p < aChoice; p++)
            {
                if (mPendingUpgradeChoices[p] == (ThirtyFlagsUpgrade)i)
                {
                    aDup = true;
                    break;
                }
            }
            if (!aDup)
                aCandidates[aCount++] = i;
        }
        if (aCount == 0)
        {
            for (int i = 0; i < TF_UPG_COUNT; i++)
            {
                bool aDup = false;
                for (int p = 0; p < aChoice; p++)
                {
                    if (mPendingUpgradeChoices[p] == (ThirtyFlagsUpgrade)i)
                    {
                        aDup = true;
                        break;
                    }
                }
                if (!aDup)
                    aCandidates[aCount++] = i;
            }
        }

        mPendingUpgradeChoices[aChoice] = (aCount > 0) ? (ThirtyFlagsUpgrade)aCandidates[Rand(aCount)] : TF_UPG_HASTE;
    }
}

void ThirtyFlags::GrantUpgrade(ThirtyFlagsUpgrade theUpgrade)
{
    if ((int)theUpgrade < 0 || (int)theUpgrade >= TF_UPG_COUNT)
        return;

    mUpgradeStacks[theUpgrade]++;
    mUpgradeCount++;

    if (gThirtyFlagsUpgradeDefs[theUpgrade].mRarity == TF_RARITY_LEGENDARY)
    {
        mLegendaryCount++;
    }
}

// ----------------------------------------------------------------------------------------------------
// ×ÛºÏÕ½¶·ÏµÊı
// ----------------------------------------------------------------------------------------------------
float ThirtyFlags::GetAttackSpeedBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.0f;
    aBonus += 0.12f * (float)GetStacks(TF_UPG_HASTE);
    aBonus += 0.08f * (float)(GetStacks(TF_UPG_RESONANCE_HASTE) * GetStacks(TF_UPG_HASTE));
    aBonus += 0.05f * (float)(GetStacks(TF_UPG_DESPERATION) * mLostLanes);
    aBonus += 0.50f * (float)GetStacks(TF_UPG_OVERLOADED);
    aBonus += 0.60f * (float)GetStacks(TF_UPG_OVERCLOCK);

    aBonus *= GetLegendaryAmplifier();

    return 1.0f + aBonus;
}

float ThirtyFlags::GetDamageBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.0f;
    aBonus += 0.15f * (float)(GetStacks(TF_UPG_RESONANCE_BARRAGE) * GetStacks(TF_UPG_BARRAGE));
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_RESONANCE_WEALTH) * CountEconomyUpgrades());
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_SCAVENGE) * (mTotalKills / 500));
    aBonus += 0.50f * (float)GetStacks(TF_UPG_WARHEAD);

    int aBloodRage = GetStacks(TF_UPG_BLOOD_RAGE);
    if (aBloodRage > 0)
    {
        aBonus += 0.30f * (float)aBloodRage * max(0.0f, GetHealthBonus() - 1.0f);
    }

    aBonus *= GetLegendaryAmplifier();
    return 1.0f + aBonus;
}

float ThirtyFlags::GetHealthBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.35f * (float)GetStacks(TF_UPG_REINFORCE);
    aBonus -= 0.30f * (float)GetStacks(TF_UPG_OVERLOADED);
    aBonus *= GetLegendaryAmplifier();

    float aMultiplier = powf(3.0f, (float)GetStacks(TF_UPG_GOD_NUT));
    return max(0.1f, (1.0f + aBonus) * aMultiplier);
}

float ThirtyFlags::GetCostScale() const
{
    if (!mActive)
        return 1.0f;

    float aReduction = 0.20f * (float)GetStacks(TF_UPG_MASS_PRODUCE);
    aReduction *= GetLegendaryAmplifier();
    return max(0.0f, 1.0f - aReduction);
}

int ThirtyFlags::GetExtraProjectiles() const
{
    if (!mActive)
        return 0;

    int aExtra = GetStacks(TF_UPG_BARRAGE);
    int aOverloads = GetStacks(TF_UPG_OVERLOAD);
    if (aOverloads > 0)
    {
        int aHundreds = (int)((GetAttackSpeedBonus() - 1.0f) / 1.0f);
        aExtra += max(0, aHundreds) * aOverloads;
    }
    return aExtra;
}

float ThirtyFlags::GetArmorPierce() const
{
    if (!mActive)
        return 0.0f;
    return 0.25f * (float)GetStacks(TF_UPG_ARMORPIERCE) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetThornsReflect() const
{
    if (!mActive)
        return 0.0f;
    return 0.20f * (float)GetStacks(TF_UPG_THORNS) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetSunDropIntervalScale() const
{
    if (!mActive)
        return 1.0f;
    float aReduction = 0.25f * (float)GetStacks(TF_UPG_ABUNDANCE) * GetLegendaryAmplifier();
    return max(0.05f, 1.0f - aReduction);
}

float ThirtyFlags::GetSunflowerYieldBonus() const
{
    if (!mActive)
        return 1.0f;
    return 1.0f + 0.60f * (float)GetStacks(TF_UPG_HIGH_YIELD) * GetLegendaryAmplifier();
}

int ThirtyFlags::GetKillSunReward() const
{
    if (!mActive)
        return 0;

    float aReward = 3.0f * (float)GetStacks(TF_UPG_SIPHON);
    if (mKillStreak > 0)
    {
        float aStreakBonus = min(0.015f * (float)mKillStreak, 0.15f);
        aReward *= (1.0f + aStreakBonus);
    }
    return (int)aReward;
}

float ThirtyFlags::GetDeathRefundRate() const
{
    if (!mActive)
        return 0.0f;
    return 0.50f * (float)GetStacks(TF_UPG_DRAIN) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetLilypadSpeedBonus() const
{
    return (mActive && GetStacks(TF_UPG_SYMBIOSIS_WATER) > 0) ? 0.30f : 0.0f;
}

int ThirtyFlags::GetTimeStopSeconds() const
{
    if (!mActive)
        return 0;
    return 10 * GetStacks(TF_UPG_TIME_STOP);
}

// ----------------------------------------------------------------------------------------------------
// ±íÏÖ²ã / Í³¼Æ
// ----------------------------------------------------------------------------------------------------
void ThirtyFlags::UpdateKillStreak()
{
    if (mKillStreakTimer > 0)
    {
        mKillStreakTimer--;
        if (mKillStreakTimer == 0)
        {
            mKillStreak = 0;
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¡¸Ö±½ÓÉËº¦¡¹µÄÍ³Ò»Èë¿Ú ¡ª¡ª È«²¿×ñÊØ¡¸ÄÏ¹ÏÓÅÏÈ³ĞÉË¡¹
//
// Ê¬±¬ / Ê¬¶¾ / ¾«Ó¢Í¶Ã¬ / ½©Íõ¼¼ÄÜ¶¼ÊôÓÚ"ÈÆ¿ªÕı³£·ÀÓù½áËãµÄÖ±½ÓÉËº¦"¡£
// ¹æÔò£¨ÓÃ»§ĞèÇó£©£º
//   * ËüÃÇ¿ÛÑª£¬²»ÔÙÎŞÌõ¼şÃëÉ± ¡ª¡ª ÑªÁ¿ºñµÄÖ²ÎïÓ¦¸Ã¿¸µÃ×¡£»
//   * **¸Ã¸ñÓĞÄÏ¹ÏÍ·Ê±£¬ÉËº¦È«²¿ÓÉÄÏ¹Ï³Ô**£¬Ì×ÔÚÀïÃæµÄÖ²ÎïÒ»µãÑª²»µô
//     £¨ÄÏ¹Ï¾ÍÊÇ»¤¼× ¡ª¡ª ÕâÒ²ÊÇ PvZ ÀïÄÏ¹ÏµÄÉè¼ÆÒâÍ¼£©¡£
// ----------------------------------------------------------------------------------------------------

// Ä³¸ñ¡¸Ö±½ÓÉËº¦¡¹µÄ³ĞÉË¶ÔÏó£ºÓĞÄÏ¹Ï¾ÍÊÇÄÏ¹Ï£¬·ñÔòÊÇ¸Ã¸ñµÄÆÕÍ¨Ö²Îï
static Plant* TFDirectDamageTarget(Board* theBoard, int theGridX, int theGridY)
{
    if (!theBoard)
        return nullptr;

    Plant* aPumpkin = theBoard->GetPumpkinAt(theGridX, theGridY);
    if (aPumpkin && !aPumpkin->mDead)
        return aPumpkin;

    return theBoard->GetTopPlantAt(theGridX, theGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
}

// ¶Ô°ë¾¶ÄÚµÄ¸ñ×Ó¸÷½áËãÒ»´ÎÖ±½ÓÉËº¦£¨Ã¿¸ñÖ»´òÒ»´Î£¬±ÜÃâÄÏ¹Ï±»Í¬¸ñ¶àÖêÖ²ÎïÖØ¸´×ª·¢£©
// ·µ»ØÖÂËÀÊıÁ¿
static int TFDirectDamageInRadius(Board* theBoard, int thePixelX, int thePixelY, int theRadius, int theDamage)
{
    if (!theBoard)
        return 0;

    bool aDone[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];
    memset(aDone, 0, sizeof(aDone));

    int aKilled = 0;
    Plant* aPlant = nullptr;
    while (theBoard->IteratePlants(aPlant))
    {
        if (aPlant->mDead || aPlant->mPlantCol < 0 || aPlant->mPlantCol >= MAX_GRID_SIZE_X ||
            aPlant->mRow < 0 || aPlant->mRow >= MAX_GRID_SIZE_Y)
            continue;

        if (!GetCircleRectOverlap(thePixelX, thePixelY, theRadius, aPlant->GetPlantRect()))
            continue;

        if (aDone[aPlant->mPlantCol][aPlant->mRow])
            continue;
        aDone[aPlant->mPlantCol][aPlant->mRow] = true;

        Plant* aVictim = TFDirectDamageTarget(theBoard, aPlant->mPlantCol, aPlant->mRow);
        if (!aVictim)
            continue;

        aVictim->mPlantHealth -= theDamage;
        if (aVictim->mPlantHealth <= 0)
        {
            theBoard->mPlantsEaten++;
            aVictim->Die();
            aKilled++;
        }
    }

    return aKilled;
}

void ThirtyFlags::OnZombieKilled(Board* theBoard, int theX, int theY, int theRow)
{
    if (!mActive)
        return;

    mTotalKills++;
    mKillStreak++;
    mKillStreakTimer = 100;

    int aSun = GetKillSunReward();
    if (aSun > 0 && theBoard)
    {
        theBoard->AddSunMoney(aSun);
    }

    // ¡¾ÈıÊ®Æì¡¿¡¸»Æ½ğÊÕ¸î¡¹£º»÷É±±ØµôÑô¹â£¬Ã¿²ãÃæ¶î +25£¨ÓÃ»§ĞèÇó£©
    int aHarvestSun = 25 * GetStacks(TF_UPG_GOLD_HARVEST);
    if (aHarvestSun > 0 && theBoard)
    {
        theBoard->AddSunMoney(aHarvestSun);
    }

    if (!theBoard)
        return;

    if (HasMutation(MUTATION_ZOMBIE_EXPLODE))
    {
        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Ê¬±¬Ô­±¾ÊÇ KillAllPlantsInRadius(90) ¡ª¡ª Ò²¾ÍÊÇ**ÎŞÌõ¼şÃëÉ±**
        // °ë¾¶ÄÚËùÓĞÖ²Îï£¬¶øÇÒ**Ò»µãÌØĞ§¶¼Ã»ÓĞ**¡£ºó¹ûÊÇÍæ¼Ò¿´µ½¡¸Ö²ÎïÄªÃûÆäÃîÏûÊ§¡¹£¬
        // ÒÔÎªÊÇ bug£¨Êµ²â·´À¡¹ı£©¡£ÏÖÔÚ¸Ä³É£º
        //   1) ÏÈ·Å±¬Õ¨Á£×Ó£¨¿É¼û£©
        //   2) ÔÙ°´¹Ì¶¨ÉËº¦½áËã£¨8000 ÑªµÄ¼á¹û/ÄÏ¹Ï¿¸µÃ×¡£©
        //   3) ÂäÈÕÖ¾£¬±ãÓÚÅÅ²é¡¸Ö²ÎïÎªÊ²Ã´Ã»ÁË¡¹
        int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
        theBoard->mApp->AddTodParticle((float)theX, (float)theY, aRenderOrder, ParticleEffect::PARTICLE_JACKEXPLODE);

        int aKilled = TFDirectDamageInRadius(theBoard, theX, theY, TF_EXPLODE_RADIUS, TF_EXPLODE_DAMAGE);

        {
            char aBuf[160];
            sprintf(aBuf, "[TFPlant] explode at px=(%d,%d) dmg=%d killed=%d",
                theX, theY, TF_EXPLODE_DAMAGE, aKilled);
            TFLog(aBuf);
        }
    }

    if (HasMutation(MUTATION_ZOMBIE_POISON))
    {
        TFAddPoison(theBoard->PixelToGridXKeepOnBoard(theX, theY),
                    theBoard->PixelToGridYKeepOnBoard(theX, theY), 450);
    }

    if (HasMutation(MUTATION_PROSPER) && Rand(100) < 30)
    {
        int aGridY = theRow;
        if (aGridY < 0 || aGridY >= MAX_GRID_SIZE_Y)
            aGridY = TFClampI(theBoard->PixelToGridYKeepOnBoard(theX, theY), 0, MAX_GRID_SIZE_Y - 1);

        Zombie* aImp = theBoard->AddZombieInRow(ZOMBIE_IMP, aGridY, theBoard->mCurrentWave);
        if (aImp)
        {
            aImp->mPosX = (float)theX;
        }
    }

    if (HasMutation(MUTATION_DIG))
    {
        int aGridX = theBoard->PixelToGridXKeepOnBoard(theX, theY);
        int aGridY = theBoard->PixelToGridYKeepOnBoard(theX, theY);
        if (aGridX >= 0 && aGridX < MAX_GRID_SIZE_X && aGridY >= 0 && aGridY < MAX_GRID_SIZE_Y)
        {
            if (!theBoard->GetCraterAt(aGridX, aGridY) && !theBoard->GetGraveStoneAt(aGridX, aGridY))
            {
                GridItem* aCrater = theBoard->AddACrater(aGridX, aGridY);
                if (aCrater)
                {
                    aCrater->mGridItemCounter = 150;
                }
            }
        }
    }
}

// ====================================================================================================
// ¹³×ÓÊµÏÖ
// ====================================================================================================

void ThirtyFlagsInitRun(Board* theBoard)
{
    gThirtyFlags.StartRun();

    // ¿ª¾ÖÒÑ¿ª·ÅµÄĞĞÖ±½Ó³ÊÏÖÎª²İÆ¤£¬²»Áô¹ö¶¯¶¯»­
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        gThirtyFlagsSodProgress[aRow] = gThirtyFlags.IsRowUnlocked(aRow) ? 1000 : 0;
    }
    gThirtyFlagsPrevUnlockedRows = gThirtyFlags.mUnlockedRows;

    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            gThirtyFlagsWaterFade[x][y] = gThirtyFlags.IsWaterCell(y, x) ? 100 : 0;
        }
    }
}

void ThirtyFlagsSetupBoard(Board* theBoard)
{
    if (!theBoard)
        return;


    if (!gThirtyFlags.mActive)
    {
        gThirtyFlags.StartRun();
    }

    for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
    {
        bool aUnlocked = gThirtyFlags.IsRowUnlocked(y);
        bool aWaterRow = gThirtyFlags.IsWaterRow(y);

        if (!aUnlocked)
        {
            // Î´¿ª·ÅµÄĞĞ£ºÕûĞĞ²»¿ÉÖÖÖ²£¨DIRT ½ö×÷ÄÚ²¿±ê¼Ç£¬ÊÓ¾õÓÉ DrawBackdrop »­ºÚÄ»£©
            theBoard->mPlantRow[y] = PLANTROW_DIRT;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = GRIDSQUARE_DIRT;
            }
            continue;
        }

        if (aWaterRow)
        {
            theBoard->mPlantRow[y] = PLANTROW_POOL;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = gThirtyFlags.IsWaterCell(y, x) ? GRIDSQUARE_POOL : GRIDSQUARE_GRASS;
            }
        }
        else
        {
            theBoard->mPlantRow[y] = PLANTROW_NORMAL;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = GRIDSQUARE_GRASS;
            }
        }
    }

    // ÃÔÎí£ºÓÒ²à mFogCols ÁĞ
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
        {
            bool aFog = (y < MAX_GRID_SIZE_Y) && gThirtyFlags.IsRowUnlocked(y) && gThirtyFlags.IsFogCell(x);
            theBoard->mGridCelFog[x][y] = aFog ? 255 : 0;
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// ¡¾±íÏÖ²ã¡¿ÌØĞ§²ã£¨¼ÓÉ«ËÄ±ßĞÎ³Ø£©
//
// ÎªÊ²Ã´Òªµ¥¶À³É²ã£¨¶ø²»ÊÇ"Ã¿¸öÌØĞ§Ò»¸ö¹Ç÷À¶¯»­"£©£º
//   ¡ì12 µÄ½áÂÛÊÇ"ºÏÅúµÄºìÀûÒª¿¿Í¬ÎÆÀí + Í¬»ìºÏÄ£Ê½ + Ë³ĞòÎŞ¹Ø"¡£¶øÌØĞ§Ç¡ºÃÈıÕß¶¼Âú×ã£º
//     * Ö»ÓÃÁ½ÕÅ**ÏÖ³É**ÌùÍ¼£ºIMAGE_SPOTLIGHT£¨Èí¹âÔÎ£©/ IMAGE_WHITEPIXEL£¨ÊµĞÄËÄ±ßĞÎ£©¡ª¡ª ÁãĞÂ×ÊÔ´£»
//     * È«²ãÖ»ÓÃ¼ÓÉ«»ìºÏ£¨DRAWMODE_ADDITIVE£©£¬ÖĞ¼ä²»»»Ä£Ê½£»
//     * ÌØĞ§Ö®¼äÊÇ¼ÓÉ«µş¼Ó£¬**ÎŞÕÚ¸ÇÓïÒå** ¡ú »æÖÆË³Ğò²»Ó°Ïì»­Ãæ ¡ú ÔÊĞí°´ÀàĞÍ³É×é¡£
//   ÓÚÊÇ "°´ÀàĞÍ·Ö×é»æÖÆ" ¾ÍµÈÓÚ "Ã¿ÀàËùÓĞÌØĞ§ºÏ²¢³É¼«ÉÙÊı´Î»æÖÆµ÷ÓÃ"¡£
//
// ³ØÂúÊ±¸²¸ÇÊÙÃü×î¶ÌµÄÒ»¸ö£¨¶ø²»ÊÇ¶ªÆú£©£¬±£Ö¤"ĞÂÊÂ¼şÒ»¶¨¿´µÃ¼û"¡£
// ----------------------------------------------------------------------------------------------------
// ¡¾ĞÔÄÜ¡¿ÌØĞ§²ãµÄÁ½¸öÓ²ÉÏÏŞ£º
//   ³ØÈİÑÏÖÆ²¢·¢ÊıÁ¿£»ÒÔ¼°"Ã¿Ö¡³ÌÌî³äÔ¤Ëã"¡ª¡ª¼Ó·¨»ìºÏÊÇÖğÏñËØ¶ÁĞ´£¬
//   Ãæ»ıÊÇºÜ¹óµÄ×ÊÔ´£¬²»ÄÜÈÎÆäÎŞÏŞÔö³¤¡£
enum { TF_FX_MAX = 192 };
// ¡¾¿ª¹Ø¡¿ÌØĞ§²ã×Ü¿ª¹Ø£ºÄ¬ÈÏ 0¡£
// °´ §15 µÄ¹æ¾Ø£ºÎ´Íê³É¡¸²İÆºÂú³¡¡¹¸´ºËµÄäÈÈ¾¸Ä¶¯²»ÄÜÄ¬ÈÏ¿ª×Å¡£
// ¿ªÆôÇ°ÇëÏÈ·´¸´¡°ÏÈ´´¼ò²âÒ»¸ö²İÆºÂú³¡µÄÖ¡¡±£¬²¢¶ÔÕÕ draw ·åÖµ¡£
#define TF_FX_ENABLE 0

enum { TF_FX_FILL_BUDGET = 60000 };   // Ã¿Ö¡ÌØĞ§ÏñËØ×ÜÃæ»ıÔ¤Ëã£¨ÏñËØ²£Ò»°ëÆÁ£©
static int gTfFxFillUsed = 0;         // ±¾Ö¡ÒÑÓÃÃæ»ı
enum { TF_FX_GLOW = 0, TF_FX_FLASH = 1, TF_FX_TRAIL = 2, TF_FX_KIND_COUNT = 3 };

struct TfFxQuad
{
    float   mX, mY;          // ÖĞĞÄ
    float   mSize;           // ±ß³¤
    float   mGrow;           // Ã¿Ö¡ÅòÕÍ
    int     mLife, mLifeMax;
    int     mKind;
    int     mAlpha;
    int     mR, mG, mB;
};

static TfFxQuad gTfFx[TF_FX_MAX];
static int      gTfFxCount = 0;

// ÔªËØÅäÉ«£ºÓë»ğ¾æÊ÷×®×ª»»»ğÇòÊ±µÄ¼ÓÉ«È¾É«±£³ÖÒ»ÖÂ£¬Íæ¼ÒÒ»ÑÛÄÜ¶ÔÉÏ
static void TfFxGetElementColor(int theElement, int& theR, int& theG, int& theB)
{
    theR = 255; theG = 220; theB = 140;
    if (theElement == TF_ELEM_ICE)            { theR = 40;  theG = 90;  theB = 255; }
    else if (theElement == TF_ELEM_DEEPFREEZE) { theR = 0;   theG = 40;  theB = 220; }
    else if (theElement == TF_ELEM_POISON)     { theR = 40;  theG = 220; theB = 40;  }
    else if (theElement == TF_ELEM_CHARM)      { theR = 255; theG = 60;  theB = 180; }
    else if (theElement == TF_ELEM_BUTTER)     { theR = 255; theG = 210; theB = 30;  }
    else if (theElement == TF_ELEM_GIANT)      { theR = 255; theG = 120; theB = 0;   }
}

// ÊÇ·ñ´øÈÎÒâ¾«Ó¢´ÊÌõ£¨ÓÃÓÚ¾«Ó¢ÍşÑ¹±íÏÖ£©
static bool TfFxIsElite(Zombie* theZombie)
{
    for (int aKind = 0; aKind < TF_ELITE_KIND_COUNT; aKind++)
    {
        if (TFHasElite(theZombie, aKind))
            return true;
    }
    return false;
}

static void TfFxSpawn(int theKind, float theX, float theY, float theSize, float theGrow,
                      int theLife, int theAlpha, int theR, int theG, int theB)
{
    if (!ThirtyFlagsMode())
        return;

    int aSlot;
    if (gTfFxCount < TF_FX_MAX)
        aSlot = gTfFxCount++;
    else
    {
        aSlot = 0;
        for (int i = 1; i < TF_FX_MAX; i++)
        {
            if (gTfFx[i].mLife < gTfFx[aSlot].mLife)
                aSlot = i;
        }
    }

    TfFxQuad& aQuad = gTfFx[aSlot];
    aQuad.mX = theX;
    aQuad.mY = theY;
    aQuad.mSize = (theSize < 4.0f) ? 4.0f : ((theSize > 72.0f) ? 72.0f : theSize);
    aQuad.mGrow = (theGrow > 3.0f) ? 3.0f : theGrow;
    aQuad.mLife = theLife;
    aQuad.mLifeMax = theLife;
    aQuad.mKind = theKind;
    aQuad.mAlpha = theAlpha;
    aQuad.mR = theR;
    aQuad.mG = theG;
    aQuad.mB = theB;
}

static void ThirtyFlagsFxUpdate()
{
    gTfFxFillUsed = 0;   // ¡¾ĞÔÄÜ¡¿Ã¿Ö¡ÖØÖÃÌî³ä¼ÆÊı
#if !defined(TF_PLAYER_BUILD) && TF_FX_ENABLE
	// ¡¾×Ô²â¡¤¿ÉÊÓ³¡¾°¡¿µ÷ÊÔ°æ£ºÃ¿ 40 Ö¡ÔÚ²İÆº**¹Ì¶¨Î»ÖÃ**Éú³ÉÒ»×é**ËÄÖÖÌØĞ§¸÷Ò»**£¬
	// ÓëÕ½¶·×´Ì¬ÍêÈ«½âñî ¡ª¡ª Ò»´ÎÔËĞĞ + Ò»ÕÅ½ØÍ¼¼´¿ÉÍê³É"¹Û¸Ğ + draw ·åÖµ"¸´ºË¡£
	// ËÄ¸öÎ»ÖÃÂÖ×ª£¬±ÜÃâ»¥Ïàµş¼Óµ¼ÖÂ¿´²»³ö¸÷×ÔĞÎ×´¡£
	static int aFxSelfTest = 0;
	if (++aFxSelfTest % 40 == 0)
	{
		int aSlot = (aFxSelfTest / 40) % 4;
		float aX = 200.0f + (float)aSlot * 120.0f;
		float aY = 200.0f + (float)(aSlot % 3) * 80.0f;

		// 1) ÃüÖĞ¹âÔÎ£¨Å¯°×£¬ÖĞµÈ³ß´ç£¬»ºÂıÀ©ÕÅ£©
		TfFxSpawn(TF_FX_GLOW, aX, aY, 40.0f, 1.4f, 24, 150, 255, 220, 140);
		// 2) ¾«Ó¢ÍşÑ¹£¨Àä×Ï£¬½Ï´ó£¬¿ìËÙÀ©ÕÅ£©
		TfFxSpawn(TF_FX_GLOW, aX + 46.0f, aY + 18.0f, 48.0f, 2.6f, 22, 120, 190, 120, 255);
		// 3) ³å»÷²¨ÉÁ°×£¨ÊµĞÄËÄ±ßĞÎ£¬¶Ì´Ù£©
		TfFxSpawn(TF_FX_FLASH, aX + 92.0f, aY, 24.0f, 0.0f, 16, 90, 180, 230, 255);
		// 4) ÔªËØÍÏÎ²£¨¶¾ÂÌ£¬Öğ½¥ÊÕËõ£©
		TfFxSpawn(TF_FX_TRAIL, aX + 132.0f, aY + 26.0f, 18.0f, -0.3f, 12, 130, 40, 220, 40);
	}
#endif
    for (int i = 0; i < gTfFxCount; i++)
    {
        gTfFx[i].mLife--;
        gTfFx[i].mSize += gTfFx[i].mGrow;
    }

    for (int i = 0; i < gTfFxCount; i++)
    {
        if (gTfFx[i].mLife <= 0 && i < gTfFxCount - 1)
        {
            gTfFx[i] = gTfFx[gTfFxCount - 1];
            gTfFxCount--;
            i--;
        }
    }
    if (gTfFxCount > 0 && gTfFx[gTfFxCount - 1].mLife <= 0)
        gTfFxCount--;
}

// °´ÀàĞÍ·Ö×é»­£ºÍ¬Ò»ÀàÓÃÍ¬Ò»ÕÅÌùÍ¼ + Í¬Ò»»ìºÏÄ£Ê½ ¡ú ÏàÁÚÌØĞ§Í¬ÎÆÀí£¬Ö±½Ó³Ôµ½ºÏÅú¡£
static void ThirtyFlagsFxDraw(Sexy::Graphics* g)
{
    if (!TF_FX_ENABLE)   // ¡¾¿ª¹Ø¡¿Ä¬ÈÏ¹Ø±Õ£¬¼û TF_FX_ENABLE
        return;
    if (!ThirtyFlagsMode() || gTfFxCount == 0)
        return;

    for (int aKind = 0; aKind < TF_FX_KIND_COUNT; aKind++)
    {
        Sexy::Image* aImage = (aKind == TF_FX_FLASH) ? Sexy::IMAGE_WHITEPIXEL : Sexy::IMAGE_SPOTLIGHT;
        if (aImage == NULL)
            continue;

        g->SetDrawMode(Sexy::Graphics::DRAWMODE_ADDITIVE);
        g->SetColorizeImages(true);

        for (int i = 0; i < gTfFxCount; i++)
        {
            TfFxQuad& aQuad = gTfFx[i];
            if (aQuad.mKind != aKind)
                continue;

            float aFade = (float)aQuad.mLife / (float)(aQuad.mLifeMax > 0 ? aQuad.mLifeMax : 1);
            if (aFade < 0.0f)
                aFade = 0.0f;
            if (aFade > 1.0f)
                aFade = 1.0f;

            int anAlpha = (int)(aQuad.mAlpha * aFade);
            if (anAlpha <= 0)
                continue;

            int aSize = (int)aQuad.mSize;
            if (aSize < 2)
                continue;

            // ¡¾ĞÔÄÜ¡¿Ã¿Ö¡Ìî³äÔ¤Ëã£º³¬³ö¾ÍÍ£»­£¬
            // ±£Ö¤×î»µÖ¡¿ÉÔ¤²â£¨¡¾0ËµµÄ¾ÍÊÇÕâ¸ö½ÌÑµ£©¡£
            if (gTfFxFillUsed + aSize * aSize > TF_FX_FILL_BUDGET)
                break;
            gTfFxFillUsed += aSize * aSize;

            g->SetColor(Sexy::Color(aQuad.mR, aQuad.mG, aQuad.mB, anAlpha));
            g->DrawImage(aImage, (int)(aQuad.mX - aSize * 0.5f), (int)(aQuad.mY - aSize * 0.5f), aSize, aSize);
        }

        g->SetDrawMode(Sexy::Graphics::DRAWMODE_NORMAL);
        g->SetColorizeImages(false);
    }
}

void ThirtyFlagsFlagChanged(Board* theBoard)
{
    if (!theBoard)
        return;

    // ¡¾±íÏÖ²ã¡¿»»ÆìË²¼äÈ«ÆÁÉÁÒ»ÏÂ£¨¼ÓÉ«°×¡¢µÍ alpha¡¢¶Ì´Ù£©
    TfFxSpawn(TF_FX_FLASH, BOARD_WIDTH * 0.5f, BOARD_HEIGHT * 0.5f,
        BOARD_WIDTH * 1.4f, 0.0f, 16, 60, 255, 240, 210);

    ThirtyFlagsSetupBoard(theBoard);

    // ¡¾ÈıÊ®Æì¡¿ĞÂ½âËøĞĞµÄĞ¡ÍÆ³µÓÉ ThirtyFlagsUpdateSod ÔÚ½âËøÄÇÒ»Ö¡²¹·Å£¨¼û TFSpawnLawnMower£©£¬ÕâÀï²»ÔÙÖØ¸´´¦Àí
    // ÕûÕÅ 180 ²¨±íÔÚ¹Ø¿¨¿ªÊ¼Ê±ÓÉ Board::PickZombieWaves Ò»´Î½¨³É
    // £¨Ã¿²¨°×Ãûµ¥°´¸Ã²¨ËùÊôÆì´ÎÈ¡£©£¬»»ÆìÖ»Ğè°Ñ¡¸µ±Ç°²¨¡¹ÍÆ½øµ½ÏÂÒ»ÃæÆìµÄÆğµã¡£
    theBoard->mCurrentWave = (gThirtyFlags.mFlag - 1) * TF_WAVES_PER_FLAG;
    {
        char aBuf[64];
        sprintf(aBuf, "FlagChanged -> flag %d (rows=%d)", gThirtyFlags.mFlag, gThirtyFlags.mUnlockedRows);
        TFLog(aBuf);
    }
    // ¡¾ÈıÊ®Æì¡¿»»ÆìÖĞÑë´ó×Ö£¨7.7 »¨»î£©
    ThirtyFlagsShowCenterText(
        StrFormat(_S("------ µÚ %d ÃæÆì ------"), gThirtyFlags.mFlag),
        Sexy::Color(255, 60, 30), 180);
    theBoard->mZombieCountDown = 1800;
    theBoard->mZombieCountDownStart = theBoard->mZombieCountDown;
    theBoard->mZombieHealthToNextWave = -1;
    theBoard->mZombieHealthWaveStart = 0;

    // ¡¾ÈıÊ®Æì¡¤Äù˜„¡¿±¾Æì¿ªÊ¼£º¸´»îÉÏÒ»ÆìÕóÍöµÄÖ²Îï
    if (gThirtyFlags.GetStacks(TF_UPG_NIRVANA) > 0 && gTFDeadCount > 0)
    {
        for (int k = 0; k < gTFDeadCount; k++)
        {
            int aCol = gTFDeadCol[k], aRow = gTFDeadRow[k];
            if (aCol < 0 || aCol >= MAX_GRID_SIZE_X || aRow < 0 || aRow >= MAX_GRID_SIZE_Y)
                continue;
            if (!gThirtyFlags.IsRowUnlocked(aRow))
                continue;
            if (theBoard->GetTopPlantAt(aCol, aRow, TOPPLANT_ONLY_NORMAL_POSITION))
                continue;
            theBoard->AddPlant(aCol, aRow, (SeedType)gTFDeadSeed[k], SeedType::SEED_NONE);
        }
        gTFDeadCount = 0;
    }

}

void ThirtyFlagsIntermissionBegin(Board* theBoard)
{
    TFLog("intermission BEGIN");
    gThirtyFlags.mIntermission = true;
    gThirtyFlags.mUpgradeChosen = false;
    gThirtyFlags.mUpgradeDialogShown = false;
    gThirtyFlags.mPrepTimeStart = TF_PREP_TIME;
    gThirtyFlags.mPrepTimeLeft = TF_PREP_TIME;
    gThirtyFlags.mSunAtFlagStart = theBoard ? theBoard->mSunMoney : 0;

    if (gThirtyFlags.mFlag % 5 == 0)
    {
        gThirtyFlags.RollMutation();
    }

    gThirtyFlags.RollUpgradeChoices();

    if (theBoard)
    {
        int aStacks = gThirtyFlags.GetStacks(TF_UPG_COMPOUND_SUN);
        if (aStacks > 0)
        {
            int aBonus = (int)((float)theBoard->mSunMoney * 0.15f * (float)aStacks);
            if (aBonus > 0)
            {
                theBoard->AddSunMoney(aBonus);
            }
        }
    }
}

void ThirtyFlagsAdvanceFlag(Board* theBoard)
{
    {
        char aBuf[128];
        sprintf(aBuf, "[TFMower] AdvanceFlag: stage=%d -> flag %d",
            theBoard && theBoard->mChallenge ? theBoard->mChallenge->mSurvivalStage : -1,
            ThirtyFlagsCurrentFlag(theBoard));
        TFLog(aBuf);
    }
    // Î¨Ò»Êı¾İÔ´ÊÇ mChallenge->mSurvivalStage + 1£»²»ÄÜÓÃ gThirtyFlags.mFlag£¬·ñÔòÓÃ Tab Ö±½ÓÌøÆìÊ±Á½ÕßÍÑ½Ú£¬³¡µØ»á°´´íÆì´ÎÆÌ¿ª¡£
    gThirtyFlags.ApplyFlag(ThirtyFlagsCurrentFlag(theBoard));
    // ¡¾ÈıÊ®Æì¡¿ĞŞ¸´£ºÕâÀï²»ÄÜ¹Ø mIntermission ¡ª¡ª CheckForGameEnd µÄË³ĞòÊÇ
    // Begin -> AdvanceFlag£¬Begin ¸ÕÖÃ true ¾Í±»ÕâÀïÄ¨µô£¬µ¯´°Ìõ¼şÓÀÔ¶Îª¼Ù¡£
    // intermission ¸ÄÓÉ TFDialog Ñ¡¿¨»Øµ÷½áÊø¡£

    if (theBoard)
    {
        theBoard->AddSunMoney(TF_STARTING_SUN);
    }

    ThirtyFlagsFlagChanged(theBoard);
}

void ThirtyFlagsIntermissionFinish(Board* theBoard)
{
    if (theBoard && gThirtyFlags.mPrepTimeLeft > 0 && gThirtyFlags.mPrepTimeStart > 0)
    {
        int aSun = (int)((float)TF_EARLY_SUN_MAX * (float)gThirtyFlags.mPrepTimeLeft / (float)gThirtyFlags.mPrepTimeStart);
        if (aSun > 0)
        {
            theBoard->AddSunMoney(aSun);
        }
    }

    gThirtyFlags.mPrepTimeLeft = 0;
}

float ThirtyFlagsZombiePointsScale(Board* theBoard)
{
    if (!ThirtyFlagsMode())
        return 1.0f;

    return gThirtyFlags.GetCountScale();
}

int ThirtyFlagsPickZombieWaves(Board* theBoard, int& theWaveCount)
{
    if (!theBoard || gLawnApp->mGameMode != GAMEMODE_THIRTY_FLAGS)
        return 0;

    // ×Ô¾Ù£ºBoard::InitLevel µÄµ÷ÓÃË³ĞòÊÇ PickBackground -> InitZombieWaves(ÄÚ²¿»áµ÷±¾º¯Êı)
    // -> ThirtyFlagsInitRun£¬Òò´Ë±¾º¯Êı¿ÉÄÜÔçÓÚ InitRun Ö´ĞĞ£»´ËÊ±ÏÈ²¹Ò»´Î StartRun£¬
    // ·ñÔò mActive ÈÔÎª false£¬²¨´Î½Ó¹Ü»á±»Ìø¹ı¡¢mNumWaves ±£³ÖÎ´³õÊ¼»¯Öµ¡£
    if (!gThirtyFlags.mActive)
    {
        gThirtyFlags.StartRun();
    }

    theWaveCount = TF_TOTAL_FLAGS * TF_WAVES_PER_FLAG;
    return 1;
}

// ----------------------------------------------------------------------------------------------------
// ½©Ê¬²à£º³õÊ¼»¯£¨ÊıÖµ³É³¤ + ¾«Ó¢´ÊÌõ£©
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsZombieInit(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return;

    if (!theZombie->IsOnBoard())
        return;

    // Final boss: shared three-phase health pool (plan 8.1).
    if (theZombie->mZombieType == ZOMBIE_BOSS)
    {
        theZombie->mBodyHealth = TF_BOSS_HEALTH;
        theZombie->mBodyMaxHealth = TF_BOSS_HEALTH;
        return;
    }

    float aHealthScale = gThirtyFlags.GetHealthScale();
    if (theZombie->mBodyHealth > 0)
    {
        theZombie->mBodyHealth = (int)((float)theZombie->mBodyHealth * aHealthScale);
        theZombie->mBodyMaxHealth = theZombie->mBodyHealth;
    }
    if (theZombie->mHelmHealth > 0)
    {
        theZombie->mHelmHealth = (int)((float)theZombie->mHelmHealth * aHealthScale);
        theZombie->mHelmMaxHealth = theZombie->mHelmHealth;
    }
    if (theZombie->mShieldHealth > 0)
    {
        theZombie->mShieldHealth = (int)((float)theZombie->mShieldHealth * aHealthScale);
        theZombie->mShieldMaxHealth = theZombie->mShieldHealth;
    }
    if (theZombie->mFlyingHealth > 0)
    {
        theZombie->mFlyingHealth = (int)((float)theZombie->mFlyingHealth * aHealthScale);
        theZombie->mFlyingMaxHealth = theZombie->mFlyingHealth;
    }

    theZombie->mVelZ = 0.0f;
    theZombie->mBossMode = 0;
    // ¡¾ÈıÊ®Æì¡¿ÆìÖÄ½©Ê¬ = Õ½ÕùÖ®Íõ£¨ÓÃ»§ĞèÇó¸Ä°æ£©£ºÈı¼şÌ×È«²¿×ßÔ­°æÔ­Éú·À¾ßÏµÍ³ ¡ª¡ª
    // ¸ß¼á¹ûÂ­¼×£¨±¾Ìå +4000£¬Í·»»¸ß¼á¹ûÁÑÎÆÁ³£©£»ÌúÍ°£¨Ô­Éú helm£º°¼Ïİ/»÷·É¶¯»­£©£»
    // ÌúÃÅ£¨Ô­Éú shield£ºµ¯¿×/ÆÆËğ¶¯»­£©¡£Íâ¼ÓÕ½Æì¹â»·ÓëËÀÍöÕÙ±ø£¨Ö¸»Ó¹Ù»úÖÆ£©¡£
    if (theZombie->mZombieType == ZombieType::ZOMBIE_FLAG)
    {
        // ¸ß¼á¹û»¤¼×£¨ÓÃ»§¿Ú¾¶£º= ¸ß¼á¹ûÄÇ 8000 Ñª£©£ºÖ±½Ó¸ø×ã 8000£¬
        // Ö®Ç°ÊÇ¡¸±¾Ìå x2 ÔÙ +4000¡¹= 5000£¬Êµ²âÆ«µÍ¡£
        theZombie->mBodyHealth = TF_FLAG_ARMOR_TALLNUT;
        theZombie->mBodyMaxHealth = TF_FLAG_ARMOR_TALLNUT;
        // éÏé­ÇòÃ±·À¾ß£ºÔ­Éú helm ÏµÍ³£¨ÓÃ»§ĞèÇó£ºÌúÍ°»»³ÉéÏé­ÇòÍ·¿ø£©
        theZombie->mHelmType = HelmType::HELMTYPE_FOOTBALL;
        // Ô­°æéÏé­Çò½©Ê¬µÄÍ·¿ø¾ÍÊÇ 1400£¨¼û Zombie.cpp µÄ ZOMBIE_FOOTBALL ³õÊ¼»¯£©£¬²»ÊÇ 1700+
        theZombie->mHelmHealth = TF_FLAG_ARMOR_HELMET;
        theZombie->mHelmMaxHealth = TF_FLAG_ARMOR_HELMET;
        // ÌúÃÅ·À¾ß£ºÔ­Éú shield ÏµÍ³£¨ÊÜ»÷µ¯¿×/ÆÆËğÖ¡£©
        theZombie->mShieldType = ShieldType::SHIELDTYPE_DOOR;
        theZombie->mShieldHealth = 1100;
        Reanimation* aFlagReanim = theZombie->mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
        if (aFlagReanim)
        {
            aFlagReanim->SetImageOverride("anim_head1", Sexy::IMAGE_REANIM_TALLNUT_CRACKED1);
            theZombie->ReanimShowPrefix("anim_screen_door", RENDER_GROUP_HIDDEN);
        }
        // ¡¾ÈıÊ®Æì¡¿ÌúÃÅ»»Æ¤Ğ¡ÍÆ³µ£¨ÓÃ»§ĞèÇó£©£ºÃÅ¹ìµÀÒş²Ø£¬ÊÖ³ÖÎ»¹ÒÔ­°æĞ¡ÍÆ³µ reanim
        //£¨AddAttachedReanim Í¬²øÈÆº£²İ¹ÒÌÙÂû»úÖÆ£¬¸½¼şËæ½©Ê¬ÒÆ¶¯/Ïú»Ù£©
        Reanimation* aMowerReanim = theZombie->AddAttachedReanim(-35, 10, ReanimationType::REANIM_LAWNMOWER);
        if (aMowerReanim)
        {
            aMowerReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
            aMowerReanim->mAnimRate = 12.0f;
        }
    }

    if (Rand(100) >= gThirtyFlags.GetEliteChancePercent())
        return;

    int aWanted = 1 + Rand(TF_ELITE_MAX);
    for (int i = 0; i < aWanted; i++)
    {
        TFSetElite(theZombie, Rand(TF_ELITE_KIND_COUNT));
    }

    if (TFHasElite(theZombie, ELITE_SHIELD))
    {
        theZombie->mBossBungeeCounter = 480;
    }
    if (TFHasElite(theZombie, ELITE_SPEAR))
    {
        theZombie->mBossStompCounter = 300;
    }


    // ¡¾ÈıÊ®Æì¡¿¾«Ó¢½©Ê¬È«Ô±¼ÓÇ¿£¨ÓÃ»§ĞèÇó£©£ºÑªÁ¿ +60%
    theZombie->mBodyHealth = (int)(theZombie->mBodyHealth * 1.6f);
    theZombie->mBodyMaxHealth = theZombie->mBodyHealth;

    // ¡¾ÈıÊ®Æì¡¿¾«Ó¢³öÃ»¾¯¸æ£¨7.7 »¨»î£©£ºÀäÈ´ 10 Ãë£¬±ÜÃâË¢ÆÁ
    {
        static int aLastWarn = -10000;
        if (theZombie->mBoard && theZombie->mBoard->mMainCounter - aLastWarn > 600)
        {
            aLastWarn = theZombie->mBoard->mMainCounter;
            ThirtyFlagsShowCenterText(SexyString(_S("!! ¾«Ó¢³öÃ» !!")), Sexy::Color(255, 40, 20), 150);
        }
    }

    // ¡¾ÈıÊ®Æì¡¿¾«Ó¢ÊÓ¾õ±íÏÖ£¨ÓÃ»§ĞèÇó£©£º°´Ö÷´ÊÌõ¸ø½©Ê¬È¾·¢¹âÉ« ¡ª¡ª Ô¶¿´Ò»ÑÛÊÇ¾«Ó¢
    //£¨Ô­°æÎŞÏÖ³É"»»Í·"×ÊÔ´¿É¸´ÓÃ£¬ÓÃ reanim ¼ÓÉ«µş¼Ó£¬Óë÷È»ó/±ù¶³È¾É«Í¬Ò»»úÖÆ£©
    Reanimation* aEliteReanim = theZombie->mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
    if (aEliteReanim)
    {
        aEliteReanim->mEnableExtraAdditiveDraw = true;
        Sexy::Color aGlow(150, 0, 0);
        if (TFHasElite(theZombie, ELITE_BERSERK))         aGlow = Sexy::Color(170, 0, 0);
        else if (TFHasElite(theZombie, ELITE_IRONWALL))      aGlow = Sexy::Color(0, 70, 180);
        else if (TFHasElite(theZombie, ELITE_SWIFT))      aGlow = Sexy::Color(170, 170, 0);
        else if (TFHasElite(theZombie, ELITE_REGEN)) aGlow = Sexy::Color(0, 150, 0);
        else if (TFHasElite(theZombie, ELITE_SHIELD))     aGlow = Sexy::Color(140, 0, 190);
        else if (TFHasElite(theZombie, ELITE_SPLIT))      aGlow = Sexy::Color(200, 100, 0);
        else if (TFHasElite(theZombie, ELITE_SPEAR))      aGlow = Sexy::Color(0, 150, 160);
        aEliteReanim->mExtraAdditiveColor = aGlow;

        // ¡¾ÈıÊ®Æì¡¿¾«Ó¢»»Í·£¨ÓÃ»§ĞèÇó£©£º°´Ö÷´ÊÌõ»»Í· ¡ª¡ª ¿ñ±©=¿ñÌ¬Å¤ÇúÍ·£¬ÆäÓà°´´ÊÌõÅäÄ«¾µ 1-4 ºÅ¡£
        // ÆìÖÄ½©Ê¬£¨Õ½ÕùÖ®Íõ£©±£Áô¸ß¼á¹ûÁÑÎÆÍ·£¬²»ÔÚ´Ë¸²¸Ç¡£
        if (theZombie->mZombieType != ZombieType::ZOMBIE_FLAG)
        {
            Sexy::Image* aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES4;
            if (TFHasElite(theZombie, ELITE_BERSERK))       aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_GROSSOUT;
            else if (TFHasElite(theZombie, ELITE_SWIFT))    aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES1;
            else if (TFHasElite(theZombie, ELITE_IRONWALL)) aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES2;
            else if (TFHasElite(theZombie, ELITE_REGEN))    aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES3;
            aEliteReanim->SetImageOverride("anim_head1", aHeadImg);
        }
    }
}

int ThirtyFlagsZombieTakeDamage(Zombie* theZombie, int theDamage, unsigned int theDamageFlags)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return theDamage;

    if (theZombie->mZombieType == ZOMBIE_BOSS)
        return theDamage;

    // ¡¾±íÏÖ²ã¡¿ÃüÖĞ¹âÔÎ£ºÖ»¶Ô"´òµÃ¶¯"µÄÉËº¦·Å£¬ÇÒÔ¼ 1/3 ¸ÅÂÊ£¬±ÜÃâÌØĞ§³Ø±»Ë¢ÆÁ
    if (theDamage > 0 && (RandRangeInt(0, 2) == 0))
    {
        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 30.0f,
            34.0f, 0.35f, 12, 90, 255, 240, 170);
    }

    int aDamage = theDamage;

    if (TestBit(theDamageFlags, (int)TF_DAMAGE_FROM_PLANT))
    {
        aDamage = (int)((float)aDamage * gThirtyFlags.GetDamageBonus());
        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ v5¡¿ÆÆÕÀ±ê¼Ç£ºÃ¿²ã +5% ÊÜÉË£¨×î¶à 5 ²ã = +25%£©
        int aMark = ThirtyFlagsGetMark(theZombie, theZombie->mBoard);
        if (aMark > 0)
            aDamage = (int)((float)aDamage * (1.0f + 0.05f * (float)aMark));

        float aCritChance = 0.15f * (float)gThirtyFlags.GetStacks(TF_UPG_CRIT) * gThirtyFlags.GetLegendaryAmplifier();
        bool aForceCrit = (gThirtyFlags.GetStacks(TF_UPG_NEURO_CRIT) > 0);
        if (aForceCrit || (aCritChance > 0.0f && Rand(10000) < (int)(aCritChance * 10000.0f)))
        {
            float aCritMul = 2.0f + (float)gThirtyFlags.GetStacks(TF_UPG_NEURO_CRIT);
            aDamage = (int)((float)aDamage * aCritMul);
        }
    }

    float aPierce = gThirtyFlags.GetArmorPierce();
    if (aPierce > 0.0f)
    {
        aDamage = (int)((float)aDamage * (1.0f + min(aPierce, 1.0f)));
    }

    if (TFHasElite(theZombie, ELITE_IRONWALL))
    {
        aDamage = (int)((float)aDamage * 0.6f);
    }

    float aReduction = gThirtyFlags.GetZombieDamageReduction();
    if (aReduction > 0.0f)
    {
        aDamage = (int)((float)aDamage * (1.0f - aReduction));
    }

    return max(1, aDamage);
}

void ThirtyFlagsZombieKilled(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || !theZombie->mBoard)
        return;

    if (theZombie->mDead)
        return;

    // ¡¾±íÏÖ²ã¡¿»÷É±³å»÷²¨£ºÏÈÀ´Ò»È¦´ó¶øÂıµÄ°×¹â£¬ÔÙµşÒ»²ãÅ¯É«ÓàêÍ
    TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 35.0f,
        40.0f, 3.2f, 16, 120, 255, 235, 200);
    TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 35.0f,
        20.0f, 1.6f, 24, 70, 255, 170, 90);

    // ¡¾ÈıÊ®Æì¡¿Õû±¸ÆÚ¼ä£¨»»ÆìÇå³¡£©µÄËÀÍö²»¼Æ»÷É±£º·ñÔò²ĞÓà½©Ê¬µÄ DieNoLoot
    // »á¹à±¬Á¬É±Êı¡¢¸ÅÂÊµôÑô¹â£¨ÓÃ»§·´À¡¡°»»ÆìÄªÃû¶à³öºÜ¶àÁ¬É±¡±£©
    if (gThirtyFlags.mIntermission)
        return;


    int aRow = theZombie->mRow;

    if (ThirtyFlagsBossIsDomainActive())
    {
        int aCol = ClampInt((int)(theZombie->mPosX - BOARD_OFFSET) / 80, 0, MAX_GRID_SIZE_X - 1);
        TFAddPoison(aCol, aRow, 300);
    }

    if (TFHasElite(theZombie, ELITE_SPLIT))
    {
        for (int i = 0; i < 2; i++)
        {
            Zombie* aImp = theZombie->mBoard->AddZombieInRow(ZOMBIE_IMP, aRow, theZombie->mBoard->mCurrentWave);
            if (aImp)
            {
                aImp->mPosX = theZombie->mPosX + (float)(i * 30);
                aImp->mBodyHealth = max(1, aImp->mBodyMaxHealth / 2);
            }
        }
    }

    // ¡¾ÈıÊ®Æì¡¿´¦¾öÌØĞ§£¨²ß»®°¸ 7.7£©£º»÷É±¾«Ó¢½©Ê¬»ò¾ŞÈË½©Ê¬Ê±£¬ÔÚËÀÍöÎ»ÖÃ·ÅÒ»È¦±¬·¢ÌØĞ§¡£
    bool aGiant = (theZombie->mZombieType == ZOMBIE_GARGANTUAR ||
                   theZombie->mZombieType == ZOMBIE_REDEYE_GARGANTUAR ||
                   theZombie->mZombieType == ZOMBIE_BOSS);
    bool aElite = false;
    for (int aKind = 0; aKind < TF_ELITE_KIND_COUNT; aKind++)
    {
        if (TFHasElite(theZombie, aKind))
        {
            aElite = true;
            break;
        }
    }
    if (aGiant || aElite)
    {
        TFSpawnExecution(theZombie->mBoard, theZombie, aGiant);
    }

    gThirtyFlags.OnZombieKilled(theZombie->mBoard, (int)theZombie->mPosX, (int)theZombie->mPosY, aRow);
}

void ThirtyFlagsZombieAtePlant(Zombie* theZombie, Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return;

    // ·´ÊÉÍ³Ò»×ß TakeDamage ½áËã£¬ËùÒÔÌú±Ú/Ê¬¼×µÈ¼õÉË»á×Ô¶¯ÕıÈ·ÉúĞ§¡£
    int aThorns = 0;

    // ¡¸¸ß¼á¹û·´ÉË¡¹£¨²ß»®°¸ 6.2£©£º¹Ì¶¨·´ÊÉÉËº¦
    if (thePlant && thePlant->mSeedType == SeedType::SEED_TALLNUT)
    {
        aThorns += TF_TALLNUT_THORNS;
    }

    // ¡¸¾£¼¬¡¹Ç¿»¯£¨²ß»®°¸ 5.4£©£º°´½©Ê¬×î´óÉúÃü°Ù·Ö±È·´µ¯¡£
    // Ô­°æ GetThornsReflect() ¶¨ÒåÁËÈ´´ÓÎ´±»µ÷ÓÃ£¬ÊÇËÀ´úÂë£¬ÕâÀï½ÓÉÏ¡£
    float aReflect = gThirtyFlags.GetThornsReflect();
    if (aReflect > 0.0f)
    {
        aThorns += (int)((float)theZombie->mBodyMaxHealth * aReflect);
    }

    if (aThorns > 0)
    {
        theZombie->TakeDamage(aThorns, 0);
    }

    // ¡¾ÈıÊ®Æì¡¿½©?P3¡¸Ê¬ÍõÁìÓò¡¹£ºÁìÓòÆÚ¼äÈ«Ìå½©Ê¬¿ĞÊ³¾ùÎüÑª?
    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¸¯»¯ÆÚ¼äÍ¬ÑùÒÖÖÆÍ»±ä¡¸ÎüÑª¡¹ÓëÊ¬ÍõÁìÓòÎüÑª
    if ((gThirtyFlags.HasMutation(MUTATION_LIFESTEAL) || ThirtyFlagsBossIsDomainActive()) &&
        ThirtyFlagsGetCorrupt(theZombie, theZombie->mBoard) == 0)
    {
        float aRate = ThirtyFlagsBossIsDomainActive() ? 0.04f : 0.02f;
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * aRate));
        theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);
    }
}

bool ThirtyFlagsZombieIsBerserk(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || theZombie->mZombieType == ZOMBIE_BOSS)
        return false;

    if (!TFHasElite(theZombie, ELITE_BERSERK))
        return false;

    return (theZombie->mBodyHealth * 10 < theZombie->mBodyMaxHealth * 3);
}

void ThirtyFlagsZombieUpdate(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || theZombie->mZombieType == ZOMBIE_BOSS)
        return;

    if (theZombie->IsDeadOrDying() || !theZombie->IsOnBoard())
        return;

    // ¡¾ÈıÊ®Æì¡¿ÆìÖÄ½©Ê¬¡¸Õ½ÕùÖ®Íõ¡¹£º±ß¿¸Æì±ß¿ªÇ¹¡£
    //
    // ×Ô¼ºÊµÏÖ£¬²»ÔÙ½èÓÃ UpdateZombieGatlingHead()£ºÄÇ¸öº¯Êı»á¶Ô
    // mSpecialHeadReanimID µ÷ PlayReanim("anim_shooting"/"anim_head_idle")£¬
    // ¶øÆìÖÄ½©Ê¬µÄ"ÌØÊâÍ·²¿"ÊÇÆì×Ó£¨REANIM_FLAG£©£¬Ã»ÓĞÕâÁ½Ìõ¶¯»­£»
    // Ò»µ©¸Ã id Ê§Ğ§£¨REANIMATIONID_NULL == 0£©¾Í»á ReanimationGet(0)
    // ¡ú ¶ÏÑÔ + ¿ÕÖ¸Õë·ÃÎÊ£¨Êµ²âµÚ 18 Æì±ÀÀ££©¡£
    if (theZombie->mZombieType == ZombieType::ZOMBIE_FLAG)
    {
        if (theZombie->mPhaseCounter <= 0)
        {
            theZombie->mPhaseCounter = TF_FLAG_ZOMBIE_CYCLE;
        }

        // Óë»úÇ¹Íã¶¹Í·Í¬ÑùµÄ½Ú×à£ºÒ»ÂÖ 150 Ö¡´ò 4 ·¢£¬Ã¿·¢¼ä¸ô 17 Ö¡
        int aC = theZombie->mPhaseCounter;
        if (aC == TF_FLAG_ZOMBIE_CYCLE || aC == TF_FLAG_ZOMBIE_CYCLE - 17 ||
            aC == TF_FLAG_ZOMBIE_CYCLE - 34 || aC == TF_FLAG_ZOMBIE_CYCLE - 51)
        {
            Projectile* aFlagPea = theZombie->mBoard->AddProjectile(
                (int)theZombie->mPosX + 20, (int)theZombie->mPosY - 20,
                theZombie->mRenderOrder, theZombie->mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
            if (aFlagPea)
            {
                aFlagPea->mMotionType = ProjectileMotion::MOTION_BACKWARDS;   // ³¯×ó´òÖ²Îï
            }
            theZombie->mApp->PlayFoley(FoleyType::FOLEY_THROW);
        }
    }

    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¾«Ó¢ÔÙÉú£ºÔ­À´ÊÇ % 10 ¡ª¡ª mZombieAge Ã¿Ö¡ +1¡¢ºó¶Ë 100 Ö¡/Ãë£¬
    // 2% ¡Á 10 ´Î/Ãë = Ã¿Ãë»Ø¸´ 20% ×î´óÉúÃü£¬Ô¶³¬Éè¼Æ¸å¡¸Ã¿Ãë 2%¡¹£¬
    // ½á¹ûÈÎºÎµÍ DPS Ö²Îï£¨ÓÇÓô¹½/Ã¨Î²²İ 40 DPS£©¶¼ÍêÈ«´ò²»¶¯¾«Ó¢¡£
    // ÏÖ°´Éè¼Æ¸åĞŞÕıÎªÃ¿Ãë 2%¡£
    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¸¯»¯ÆÚ¼ä½ûÖ¹»Ø¸´£¨ÕâÊÇ¡¸´ò²»¶¯¾«Ó¢¡¹µÄ»úÖÆ½â£©
    // ¡¾±íÏÖ²ã¡¿¾«Ó¢ÍşÑ¹£º½ÅÏÂ³£×¤Ò»È¦ÀäÉ«¹â»·£¬Ã¿¸ôÒ»»á¶ùÏòÍâÀ©Ò»È¦Âö³å
    if ((theZombie->mZombieAge % 30) == 0 && TfFxIsElite(theZombie))
    {
        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY + 12.0f,
            46.0f, 2.2f, 22, 70, 190, 120, 255);
    }
    if ((theZombie->mZombieAge % 7) == 0 && TfFxIsElite(theZombie))
    {
        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY + 14.0f,
            30.0f, 0.0f, 8, 45, 150, 90, 255);
    }

    if (TFHasElite(theZombie, ELITE_REGEN) &&
        ThirtyFlagsGetCorrupt(theZombie, theZombie->mBoard) == 0 &&
        (theZombie->mZombieAge % 100) == 0)
    {
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * 0.02f));
        if (theZombie->mBodyHealth < theZombie->mBodyMaxHealth)
        {
            theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);

        // ¡¾ÈıÊ®Æì¡¤Éñ¸ñº®±ù¡¿¼õËÙĞ§¹û¸½¼ÓÕæÊµÉËº¦£¨´ËÇ°¶¨ÒåÁËÇ¿»¯È´´ÓÎ´½ÓÏß£©
        {
            int aGodIce = gThirtyFlags.GetStacks(TF_UPG_GOD_ICE);
            if (aGodIce > 0 && theZombie->mIceTrapCounter > 0 && (theZombie->mZombieAge % 30) == 0)
            {
                theZombie->TakeDamage(4 * aGodIce, 0U);
            }
        }
        }
    }

    if (TFHasElite(theZombie, ELITE_SHIELD) &&
        ThirtyFlagsGetCorrupt(theZombie, theZombie->mBoard) == 0)
    {
        if (theZombie->mBossBungeeCounter > 0)
        {
            theZombie->mBossBungeeCounter--;
        }
        else
        {
            theZombie->mBossBungeeCounter = 480;
            int aShield = 300 + (int)((float)theZombie->mBodyMaxHealth * 0.10f);
            theZombie->mBodyHealth = min(theZombie->mBodyHealth + aShield, theZombie->mBodyMaxHealth);
        }
    }

    if (TFHasElite(theZombie, ELITE_SPEAR))
    {
        if (theZombie->mBossStompCounter != 0)
        {
            theZombie->mBossStompCounter += (theZombie->mBossStompCounter > 0) ? -1 : 1;
            if (theZombie->mBossStompCounter == 0)
            {
                theZombie->mBossStompCounter = 300;
            }
        }
        else
        {
            Plant* aTarget = theZombie->FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
            if (aTarget)
            {
                theZombie->mBossStompCounter = -15;

                // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Í¶Ã¬Í¬Ñù×ß¡¸ÄÏ¹ÏÓÅÏÈ³ĞÉË¡¹£º×°ÔÚÄÏ¹ÏÀïµÄÖ²Îï²»µôÑª
                Plant* aSpearVictim = TFDirectDamageTarget(theZombie->mBoard, aTarget->mPlantCol, aTarget->mRow);
                if (aSpearVictim)
                {
                    aSpearVictim->mPlantHealth -= TF_ELITE_SPEAR_DAMAGE;
                    if (aSpearVictim->mPlantHealth <= 0)
                    {
                        theZombie->mBoard->mPlantsEaten++;
                        aSpearVictim->Die();
                    }
                }

                int aRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE, theZombie->mRow, 0);
                Projectile* aSpear = theZombie->mBoard->AddProjectile(
                    (int)theZombie->mPosX, (int)theZombie->mPosY + 10, aRenderOrder, theZombie->mRow,
                    PROJECTILE_CACTUS_SPEAR);
                if (aSpear)
                {
                    aSpear->mWidth = 20;
                    aSpear->mHeight = 20;
                    aSpear->mRotation = 0.0f;
                    aSpear->mRotationSpeed = 0.0f;

                    // ¡¾ÈıÊ®Æì¡¿ÈÃÕâ¸ùÃ¬ÕæµÄ¿´µÃ¼û£ºÔ­À´Ã»ÉèÔË¶¯Ä£Ê½£¬×ßµÄÊÇ
                    // ¡¸Ä¬ÈÏÏòÓÒ 3.33/frame¡¹¡ª¡ªÒ²¾ÍÊÇ´Ó½©Ê¬ÉíÉÏ³¯½©Ê¬¶ÑÀï·É£¬
                    // ¶øÇÒ¼¸Ê®Ö¡¾Í³ö½çÏûÊ§ÁË£¬Íæ¼Ò»ù±¾¿´²»µ½£¨Êµ²â·´À¡"Ã»¼û¹ıÕâ¸öÃ¬"£©¡£
                    // ¸Ä³É BACKWARDS£¨Ïò×ó£©£¬³¯×ÅËü¸Õ¸Õ´ÌÖĞµÄÖ²Îï·½Ïò·É³öÈ¥¡£
                    aSpear->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
                }
            }
            else
            {
                theZombie->mBossStompCounter = 60;
            }
        }
    }

}

// ----------------------------------------------------------------------------------------------------
// Ö²Îï²à
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsPlantInit(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return;

    // ---- »ù´¡ÖµÔöÇ¿£¨²ß»®°¸µÚ 6 ÕÂ£©----
    switch (thePlant->mSeedType)
    {
    case SeedType::SEED_WALLNUT:
    case SeedType::SEED_EXPLODE_O_NUT:
    case SeedType::SEED_GIANT_WALLNUT:
        thePlant->mPlantHealth *= 2;          // 4000 -> 8000
        break;
    case SeedType::SEED_TALLNUT:
        thePlant->mPlantHealth *= 2;          // 8000 -> 16000
        break;
    case SeedType::SEED_PUMPKINSHELL:
        thePlant->mPlantHealth *= 2;          // »¤¼× 4000 -> 8000
        break;
    case SeedType::SEED_UMBRELLA:
        thePlant->mPlantHealth *= 2;
        break;
    default:
        break;
    }

    // ---- Í¨ÓÃ±¶ÂÊÍ¨µÀ ----
    if (thePlant->mPlantHealth > 0)
    {
        thePlant->mPlantHealth = (int)((float)thePlant->mPlantHealth * gThirtyFlags.GetHealthBonus());
        thePlant->mPlantMaxHealth = thePlant->mPlantHealth;
    }

    // ¡¸¹²Éú¡¤Ë®Â·¡¹£ºË¯Á«²»ÔÙ¼·Õ¼¿¨²Û
    if (thePlant->mSeedType == SeedType::SEED_LILYPAD && gThirtyFlags.GetStacks(TF_UPG_SYMBIOSIS_WATER) > 0)
    {
        if (thePlant->mBoard && thePlant->mBoard->mSeedBank)
        {
            thePlant->mBoard->mSeedBank->mNumPackets = min(TF_SEED_SLOTS_MAX, thePlant->mBoard->mSeedBank->mNumPackets + 1);
            thePlant->mBoard->mSeedBank->UpdateWidth();
        }
    }

    int aCooldown = gThirtyFlags.GetStacks(TF_UPG_COOLDOWN);
    if (aCooldown > 0 && thePlant->mLaunchRate > 0)
    {
        float aScale = max(0.15f, 1.0f - 0.20f * (float)aCooldown);
        thePlant->mLaunchRate = max(5, (int)((float)thePlant->mLaunchRate * aScale));
    }
    // Ãâ¿§·È¶¹£¨²ß»®°¸ 6.2 / 6.3£©£ºÈıÊ®ÆìÈ«³ÌÁ¬Ğø×÷Õ½£¬Ò¹ĞĞÖ²Îï²»ÔÙË¯¾õ¡£
    // ±ØĞë·ÅÔÚÕâÀï ¡ª¡ª ThirtyFlagsPlantInit ±È PlantInitialize ÀïµÄ
    // "IsNocturnal && !StageIsNight -> SetSleeping(true)" ÍíÖ´ĞĞ£¬ÄÜ¸²¸ÇµôË¯Ãß¡£
    if (Plant::IsNocturnal(thePlant->mSeedType) && thePlant->mIsAsleep)
    {
        thePlant->SetSleeping(false);
    }
    // ¡¾ÈıÊ®Æì¡¿Ğ¡Åç¹½¸ÄÔì£¨ÓÃ»§ĞèÇó£©£ºÖÖÏÂ 5 ÃëºóÏûÊ§²¢²ú³öÒ»¸öĞ¡Ñô¹â¡£
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        thePlant->mPuffLife = 300;   // 300 Ö¡ = 5 Ãë£¨×¨Êô³ÉÔ±£¬ÎğÓÃ mWakeUpCounter£©
    }
    // ¡¾ÈıÊ®Æì¡¿¡¸±©ÁÒÖÖ×Ó¡¹£ºÖ²ÎïÖÖÏÂÁ¢¼´¿ª»ğ£¨ÉäÊÖÀà mLaunchCounter ¹éÁã¼´´¥·¢Ê×Éä£©
    if (gThirtyFlags.GetStacks(TF_UPG_BURST_SEED) > 0 && thePlant->mLaunchRate > 0)
    {
        thePlant->mLaunchCounter = 0;
    }

    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ v5¡¿µşÖÖÊÓ¾õ´íÎ»£ºÍ¬¸ñÒÑÓĞÖ²ÎïÊ±Ïò×óÆ«ÒÆ 6px ¡ª¡ª
    // Ô­°æÍ¬¸ñµÚ¶şÖê»áÓëµÚÒ»ÖêÍêÈ«ÖØµş£¨¿´²»³öµşÖÖÉúĞ§£©£¬´í¿ªºóÁ½Öê¶¼¿É±æ¡£
    if (thePlant->mBoard && thePlant->IsOnBoard())
    {
        Plant* aScanPlant = nullptr;
        while (thePlant->mBoard->IteratePlants(aScanPlant))
        {
            if (aScanPlant != thePlant && aScanPlant->mPlantCol == thePlant->mPlantCol &&
                aScanPlant->mRow == thePlant->mRow && aScanPlant->mPlantHealth > 0)
            {
                thePlant->mX -= 6.0f;
                break;
            }
        }
    }
}

float ThirtyFlagsPlantLaunchDecrement(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 1.0f;

    float aSpeed = gThirtyFlags.GetAttackSpeedBonus();

    if (thePlant->IsOnBoard() && thePlant->mBoard)
    {
        Plant* aUnder = thePlant->mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, TOPPLANT_ONLY_UNDER_PLANT);
        if (aUnder && (aUnder->mSeedType == SeedType::SEED_LILYPAD || aUnder->mSeedType == SeedType::SEED_FLOWERPOT))
        {
            aSpeed *= (1.0f + gThirtyFlags.GetLilypadSpeedBonus());
        }
    }

    // ²ß»®°¸ 6.3£ºĞ¡Åç¹½¹¥ËÙ x3
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aSpeed *= 3.0f;
    }

    // ¡¾ÈıÊ®Æì¡¿Í¶ÖÀ¼Ò×å¹¥ËÙ x1.5£¨ÓëÉËº¦¼Ó³ÉÅäÌ×£¬ÈÃÍ¶ÖÀÏµÓĞĞ¯´ø¼ÛÖµ£©
    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ v5¡¿Í¶ÖÀÏµÊ±Ì¬²ğ·Ö£º¾íĞÄ²Ë = ¸ßÆµĞ¡½¦Éä£¨¿ìĞ¡£©£¬
    // Î÷¹Ï/±ù¹Ï = µÍÆµÖØÅÚ£¨±£³Ö 1.5 Ö±µ½Æä½¦ÉäÇ¿»¯ÂÖÂäµØ£©
    if (thePlant->mSeedType == SeedType::SEED_CABBAGEPULT)
    {
        aSpeed *= 2.0f;
    }
    else if (thePlant->mSeedType == SeedType::SEED_KERNELPULT ||
             thePlant->mSeedType == SeedType::SEED_MELONPULT ||
             thePlant->mSeedType == SeedType::SEED_WINTERMELON)
    {
        aSpeed *= 1.5f;
    }
    return aSpeed;
}

int ThirtyFlagsPlantShotBonus(int theSeedType)
{
    if (!ThirtyFlagsMode())
        return 0;

    switch ((SeedType)theSeedType)
    {
    case SeedType::SEED_PEASHOOTER:     return TF_SHOT_BONUS_PEA;
    case SeedType::SEED_SNOWPEA:        return TF_SHOT_BONUS_PEA;
    case SeedType::SEED_REPEATER:       return TF_SHOT_BONUS_REPEATER;
    case SeedType::SEED_GATLINGPEA:     return TF_SHOT_BONUS_REPEATER;
    case SeedType::SEED_THREEPEATER:    return TF_SHOT_BONUS_THREEPEA;
    case SeedType::SEED_SPLITPEA:       return TF_SHOT_BONUS_SPLITPEA;
    case SeedType::SEED_STARFRUIT:      return TF_SHOT_BONUS_STARFRUIT;
    case SeedType::SEED_CACTUS:         return TF_SHOT_BONUS_CACTUS;
    // ¡¾ÈıÊ®Æì¡¿Í¶ÖÀ¼Ò×åÇ¿»¯£¨ÓÃ»§ĞèÇó£ºÉä»÷ÏµÓĞ»úÇ¹+»ğ¾æ£¬Í¶ÖÀÏµÒªÓĞ´øµÄ±ØÒª£©£º
    // ¾íĞÄ²Ë 40->70 / ÓñÃ× 20->45 / Î÷¹Ï 80->130 / ±ù¹Ï 80->120£¨¶îÍâÉËº¦×ß DoImpact ¹«¹²Â·¾¶²¹½áËã£©
    case SeedType::SEED_CABBAGEPULT:    return 55;   // v5-flat: 40 -> 95
    case SeedType::SEED_KERNELPULT:     return 45;   // v5-flat: 20 -> 65
    case SeedType::SEED_MELONPULT:      return 85;   // v5-flat: 80 -> 165
    case SeedType::SEED_WINTERMELON:    return 70;   // v5-flat: 80 -> 150
    default:                            return 0;
    }
}

// -----------------------------------------------------------------------------------------
// 7.7 ±íÏÖ²ã£ºÉËº¦Ìø×Ö + Á¬É±·´À¡
// -----------------------------------------------------------------------------------------
namespace
{
struct TFDamageNumber
{
    float   mX;
    float   mY;
    int     mValue;
    int     mLife;
    int     mColorKind;   // 0ÆÕÍ¨ 1±ù 2¶¾ 3»ğ 4±©»÷½ğ
};



// ¡¾ÈıÊ®Æì¡¿ÖĞÑë¹«¸æ£¨7.7 »¨»î£©£º»»Æì´ó×Ö / ¾«Ó¢³öÃ»¾¯¸æ£¬¾ÓÖĞ´ó×Öµ­³ö
static int          gTFCenterLife = 0;
static int          gTFCenterLifeMax = 1;
static SexyString   gTFCenterText;
static Sexy::Color  gTFCenterColor(255, 255, 255);


TFDamageNumber  gTFNumbers[TF_DAMAGE_NUM_MAX];
int             gTFNumberCount = 0;

int             gTFStreakShow   = 0;    // Á¬É±ÌõÊ£ÓàÏÔÊ¾Ö¡
int             gTFStreakValue  = 0;    // ¿ìÕÕÏÂÀ´µÄÁ¬É±Êı
int             gTFStreakSeen   = 0;    // ÉÏÒ»Ö¡¿´µ½µÄÁ¬É±Êı£¬ÓÃÓÚ¼ì²â±ä»¯

// ¡¾ÈıÊ®Æì¡¿´¦¾ö±¬·¢»·£¨²ß»®°¸ 7.7£©
struct TFBurstRing
{
    float   mX;
    float   mY;
    int     mLife;
    bool    mBig;          // ¾ŞÈË / ½©Íõ£º¸ü´ó¸üÖØ
};

TFBurstRing     gTFBursts[TF_BURST_MAX];
int             gTFBurstCount   = 0;

// ¡¾ÈıÊ®Æì¡¿¶ÙÖ¡£¨²ß»®°¸ 7.7£©£ºÖ»¶³½á±íÏÖ²ãÍÆ½ø£¬²»¸ÉÔ¤ Board::Update Ê±Ğò
int             gTFFreezeFrames = 0;
int             gTFHitFlash     = 0;

// ¡¾ÈıÊ®Æì¡¿Ñô¹â¹ö¶¯£¨²ß»®°¸ 7.7£©
int             gTFSunSeen      = -1;
int             gTFSunGain      = 0;
int             gTFSunRollValue = 0;
int             gTFSunRollLife  = 0;

// ¡¾ÈıÊ®Æì¡¿±íÏÖ²ãÖ¡¼ÆÊı£¨ºôÎü¸ßÁÁÓÃ£©
int             gTFTick         = 0;

void TFPushBurst(float theX, float theY, bool theBig)
{
    int aSlot;
    if (gTFBurstCount < TF_BURST_MAX)
    {
        aSlot = gTFBurstCount++;
    }
    else
    {
        aSlot = 0;
        for (int i = 1; i < TF_BURST_MAX; i++)
        {
            if (gTFBursts[i].mLife < gTFBursts[aSlot].mLife)
                aSlot = i;
        }
    }

    gTFBursts[aSlot].mX = theX;
    gTFBursts[aSlot].mY = theY;
    gTFBursts[aSlot].mLife = TF_BURST_LIFE;
    gTFBursts[aSlot].mBig = theBig;
}

// ÓÃÏß¶Î½üËÆ»­Ò»¸öÔ²£¨Graphics ÎŞÏßÈ¦½Ó¿Ú£©£¬µ±×÷³å»÷²¨ / ±¬·¢»·
void TFDrawRing(Sexy::Graphics* g, int theCX, int theCY, float theRadius, const Sexy::Color& theColor)
{
    const int aSegments = 28;
    g->SetColor(theColor);

    float aPrevX = (float)theCX + theRadius;
    float aPrevY = (float)theCY;
    for (int i = 1; i <= aSegments; i++)
    {
        float aAngle = (float)i * 6.2831853f / (float)aSegments;
        float aX = (float)theCX + theRadius * cosf(aAngle);
        float aY = (float)theCY + theRadius * sinf(aAngle);
        g->DrawLine((int)aPrevX, (int)aPrevY, (int)aX, (int)aY);
        aPrevX = aX;
        aPrevY = aY;
    }
}
}

// ¡¾ÈıÊ®Æì¡¿´¦¾öÌØĞ§£¨²ß»®°¸ 7.7£©£º±¬·¢»· + Á£×Ó + ÕğÆÁ + ¶ÙÖ¡£¬È«²¿¸´ÓÃÔ­°æ×ÊÔ´¡£
static void TFSpawnExecution(Board* theBoard, Zombie* theZombie, bool theGiant)
{
    if (!theBoard || !theZombie)
        return;

    int aX = (int)theZombie->mPosX + 20;
    int aY = (int)theZombie->mPosY;
    int aRow = theZombie->mRow;

    TFPushBurst((float)aX, (float)aY, theGiant);

    if (gLawnApp)
    {
        int aRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_TOP, aRow, 0);
        gLawnApp->AddTodParticle((float)aX, (float)aY, aRenderOrder,
            theGiant ? ParticleEffect::PARTICLE_JACKEXPLODE : ParticleEffect::PARTICLE_STARBURST);
    }

    // ÕğÆÁ£º¾ŞÈË¸üÖØ£¨¸´ÓÃÔ­°æ Board::ShakeBoard£©
    theBoard->ShakeBoard(theGiant ? 4 : 2, theGiant ? -6 : -3);

    // ¶ÙÖ¡£º¾ŞÈË 10 Ö¡¡¢¾«Ó¢ 6 Ö¡¡£Ö»¶³½á±íÏÖ²ãÍÆ½ø£¬²»¶¯ Board::Update µÄÊ±ĞòÂß¼­¡£
    gTFFreezeFrames = min(TF_FREEZE_MAX_FRAMES, theGiant ? 10 : 6);
    gTFHitFlash = theGiant ? 4 : 0;      // °×ÉÁ½ö¾ŞÈË£¬±ÜÃâºóÆÚ¾«Ó¢»÷É±Æµ·±ÉÁÆÁ
}

// ¡¾ÈıÊ®Æì¡¿¶ÙÖ¡²éÑ¯£¨²ß»®°¸ 7.7£©£ºÓÉµ÷ÓÃ·½¾ö¶¨ÈçºÎÊ¹ÓÃ£¨ÀıÈçÌø¹ı±¾Ö¡µÄ±íÏÖ²ã²åÖµ£©¡£
bool ThirtyFlagsShouldFreezeFrame()
{
    return ThirtyFlagsMode() && gTFFreezeFrames > 0;
}

void ThirtyFlagsAddDamageNumber(int theX, int theY, int theDamage, int theColorKind)
{
    if (!ThirtyFlagsMode() || theDamage <= 0)
        return;

    int aSlot;
    if (gTFNumberCount < TF_DAMAGE_NUM_MAX)
    {
        aSlot = gTFNumberCount++;
    }
    else
    {
        // ³ØÂú£º¸´ÓÃÊ£ÓàÊÙÃü×î¶ÌµÄÄÇÌõ
        aSlot = 0;
        for (int i = 1; i < TF_DAMAGE_NUM_MAX; i++)
        {
            if (gTFNumbers[i].mLife < gTFNumbers[aSlot].mLife)
                aSlot = i;
        }
    }

    gTFNumbers[aSlot].mX = (float)theX;
    gTFNumbers[aSlot].mY = (float)theY;
    gTFNumbers[aSlot].mValue = theDamage;
    gTFNumbers[aSlot].mLife = TF_DAMAGE_NUM_LIFE;
    gTFNumbers[aSlot].mColorKind = theColorKind;
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ¡¾ĞÔÄÜ¡¿Ã¿Ö¡ºÄÊ±Ì½Õë£¨µ÷ÊÔ°æÍ³¼Æ£»PLAYER °æÎª¿ÕÊµÏÖ£©
//
// ÎªÊ²Ã´ÏÈ×öÕâ¸ö¡¢¶ø²»ÊÇÖ±½Ó»»Êı¾İ½á¹¹£ºÏÈÇ°Æ¾¡¸¶ÔÏó³Ø±éÀúÊÇÏßĞÔÉ¨Ãè¡¢Ì½²â¿Õ²Û±ØÈ» cache miss¡¹
// µÄÅĞ¶Ï×ö¹ıÒ»´ÎÓÅ»¯£¨»î²ÛÎ»Í¼ + Ìø¶Î£©£¬Î¢»ù×¼Êµ²â·´¶ø¸üÂı£¨ÈÈÊı¾İ 0.6x£¬ÆÓËØÎ»É¨Ãè 0.3x£©¡ª¡ª
// Ô­ÊµÏÖÊÇË³Ğò·ÃÎÊ¡¢Ô¤È¡Æ÷³ÔµÃÏÂ£¬¶ø¸ü´ÏÃ÷µÄ½á¹¹¶àÁË·ÖÖ§Óë¼ä½ÓÑ°Ö·¡£
// ½ÌÑµ£ºÕâ¸öÁ¿¼¶ÏÂ¡¾·Ã´æÄ£Ê½Óë³£Êı¡¿±È¡¾½¥½ü¸´ÔÓ¶È¡¿ÖØÒª£¬»»½á¹¹Ö®Ç°±ØĞëÏÈÓĞÊı¾İ¡£
//
// Èı¸öÊ±¼ä´Á°ÑÒ»Ö¡²ğ³ÉËÄ¶Î£º
//   UpdateBegin ÔÚ Board::Update ×î¿ªÍ·£»DrawBegin / DrawEnd ¼Ğ×¡ Board::Draw
//   update = DrawBegin - UpdateBegin      ¸üĞÂ²à×ÜºÄÊ±
//   draw   = DrawEnd   - DrawBegin        ÕæÕıµÄ»æÖÆºÄÊ±
//   idle   = ÏÂÒ»Ö¡ UpdateBegin - DrawEnd Ö¡ÂÊÏŞÖÆÆ÷/´¹Ö±Í¬²½µÄµÈ´ı
//   frame  = Á½Ö¡ UpdateBegin Ö®¼ä = update + draw + idle
// ----------------------------------------------------------------------------------------------------
#if !defined(TF_PLAYER_BUILD)
static LARGE_INTEGER gTFPerfFreq;
static LARGE_INTEGER gTFPerfUpdateBeginTick;
static LARGE_INTEGER gTFPerfDrawBeginTick;
static LARGE_INTEGER gTFPerfPrevUpdateBeginTick;
static LARGE_INTEGER gTFPerfPrevDrawEndTick;
static bool          gTFPerfInited = false;
static int           gTFPerfFrames = 0;
static int           gTFPerfSlowLogged = 0;
static double        gTFPerfSumUpdate = 0.0;
static double        gTFPerfSumDraw = 0.0;
static double        gTFPerfSumIdle = 0.0;
static double        gTFPerfSumFrame = 0.0;
static double        gTFPerfMaxUpdate = 0.0;
static double        gTFPerfMaxDraw = 0.0;
static double        gTFPerfMaxFrame = 0.0;

// ¡¾ĞÔÄÜ¡¿¿ò¼ÜäÖÈ¾Æ÷Ìá¹©µÄ¾«ÁéÍ³¼Æ£¨D3DInterface.cpp£©¡£
// ÕâÀïÖ»×öÉùÃ÷£¬²»Òı D3DInterface.h ¡ª¡ª ÄÇÍ·ÎÄ¼ş»áÍÏ½ø d3d8 µÄÍ·£¬Óë Lawn ²à³åÍ»¡£
void D3DInterfaceGetSpriteStats(unsigned int* theQuads, unsigned int* theCalls);

static unsigned int gTFPerfSpriteQuads = 0;   // ¡¾ĞÔÄÜ¡¿±¾Ö¡¾«ÁéËÄ±ßĞÎÊı
static unsigned int gTFPerfSpriteCalls = 0;   // ¡¾ĞÔÄÜ¡¿±¾Ö¡Êµ¼Ê»æÖÆµ÷ÓÃ´ÎÊı
static unsigned int gTFPerfSpriteClipped = 0;  // ¡¾ĞÔÄÜ¡¿ÆäÖĞ×ß CPU ¼ô¼ôµÄËÄ±ßĞÎÊı
static unsigned int gTFPerfFx = 0;            // ¡¾Õï¶Ï¡¿ÌØĞ§ÊıÁ¿
static unsigned int gTFPerfFill = 0;          // ¡¾Õï¶Ï¡¿±¾Ö¡ÒÑÓÃÌî³ä

static double TFPerfMs(LARGE_INTEGER theFrom, LARGE_INTEGER theTo)
{
    return (double)(theTo.QuadPart - theFrom.QuadPart) * 1000.0 / (double)gTFPerfFreq.QuadPart;
}

void ThirtyFlagsPerfUpdateBegin()
{
    if (!ThirtyFlagsMode())
        return;

    if (!gTFPerfInited)
    {
        QueryPerformanceFrequency(&gTFPerfFreq);
        gTFPerfInited = true;
    }

    LARGE_INTEGER aNow;
    QueryPerformanceCounter(&aNow);

    // ÊÕÎ²ÉÏÒ»Ö¡£ºÉÏÒ»Ö¡ UpdateBegin µ½ÏÖÔÚ = ÕûÖ¡¼ä¸ô£»ÉÏÒ»Ö¡ DrawEnd µ½ÏÖÔÚ = µÈ´ı
    if (gTFPerfPrevUpdateBeginTick.QuadPart != 0 && gTFPerfPrevDrawEndTick.QuadPart != 0)
    {
        double aFrameMs = TFPerfMs(gTFPerfPrevUpdateBeginTick, aNow);
        double anIdleMs = TFPerfMs(gTFPerfPrevDrawEndTick, aNow);
        if (aFrameMs > 0.0 && aFrameMs < 500.0)
        {
            if (anIdleMs < 0.0)
                anIdleMs = 0.0;
            gTFPerfFrames++;
            gTFPerfSumFrame += aFrameMs;
            gTFPerfSumIdle += anIdleMs;
            if (aFrameMs > gTFPerfMaxFrame)
                gTFPerfMaxFrame = aFrameMs;
        }
    }

    gTFPerfPrevUpdateBeginTick = aNow;
    gTFPerfUpdateBeginTick = aNow;
}

void ThirtyFlagsPerfDrawBegin()
{
    if (!ThirtyFlagsMode() || !gTFPerfInited)
        return;

    LARGE_INTEGER aNow;
    QueryPerformanceCounter(&aNow);
    gTFPerfDrawBeginTick = aNow;

    double anUpdateMs = TFPerfMs(gTFPerfUpdateBeginTick, aNow);
    if (anUpdateMs > 0.0 && anUpdateMs < 500.0)
    {
        gTFPerfSumUpdate += anUpdateMs;
        if (anUpdateMs > gTFPerfMaxUpdate)
            gTFPerfMaxUpdate = anUpdateMs;
    }
}

void ThirtyFlagsPerfDrawEnd(Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theBoard || !gTFPerfInited)
        return;

    LARGE_INTEGER aNow;
    QueryPerformanceCounter(&aNow);
    gTFPerfPrevDrawEndTick = aNow;

    double aDrawMs = TFPerfMs(gTFPerfDrawBeginTick, aNow);
    if (aDrawMs > 0.0 && aDrawMs < 500.0)
    {
        gTFPerfSumDraw += aDrawMs;
        if (aDrawMs > gTFPerfMaxDraw)
            gTFPerfMaxDraw = aDrawMs;
    }

    // ¡¾ĞÔÄÜ¡¿È¡±¾Ö¡¾«Áé»æÖÆÍ³¼Æ£¨¶ÁÈ¡¼´ÇåÁã£©£ºËÄ±ßĞÎÊı / Êµ¼Ê»æÖÆµ÷ÓÃ´ÎÊı
    D3DInterfaceGetSpriteStats(&gTFPerfSpriteQuads, &gTFPerfSpriteCalls);

    // µ¥Ö¡µôÖ¡£ºÁ¢¿Ì¼ÇÒ»ĞĞ£¨º¬µ±Ê±µÄÊµÌå¹æÄ££©£¬×î¶à 40 Ìõ
    double aFrameMs = (gTFPerfPrevUpdateBeginTick.QuadPart != 0)
        ? TFPerfMs(gTFPerfPrevUpdateBeginTick, aNow) : 0.0;
    if (aFrameMs > 40.0 && gTFPerfSlowLogged < 40)
    {
        gTFPerfSlowLogged++;
        char aBuf[256];
        sprintf(aBuf, "[TFPerf] SLOW frame=%.1fms update=%.1f draw=%.1f | z=%u pl=%u pr=%u reanim=%u emit=%u part=%u",
            aFrameMs, TFPerfMs(gTFPerfUpdateBeginTick, gTFPerfDrawBeginTick), aDrawMs,
            theBoard->mZombies.mSize, theBoard->mPlants.mSize, theBoard->mProjectiles.mSize,
            theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize,
            theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters.mSize,
            theBoard->mApp->mEffectSystem->mParticleHolder->mParticles.mSize);
        TFLog(aBuf);
    }

    // Ã¿ 500 Ö¡»ã×ÜÒ»ĞĞ
    if (gTFPerfFrames >= 500)
    {
        char aBuf[320];
        sprintf(aBuf,
            "[TFPerf] %d frames avg=%.1f/%.1f/%.1f/%.1f max=%.1f/%.1f/%.1f ms (frame/update/draw/idle, %.0f fps) | z=%u pl=%u pr=%u reanim=%u emit=%u part=%u | sprite=%u/%u clip=%u 3d=%d",
            gTFPerfFrames,
            gTFPerfSumFrame / (double)gTFPerfFrames, gTFPerfSumUpdate / (double)gTFPerfFrames,
            gTFPerfSumDraw / (double)gTFPerfFrames, gTFPerfSumIdle / (double)gTFPerfFrames,
            gTFPerfMaxFrame, gTFPerfMaxUpdate, gTFPerfMaxDraw,
            1000.0 / (gTFPerfSumFrame / (double)gTFPerfFrames),
            theBoard->mZombies.mSize, theBoard->mPlants.mSize, theBoard->mProjectiles.mSize,
            theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize,
            theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters.mSize,
            theBoard->mApp->mEffectSystem->mParticleHolder->mParticles.mSize,
            gTFPerfSpriteQuads, gTFPerfSpriteCalls,
            gTFPerfSpriteClipped,
            theBoard->mApp->Is3DAccelerated() ? 1 : 0);
        TFLog(aBuf);

        gTFPerfFrames = 0;
        gTFPerfSumUpdate = gTFPerfSumDraw = gTFPerfSumIdle = gTFPerfSumFrame = 0.0;
        gTFPerfMaxFrame = gTFPerfMaxUpdate = gTFPerfMaxDraw = 0.0;
    }
}
#else
void ThirtyFlagsPerfUpdateBegin() { }
void ThirtyFlagsPerfDrawBegin() { }
void ThirtyFlagsPerfDrawEnd(Board*) { }
#endif

void ThirtyFlagsUpdateVisuals()
{
    if (!ThirtyFlagsMode())
        return;

    // ¡¾±íÏÖ²ã¡¿ÌØĞ§³ØÍÆ½ø
    ThirtyFlagsFxUpdate();

    // ¡¾ĞÔÄÜ¡¿ËüÔÚ Board::Update ×î¿ªÍ·±»µ÷ÓÃ£¬ÕıºÃµ±×÷¡¸±¾Ö¡¸üĞÂ¿ªÊ¼¡¹µÄÊ±¼ä´Á
    ThirtyFlagsPerfUpdateBegin();

    gTFTick++;

    // ¡¾ÈıÊ®Æì¡¿¶ÙÖ¡£¨²ß»®°¸ 7.7£©£º¶³½áÆÚ¼äÌø×Ö / ±¬·¢»· / Á¬É±Ìõ / Ñô¹â¹ö¶¯È«²¿¶¨¸ñ£¬
    // Ö»ÍÆ½ø¼ÆÊıÆ÷±¾Éí£¬ÓÃÀ´ÖÆÔì´ò»÷Ë²¼äµÄ"¶Ù¸Ğ"¡£²»´¥Åö Board::Update µÄÊ±ĞòÂß¼­¡£
    if (gTFHitFlash > 0)
        gTFHitFlash--;

    if (gTFFreezeFrames > 0)
    {
        gTFFreezeFrames--;
        return;
    }

    // ¡¾ÈıÊ®Æì¡¿´¦¾ö±¬·¢»·ÍÆ½ø£¨²ß»®°¸ 7.7£©
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife > 0)
            gTFBursts[i].mLife--;
    }
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife <= 0 && i < gTFBurstCount - 1)
        {
            gTFBursts[i] = gTFBursts[gTFBurstCount - 1];
            gTFBurstCount--;
            i--;
        }
    }
    if (gTFBurstCount > 0 && gTFBursts[gTFBurstCount - 1].mLife <= 0)
        gTFBurstCount--;

    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife > 0)
        {
            gTFNumbers[i].mLife--;
            gTFNumbers[i].mY -= 0.9f;      // »º»ºÉÏÆ®
        }
    }

    // °ÑÒÑ¾­ÏûÊ§µÄÌõÄ¿¼·µ½Êı×éÎ²²¿£¬±£³ÖÇ°¶Î½ô´Õ
    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife <= 0 && i < gTFNumberCount - 1)
        {
            gTFNumbers[i] = gTFNumbers[gTFNumberCount - 1];
            gTFNumberCount--;
            i--;
        }
    }
    if (gTFNumberCount > 0 && gTFNumbers[gTFNumberCount - 1].mLife <= 0)
        gTFNumberCount--;

    if (gTFCenterLife > 0)
        gTFCenterLife--;

    // Á¬É±Ìõ£ºÁ¬É±ÊıÒ»±ä¾ÍÖØĞÂ¼ÆÊ±
    if (gThirtyFlags.mKillStreak != gTFStreakSeen)
    {
        gTFStreakSeen = gThirtyFlags.mKillStreak;
        if (gTFStreakSeen > 1)
        {
            gTFStreakValue = gTFStreakSeen;
            gTFStreakShow = TF_STREAK_SHOW_LIFE;
        }
    }
    if (gTFStreakShow > 0)
        gTFStreakShow--;

    // ¡¾ÈıÊ®Æì¡¿Ñô¹â¹ö¶¯£¨²ß»®°¸ 7.7£©£ºÑô¹âÔö¼ÓÊ±°ÑÔöÁ¿×ö³É¹ö¶¯Êı×Ö£¬´Ó 0 Æ½»¬±Æ½üÊµ¼ÊÔöÁ¿¡£
    // ËµÃ÷£ºÕæÕıµÄÑô¹â¼ÆÊıÆ÷»æÖÆÔÚ SeedPacket.cpp::SeedBank::Draw£¨²»ÔÚ±¾ÈÎÎñÎÄ¼ş·¶Î§ÄÚ£©£¬
    // ÕâÀïÓÃÒ»Ã¶Ìù½üÑô¹âÀ¸µÄ¹ö¶¯"+N"Ö¸Ê¾Æ÷ÊµÏÖÍ¬ÑùµÄÊı×Ö¹ö¶¯¹ı¶É¡£
    Board* aBoard = gLawnApp ? gLawnApp->mBoard : nullptr;
    if (aBoard)
    {
        int aSun = max(aBoard->mSunMoney, 0);
        if (gTFSunSeen < 0)
            gTFSunSeen = aSun;

        if (aSun > gTFSunSeen)
        {
            gTFSunGain += (aSun - gTFSunSeen);
            gTFSunRollValue = 0;
            gTFSunRollLife = TF_SUN_ROLL_LIFE;
        }
        gTFSunSeen = aSun;
    }

    if (gTFSunRollLife > 0)
    {
        gTFSunRollLife--;
        if (gTFSunRollValue < gTFSunGain)
        {
            int aStep = max(1, gTFSunGain / 8 + 1);
            gTFSunRollValue = min(gTFSunGain, gTFSunRollValue + aStep);
        }
        if (gTFSunRollLife <= 0)
        {
            gTFSunGain = 0;
            gTFSunRollValue = 0;
        }
    }
}

void ThirtyFlagsDrawVisuals(Sexy::Graphics* g)
{
    // ¡¾±íÏÖ²ã¡¿ÌØĞ§²ã£¨¼ÓÉ«£¬¶ÀÁ¢³É×é£©
    ThirtyFlagsFxDraw(g);

    if (!ThirtyFlagsMode() || !g)
        return;

    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Ê¬¶¾Ì¶£ºÖ®Ç°Ö»ÓĞÉËº¦¡¢**ÍêÈ«Ã»ÓĞ»æÖÆ**£¬Íæ¼Ò¿´²»µ½¶¾ÔÚÄÄ£¬
    // Ö»»á¿´µ½Ö²ÎïµôÑªÉõÖÁÏûÊ§¡£ÕâÀï²¹Ò»¸ö°ëÍ¸Ã÷¶¾Ì¶ + ºôÎü»·¡£
    {
        Board* aPoolBoard = gLawnApp ? gLawnApp->mBoard : nullptr;
        if (aPoolBoard)
        {
            for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
            {
                for (int aPoolRow = 0; aPoolRow < MAX_GRID_SIZE_Y; aPoolRow++)
                {
                    int aFrames = gThirtyFlagsPoison[aCol][aPoolRow];
                    if (aFrames <= 0)
                        continue;

                    int aPx = aPoolBoard->GridToPixelX(aCol, aPoolRow);
                    int aPy = aPoolBoard->GridToPixelY(aCol, aPoolRow);
                    int anAlpha = min(130, 50 + aFrames / 5);

                    g->SetColor(Sexy::Color(80, 190, 60, anAlpha));
                    g->FillRect(aPx + 8, aPy + 18, 64, 66);
                    TFDrawRing(g, aPx + 40, aPy + 52, 30.0f, Sexy::Color(160, 255, 120, anAlpha));
                }
            }
        }
    }

    // ¡¾ÈıÊ®Æì¡¿´¦¾ö±¬·¢»·£¨²ß»®°¸ 7.7£©£º¾«Ó¢ / ¾ŞÈË½©Ê¬ÕóÍö´¦µÄÀ©É¢³å»÷»·
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife <= 0)
            continue;

        int aElapsed = TF_BURST_LIFE - gTFBursts[i].mLife;
        int anAlpha = ClampInt(gTFBursts[i].mLife * 220 / TF_BURST_LIFE, 0, 220);
        float aMaxRadius = gTFBursts[i].mBig ? 90.0f : 55.0f;
        float aRadius = 8.0f + aMaxRadius * ((float)aElapsed / (float)TF_BURST_LIFE);

        TFDrawRing(g, (int)gTFBursts[i].mX, (int)gTFBursts[i].mY, aRadius,
            gTFBursts[i].mBig ? Sexy::Color(255, 150, 40, anAlpha) : Sexy::Color(255, 235, 120, anAlpha));
        TFDrawRing(g, (int)gTFBursts[i].mX, (int)gTFBursts[i].mY, aRadius * 0.6f,
            Sexy::Color(255, 255, 220, anAlpha / 2));
    }

    // ¡¾ÈıÊ®Æì¡¿¶ÙÖ¡¸ß¹â£¨²ß»®°¸ 7.7£©£º¶ÙÖ¡ÆğÊ¼Ò»Á½Ö¡µÄ¼«µ­°×ÉÁ£¬Ç¿»¯´ò»÷Ë²¼ä
    if (gTFHitFlash > 0)
    {
        g->SetColor(Sexy::Color(255, 255, 255, ClampInt(gTFHitFlash * 12, 0, 48)));
        g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    }

    // ÉËº¦Ìø×Ö
    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife <= 0)
            continue;

        SexyString aText = StrFormat(_S("%d"), gTFNumbers[i].mValue);
        int anAlpha = ClampInt(gTFNumbers[i].mLife * 300 / TF_DAMAGE_NUM_LIFE, 0, 255);

        // ¡¾ÈıÊ®Æì¡¿7.7 Ìø×Ö·ÖÉ«£ºÆÕÍ¨»Æ°× / ±ùÀ¶ / ¶¾ÂÌ / »ğ³È / ±©»÷½ğ£¨>=100 ´óÉËº¦»ò±©»÷£©
        Sexy::Color aMain;
        switch (gTFNumbers[i].mColorKind)
        {
        case 1:   aMain = Sexy::Color(130, 200, 255, anAlpha); break;
        case 2:   aMain = Sexy::Color(130, 255, 130, anAlpha); break;
        case 3:   aMain = Sexy::Color(255, 150, 60,  anAlpha); break;
        case 4:   aMain = Sexy::Color(255, 215, 0,   anAlpha); break;
        default:  aMain = Sexy::Color(255, 240, 150, anAlpha); break;
        }

        int aPasses = (gTFNumbers[i].mColorKind == 4) ? 9 : 5;   // ´óÉËº¦Ãè±ß¸üºñ
        for (int aPass = 0; aPass < aPasses; aPass++)
        {
            static const int aOff[9][2] = { {0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
            Sexy::Color aCol = (aPass == 0) ? aMain : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, aText, (int)gTFNumbers[i].mX + aOff[aPass][0], (int)gTFNumbers[i].mY + aOff[aPass][1],
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // ¡¾ÈıÊ®Æì¡¿ÖĞÑë¹«¸æ£¨»»Æì´ó×Ö / ¾«Ó¢³öÃ»£©
    if (gTFCenterLife > 0)
    {
        int anAlpha = ClampInt(gTFCenterLife * 400 / gTFCenterLifeMax, 0, 255);
        for (int aPass = 0; aPass < 9; aPass++)
        {
            static const int aOff[9][2] = { {0,0},{2,0},{-2,0},{0,2},{0,-2},{2,2},{2,-2},{-2,2},{-2,-2} };
            Sexy::Color aCol = (aPass == 0) ? Sexy::Color(gTFCenterColor.mRed, gTFCenterColor.mGreen, gTFCenterColor.mBlue, anAlpha)
                                            : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, gTFCenterText, BOARD_WIDTH / 2 + aOff[aPass][0], 210 + aOff[aPass][1],
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // Á¬É±·´À¡Ìõ
    if (gTFStreakShow > 0 && gTFStreakValue > 1)
    {
        int anAlpha = ClampInt(gTFStreakShow * 300 / TF_STREAK_SHOW_LIFE, 0, 255);
        SexyString aText = StrFormat(_S("%d Á¬É±"), gTFStreakValue);

        for (int aPass = 0; aPass < 5; aPass++)
        {
            int aOffX = (aPass == 1) - (aPass == 2);
            int aOffY = (aPass == 3) - (aPass == 4);
            Sexy::Color aCol = (aPass == 0) ? Sexy::Color(255, 70, 40, anAlpha)
                                            : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, aText, BOARD_WIDTH / 2 + aOffX, 150 + aOffY,
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // ¡¾ÈıÊ®Æì¡¿Ñô¹â¹ö¶¯£¨²ß»®°¸ 7.7£©£º¹ö¶¯"+N"Ö¸Ê¾Æ÷£¬½ôÌùÑô¹âÀ¸ÏÂ·½
    if (gTFSunRollLife > 0 && gTFSunGain > 0)
    {
        int anAlpha = ClampInt(gTFSunRollLife * 255 / TF_SUN_ROLL_LIFE, 0, 255);
        SexyString aText = StrFormat(_S("+%d"), gTFSunRollValue);
        TodDrawString(g, aText, 34, 104, Sexy::FONT_DWARVENTODCRAFT12,
            Sexy::Color(255, 240, 120, anAlpha), DrawStringJustification::DS_ALIGN_CENTER);
    }

    // ¡¾ÈıÊ®Æì¡¿Ç¿»¯³éÈ¡³ö³¡Ñİ³ö£¨²ß»®°¸ 7.7£©£º³éÈ¡½×¶Î¸øÆÁÄ»×îÍâÑØ¼ÓÒ»È¦ºôÎü½ğÉ«¸ßÁÁ±ß¿ò¡£
    // ÈıÑ¡Ò»µ¯´°±¾ÌåÔÚ TFDialog.cpp£¨²»ÔÚ±¾ÈÎÎñÎÄ¼ş·¶Î§ÄÚ£©£¬´Ë´¦ÓÃ±ß¿ò¸ßÁÁºæÍĞ"³éÈ¡ÖĞ"·ÕÎ§£»
    // ±ß¿òÎ»ÓÚÆÁÄ»×îÍâÑØ£¬²»»á±»¾ÓÖĞµÄµ¯´°ÕÚ×¡¡£
    if (gThirtyFlags.mIntermission && !gThirtyFlags.mUpgradeChosen)
    {
        int aPulse = (int)((sinf((float)gTFTick * 0.12f) + 1.0f) * 0.5f * 80.0f) + 120;
        aPulse = ClampInt(aPulse, 0, 255);
        g->SetColor(Sexy::Color(255, 210, 90, aPulse));

        const int aThick = 6;
        g->FillRect(0, 0, BOARD_WIDTH, aThick);
        g->FillRect(0, BOARD_HEIGHT - aThick, BOARD_WIDTH, aThick);
        g->FillRect(0, 0, aThick, BOARD_HEIGHT);
        g->FillRect(BOARD_WIDTH - aThick, 0, aThick, BOARD_HEIGHT);
    }
}

void ThirtyFlagsShowCenterText(const SexyString& theText, const Sexy::Color& theColor, int theLife)
{
    if (!ThirtyFlagsMode())
        return;
    gTFCenterText = theText;
    gTFCenterColor = theColor;
    gTFCenterLife = theLife;
    gTFCenterLifeMax = theLife;
}

// -------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¤Æ½ºâ v5¡¿ÆÆÕÀ±ê¼ÇÏµÍ³£¨Íã¶¹ÉäÊÖ¡¢Ã¨Î²²İ = Ê©¼ÓÕß / Ë«·¢¡¢»úÇ¹ = Òı±¬Õß£©
// ´æ´¢ÓÃÎÄ¼ş¼¶ static ±í°´½©Ê¬ ID Ë÷Òı£¨²»µÃ¸ø Zombie ¼Ó³ÉÔ±¡ª¡ªSyncBoard °´ sizeof(Board) Ğ´ÅÌ£©
// -------------------------------------------------------------------------------------------
static int  gTFMarkCount[512];
static int  gTFMarkTimer[512];
static int  gTFMarkID[512];

void ThirtyFlagsAddMark(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512)
        return;
    if (gTFMarkID[aZid] != aZid)
    {
        gTFMarkID[aZid] = aZid;
        gTFMarkCount[aZid] = 0;
    }
    if (gTFMarkCount[aZid] < 5)
        gTFMarkCount[aZid]++;
    gTFMarkTimer[aZid] = 240;   // 4 ÃëÓĞĞ§ÆÚ
}

int ThirtyFlagsGetMark(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return 0;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512 || gTFMarkID[aZid] != aZid)
        return 0;
    if (gTFMarkTimer[aZid] <= 0)
        return 0;
    return gTFMarkCount[aZid];
}

// -------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿ÓÇÓô¹½¡¸æß×Ó¸¯»¯¡¹
//
// ±»æß×ÓÃüÖĞµÄ½©Ê¬½øÈë¸¯»¯£º
//   1) ½ûÖ¹Ò»ÇĞ»Ø¸´ ¡ª¡ª ¾«Ó¢¡¸ÔÙÉú¡¹/¡¸»¤¶Ü¡¹¡¢Í»±ä¡¸ÎüÑª¡¹È«²¿Ê§Ğ§
//      £¨Õâ²ÅÊÇ¡¸´ò²»¶¯¾«Ó¢¡¹µÄ»úÖÆ½â£º²»¿¿ DPS Ó²Åö£¬¶øÊÇ¹Øµô¶Ô·½µÄ»ØÑª£©
//   2) Ã¿²ã -12% ÒÆËÙ£¨×î¶à 4 ²ã = -48%£©
// Í¬ÑùÓÃÎÄ¼ş¼¶ static ±í°´½©Ê¬ ID Ë÷Òı£¨²»µÃ¸ø Zombie ¼Ó³ÉÔ±£©¡£
// -------------------------------------------------------------------------------------------
static int  gTFCorruptStacks[512];
static int  gTFCorruptTimer[512];
static int  gTFCorruptID[512];

void ThirtyFlagsAddCorrupt(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return;
    if (theZombie->mZombieType == ZOMBIE_BOSS)
        return;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512)
        return;
    if (gTFCorruptID[aZid] != aZid)
    {
        gTFCorruptID[aZid] = aZid;
        gTFCorruptStacks[aZid] = 0;
    }
    if (gTFCorruptStacks[aZid] < TF_CORRUPT_MAX_STACKS)
        gTFCorruptStacks[aZid]++;
    gTFCorruptTimer[aZid] = TF_CORRUPT_DURATION;
}

int ThirtyFlagsGetCorrupt(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return 0;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512 || gTFCorruptID[aZid] != aZid)
        return 0;
    if (gTFCorruptTimer[aZid] <= 0)
        return 0;
    return gTFCorruptStacks[aZid];
}

// -------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Ã¨Î²²İ¡¸ÊÕ¸î¡¹
//
// Ã¨Î²²İµÄ´Ì´ø¡¸ÆÆÕÀ¡¹±ê¼Ç£¨¸´ÓÃÍã¶¹µÄ±ê¼ÇÏµÍ³£©£¬²¢ÇÒ£º
//   * ¶Ô±»¸¯»¯µÄÄ¿±êÉËº¦ ¡Á2£¨¸¯»¯ ¡Á ÊÕ¸î = Ã÷È·µÄ×éºÏ build£©
//   * ¶Ô¾«Ó¢½©Ê¬ÉËº¦ ¡Á1.5£¨²»ÒÀÀµ¸¯»¯Ò²ÄÜÒ§µÃ¶¯¾«Ó¢£©
// Á½ÕßÍ¬Ê±³ÉÁ¢Ê±È¡³Ë»ı£¨¡Á3£©¡£·µ»Ø°Ù·Ö±È£¬100 = ÎŞ¼Ó³É¡£
// -------------------------------------------------------------------------------------------
int ThirtyFlagsHarvestPercent(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return 100;
    int aPct = 100;
    if (ThirtyFlagsGetCorrupt(theZombie, theBoard) > 0)
        aPct = aPct * TF_HARVEST_CORRUPT_PCT / 100;
    if (TFHasElite(theZombie, ELITE_SWIFT) || TFHasElite(theZombie, ELITE_IRONWALL) ||
        TFHasElite(theZombie, ELITE_REGEN) || TFHasElite(theZombie, ELITE_SHIELD) ||
        TFHasElite(theZombie, ELITE_SPLIT) || TFHasElite(theZombie, ELITE_SPEAR) ||
        TFHasElite(theZombie, ELITE_BERSERK))
        aPct = aPct * TF_HARVEST_ELITE_PCT / 100;
    return aPct;
}

// -------------------------------------------------------------------------------------------
// ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿²ß»®°¸ 6.2£º±»ÄÏ¹ÏÌ××¡µÄÖ²Îï£¬Ñô¹âÏûºÄÓëÀäÈ´ -50%
//
// ÓïÒå³ÎÇå£¨²ß»®°¸Ô­ÎÄ£º¡¸ÄÏ¹ÏÍ·£º»¤¼× 4000¡ú8000£»±»ÄÏ¹ÏÌ××¡µÄÖ²Îï£¬ÆäÑô¹âÏûºÄÓëÀäÈ´ -50%¡¹£©£º
// Ö÷ÌåÊÇ**±»Ì××¡µÄÄÇÖêÖ²Îï**£¬²»ÊÇÄÏ¹ÏÍ·±¾Éí ¡ª¡ª ËùÒÔÄÏ¹ÏÍ·»Ö¸´Ô­¼Û¡£
//
// Î¨Ò»ÄÜ´òÕÛµÄÊ±»úÊÇ¡¸ÍùÒÑÓĞÄÏ¹ÏÍ·µÄ¸ñ×ÓÀïÖÖÖ²Îï¡¹£º±¾¸Ä°æµÄµşÖÖ»úÖÆÔÊĞíÕâÑù×ö
// £¨Board::CanPlantAt µÄ aPumpkinPlant ·ÖÖ§£©£¬ÕâÒ²ÕıºÃ¹ÄÀø¡¸ÏÈÆÌÄÏ¹Ï¡¢ÔÙÍùÀïÃæÖÖ¡¹µÄ½¨ÔìË³Ğò¡£
// ·´¹ıÀ´¡¸ÏÈÖÖÖ²Îï¡¢ÔÙÓÃÄÏ¹ÏÌ××¡¡¹Ê±£¬Ö²ÎïµÄÇ®ºÍÀäÈ´Ôç¾Í¸¶¹ıÁË£¬ÎŞ·¨×·ÈÏÕÛ¿Û¡£
// -------------------------------------------------------------------------------------------
bool ThirtyFlagsIsPlantingIntoPumpkin(Board* theBoard, int theGridX, int theGridY, int theSeedType)
{
    if (!ThirtyFlagsMode() || !theBoard)
        return false;
    if (theGridX < 0 || theGridY < 0)
        return false;
    // ÄÏ¹ÏÍ·×Ô¼º²»ÊÇ¡¸±»Ì××¡µÄÖ²Îï¡¹£»Ä£·ÂÕßÒ²²»²ÎÓë£¨Ëü¸´ÖÆµÄÊÇ±ğµÄ¿¨£©
    if (theSeedType == (int)SeedType::SEED_PUMPKINSHELL || theSeedType == (int)SeedType::SEED_IMITATER)
        return false;
    return theBoard->GetPumpkinAt(theGridX, theGridY) != nullptr;
}

static void ThirtyFlagsTickCorrupt()
{
    for (int i = 0; i < 512; i++)
    {
        if (gTFCorruptTimer[i] > 0)
        {
            gTFCorruptTimer[i]--;
            if (gTFCorruptTimer[i] == 0)
                gTFCorruptStacks[i] = 0;
        }
    }
}

static void ThirtyFlagsTickMarks()
{
    for (int i = 0; i < 512; i++)
    {
        if (gTFMarkTimer[i] > 0)
        {
            gTFMarkTimer[i]--;
            if (gTFMarkTimer[i] == 0)
                gTFMarkCount[i] = 0;
        }
    }
}

// -------------------------------------------------------------------------------------------
// [ThirtyFlags] RUN SAVE
//
// The original game already persists everything this mode needs:
//   * Board::UpdateLevelEndSequence() calls TryToSaveGame() at mNextSurvivalStageCounter == 1
//     whenever LawnApp::IsSurvivalMode() is true -- and IsSurvivalMode() returns true for
//     GAMEMODE_THIRTY_FLAGS -- so a snapshot is written every time a flag is cleared.
//   * SyncBoard() serializes the whole Board from mPaused onwards: grid squares (dirt / grass /
//     pool), fog, plant rows, plants, sun, lawn mowers, the 180-slot wave table, the seed bank,
//     plus the Challenge struct. Challenge::mSurvivalStage is the single source of truth for the
//     flag number, so the flag survives the round trip for free.
//   * LawnApp::PreNewGame(mode, true) -> TryLoadGame() -> Board::LoadGame() restores all of it and
//     then shows the original ContinueDialog ("continue / new game").
//
// What is NOT in Board is the roguelite layer: upgrade stacks, mutations and the intermission
// state. Board's layout cannot be extended (SyncBoard writes it as one raw block), so that layer
// goes into a small sidecar written immediately after LawnSaveGame and read immediately after
// LawnLoadGame. Both files live in AppData/userdata and die together.
// -------------------------------------------------------------------------------------------
static std::string TFGetExtraPath()
{
    LawnApp* aApp = gLawnApp;
    if (!aApp || !aApp->mPlayerInfo)
        return std::string();
    return GetSavedGameName(GameMode::GAMEMODE_THIRTY_FLAGS, aApp->mPlayerInfo->mId) + ".tf";
}

void ThirtyFlagsOnSaveGame(Board* theBoard)
{
    if (!theBoard || !ThirtyFlagsMode())
        return;

    std::string aPath = TFGetExtraPath();
    if (aPath.empty())
        return;

    FILE* aFile = fopen(aPath.c_str(), "wb");
    if (!aFile)
        return;

    int aMagic = TF_SAVE_MAGIC;
    fwrite(&aMagic, sizeof(aMagic), 1, aFile);
    fwrite(&gThirtyFlags.mFlag, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mMutationPicksLeft, sizeof(int), 1, aFile);
    fwrite(gThirtyFlags.mMutations, sizeof(bool), NUM_MUTATIONS, aFile);
    fwrite(gThirtyFlags.mUpgradeStacks, sizeof(int), TF_UPG_COUNT, aFile);
    fwrite(&gThirtyFlags.mLegendaryCount, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mIntermission, sizeof(bool), 1, aFile);
    fwrite(&gThirtyFlags.mUpgradeChosen, sizeof(bool), 1, aFile);
    fwrite(&gThirtyFlags.mPrepTimeLeft, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mPrepTimeStart, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mSunAtFlagStart, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mTotalKills, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mLostLanes, sizeof(int), 1, aFile);
    fwrite(&gThirtyFlags.mBossPhase, sizeof(int), 1, aFile);
    fwrite(&gTFDeadCount, sizeof(int), 1, aFile);
    if (gTFDeadCount > 0)
    {
        fwrite(gTFDeadCol, sizeof(int), gTFDeadCount, aFile);
        fwrite(gTFDeadRow, sizeof(int), gTFDeadCount, aFile);
        fwrite(gTFDeadSeed, sizeof(int), gTFDeadCount, aFile);
    }
    fwrite(gThirtyFlagsPoison, sizeof(gThirtyFlagsPoison), 1, aFile);
    // ¡¾ÈıÊ®Æì¡¿²¹³µÎ»Í¼£¬·ÅÔÚ×îºó£¬ÀÏ´æµµ¶Á²»µ½¾ÍÊÇ 0¡£
    fwrite(&gThirtyFlagsMowerGranted, sizeof(int), 1, aFile);
    fclose(aFile);
}

void ThirtyFlagsOnLoadGame(Board* theBoard)
{
    if (!theBoard || !gLawnApp)
        return;
    if (gLawnApp->mGameMode != GameMode::GAMEMODE_THIRTY_FLAGS)
        return;

    // Board / plants / sun / mowers / grid / wave table / mSurvivalStage are already restored by
    // SyncBoard. This path never runs Board::InitLevel(), so ThirtyFlagsInitRun() never runs --
    // rebuild the run object here or every ThirtyFlags hook would see a dead state.
    gThirtyFlags.mActive = true;
    gThirtyFlags.ApplyFlag(ThirtyFlagsCurrentFlag(theBoard));

    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
        gThirtyFlagsSodProgress[aRow] = gThirtyFlags.IsRowUnlocked(aRow) ? 1000 : 0;
    gThirtyFlagsPrevUnlockedRows = gThirtyFlags.mUnlockedRows;
    // [ThirtyFlags] ¾É´æµµÀï²¹Î»µÄ³µÍ£ÔÚ mPosX = -160£¨ÆÁÍâ£©£¬ÕâÀï°ÑËüÃÇÅ²»Ø³¡ÄÚ¡£
    {
        LawnMower* aMower = nullptr;
        while (theBoard->mLawnMowers.IterateNext(aMower))
        {
            if (!aMower->mDead && aMower->mMowerState == LawnMowerState::MOWER_READY && aMower->mPosX < -50.0f)
            {
                aMower->mPosX = -21.0f;
                aMower->mVisible = true;
            }
        }
    }
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
            gThirtyFlagsWaterFade[x][y] = gThirtyFlags.IsWaterCell(y, x) ? 100 : 0;

    memset(gThirtyFlagsPoison, 0, sizeof(gThirtyFlagsPoison));
    gThirtyFlagsMowerGranted = 0;
    gThirtyFlags.mBossPtr = NULL;
    gThirtyFlags.mBossPhase = 0;
    gThirtyFlags.mIntermission = false;
    gThirtyFlags.mUpgradeChosen = false;
    gThirtyFlags.mUpgradeDialogShown = false;
    gTFDeadCount = 0;

    std::string aPath = TFGetExtraPath();
    FILE* aFile = aPath.empty() ? nullptr : fopen(aPath.c_str(), "rb");
    if (!aFile)
    {
        TFLog("load: no .tf sidecar, resuming with a bare flag");
        return;
    }

    int aMagic = 0;
    int aGot = (int)fread(&aMagic, sizeof(aMagic), 1, aFile);
    if (aGot != 1 || aMagic != TF_SAVE_MAGIC)
    {
        fclose(aFile);
        TFLog("load: bad .tf sidecar magic");
        return;
    }

    int aFlag = 0;
    aGot += (int)fread(&aFlag, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mMutationPicksLeft, sizeof(int), 1, aFile);
    aGot += (int)fread(gThirtyFlags.mMutations, sizeof(bool), NUM_MUTATIONS, aFile);
    aGot += (int)fread(gThirtyFlags.mUpgradeStacks, sizeof(int), TF_UPG_COUNT, aFile);
    aGot += (int)fread(&gThirtyFlags.mLegendaryCount, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mIntermission, sizeof(bool), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mUpgradeChosen, sizeof(bool), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mPrepTimeLeft, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mPrepTimeStart, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mSunAtFlagStart, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mTotalKills, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mLostLanes, sizeof(int), 1, aFile);
    aGot += (int)fread(&gThirtyFlags.mBossPhase, sizeof(int), 1, aFile);
    aGot += (int)fread(&gTFDeadCount, sizeof(int), 1, aFile);
    if (gTFDeadCount < 0 || gTFDeadCount > 64)
        gTFDeadCount = 0;
    if (gTFDeadCount > 0)
    {
        aGot += (int)fread(gTFDeadCol, sizeof(int), gTFDeadCount, aFile);
        aGot += (int)fread(gTFDeadRow, sizeof(int), gTFDeadCount, aFile);
        aGot += (int)fread(gTFDeadSeed, sizeof(int), gTFDeadCount, aFile);
    }
    aGot += (int)fread(gThirtyFlagsPoison, sizeof(gThirtyFlagsPoison), 1, aFile);
    aGot += (int)fread(&gThirtyFlagsMowerGranted, sizeof(int), 1, aFile);
    fclose(aFile);

    gThirtyFlags.mUpgradeCount = 0;
    for (int i = 0; i < TF_UPG_COUNT; i++)
        gThirtyFlags.mUpgradeCount += gThirtyFlags.mUpgradeStacks[i];

    // A dialog cannot survive the reload, so let the upgrade prompt come back.
    if (gThirtyFlags.mIntermission && !gThirtyFlags.mUpgradeChosen)
        gThirtyFlags.mUpgradeDialogShown = false;

    {
        char aBuf[128];
        sprintf(aBuf, "load: flag %d, upgrades %d, intermission %d", ThirtyFlagsCurrentFlag(theBoard),
            gThirtyFlags.mUpgradeCount, (int)gThirtyFlags.mIntermission);
        TFLog(aBuf);
    }
    ThirtyFlagsShowCenterText(StrFormat(_S("------ ¼ÌĞø£ºµÚ %d ÃæÆì ------"), ThirtyFlagsCurrentFlag(theBoard)),
        Sexy::Color(255, 200, 60), 180);
}

void ThirtyFlagsClearSave()
{
    LawnApp* aApp = gLawnApp;
    if (!aApp || !aApp->mPlayerInfo)
        return;
    if (aApp->mGameMode != GameMode::GAMEMODE_THIRTY_FLAGS)
        return;

    std::string aBase = GetSavedGameName(GameMode::GAMEMODE_THIRTY_FLAGS, aApp->mPlayerInfo->mId);
    aApp->EraseFile(aBase);
    aApp->EraseFile(aBase + ".tf");
}

int ThirtyFlagsRollFireballElement(int theFrame)
{
    if (!ThirtyFlagsMode())
        return TF_ELEM_NONE;

    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ v5¡¿Í¬ÂÖ¶à·¢µ¯µÄÔªËØ¸ÅÂÊµİ¼õ£ºÍ¬Ò»Ö¡ÄÚµÚ 2/3/4 ·¢µ¯µÄÔªËØ¸ÅÂÊ
    // ÒÀ´Î ¡Á0.6 / ¡Á0.4 / ¡Á0.25 ¡ª¡ª ¾«×¼Ï÷¡¸¶àµ¯ build ¡Á »ğ¾æ¡¹µÄ³ËËãÊÕÒæ¡£
    static int aLastFrame = -1;
    static int aCountInFrame = 0;
    if (theFrame != aLastFrame)
    {
        aLastFrame = theFrame;
        aCountInFrame = 0;
    }
    aCountInFrame++;
    int aDecay = (aCountInFrame <= 1) ? 100 : (aCountInFrame == 2 ? 60 : (aCountInFrame == 3 ? 40 : 25));

    int aRoll = Rand(100);
    int aElemGate = 56 * aDecay / 100;   // ÔªËØ×Ü¸ÅÂÊ 56%£¬ÊÜµİ¼õË¥¼õ
    if (aRoll >= aElemGate)  return TF_ELEM_NONE;   // ÆÕÍ¨»ğÇò

    // ÔªËØÄÚ²¿·Ö²¼£¨°´Ô­±ÈÀıËõ·Å£©
    int aSub = Rand(aElemGate);
    int aAcc = 0;
    aAcc += 12 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_ICE;
    aAcc +=  8 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_DEEPFREEZE;
    aAcc += 10 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_POISON;
    aAcc +=  4 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_CHARM;
    aAcc += 12 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_BUTTER;
    return TF_ELEM_GIANT;
}

void ThirtyFlagsAddPoisonAt(int theCol, int theRow, int theFrames)
{
    TFAddPoison(theCol, theRow, theFrames);
}

int ThirtyFlagsPlantPierce(int theSeedType)
{
    if (!ThirtyFlagsMode())
        return 0;

    switch ((SeedType)theSeedType)
    {
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_SNOWPEA:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_GATLINGPEA:
    case SeedType::SEED_THREEPEATER:
    case SeedType::SEED_SPLITPEA:
        return TF_PIERCE_PEA;
    default:
        return 0;
    }
}

int ThirtyFlagsPlantExtraProjectiles(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 0;

    int aCount = gThirtyFlags.GetExtraProjectiles();

    // ¡¾ÈıÊ®Æì¡¿º£Ä¢¹½ / Ğ¡Åç¹½£ºÃ¿´ÎÉä»÷¶îÍâ´òÒ»·¢£¨²ß»®°¸ 6.3£©¡£
    // ÕæÕıµÄ"ÉÏĞĞ¶îÍâ×Óµ¯"ÔÚ Plant::UpdateShooter Àïµ¥¶À´¦Àí¡£
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM ||
        thePlant->mSeedType == SeedType::SEED_SEASHROOM)
    {
        aCount += 1;
    }

    return aCount;
}

void ThirtyFlagsPlantDied(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant || !thePlant->mBoard)
        return;

    float aRefundRate = gThirtyFlags.GetDeathRefundRate();
    if (aRefundRate > 0.0f)
    {
        int aCost = Plant::GetCost(thePlant->mSeedType, thePlant->mImitaterType);
        int aRefund = (int)((float)aCost * aRefundRate);
        if (aRefund > 0)
        {
            thePlant->mBoard->AddSunMoney(aRefund);

        // ¡¾ÈıÊ®Æì¡¤Äù˜„¡¿¼ÇÂ¼ÕóÍöÖ²Îï£¬ÏÂÒ»ÃæÆì¿ªÊ¼Ê±¸´»î
        if (gThirtyFlags.GetStacks(TF_UPG_NIRVANA) > 0 && gTFDeadCount < 64)
        {
            gTFDeadCol[gTFDeadCount] = thePlant->mPlantCol;
            gTFDeadRow[gTFDeadCount] = thePlant->mRow;
            gTFDeadSeed[gTFDeadCount] = (int)thePlant->mSeedType;
            gTFDeadCount++;
        }
        // ¡¾ÈıÊ®Æì¡¤ÍÌÊÉ¡¿ÕóÍöÊ±°Ñ×ÔÉí×î´óÉúÃüµÄÒ»²¿·Ö·Ö¸øËÄÁÚÖ²Îï
        {
            int aDevour = gThirtyFlags.GetStacks(TF_UPG_DEVOUR);
            if (aDevour > 0 && thePlant->mBoard)
            {
                int aShare = (int)((float)thePlant->mPlantMaxHealth * 0.125f * (float)aDevour);
                static const int aDX[4] = { 1, -1, 0, 0 };
                static const int aDY[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; k++)
                {
                    int aCol = thePlant->mPlantCol + aDX[k];
                    int aRow = thePlant->mRow + aDY[k];
                    if (aCol < 0 || aCol >= MAX_GRID_SIZE_X || aRow < 0 || aRow >= MAX_GRID_SIZE_Y)
                        continue;
                    Plant* aNear = thePlant->mBoard->GetTopPlantAt(aCol, aRow, TOPPLANT_ONLY_NORMAL_POSITION);
                    if (aNear && aNear != thePlant)
                    {
                        aNear->mPlantHealth += aShare;
                        aNear->mPlantMaxHealth += aShare;
                    }
                }
            }
        }
        // ¡¾ÈıÊ®Æì¡¤Á¬Ëø¡¿±¬Õ¨ÀàÕóÍöºóÔÙ´¥·¢Ò»´Î°ë¶î¶ş´Î±¬Õ¨
        {
            int aChain = gThirtyFlags.GetStacks(TF_UPG_CHAIN);
            if (aChain > 0)
            {
                SeedType aSeed = (SeedType)thePlant->mSeedType;
                bool aExplosive = (aSeed == SeedType::SEED_CHERRYBOMB || aSeed == SeedType::SEED_JALAPENO ||
                                   aSeed == SeedType::SEED_POTATOMINE || aSeed == SeedType::SEED_DOOMSHROOM ||
                                   aSeed == SeedType::SEED_SQUASH || aSeed == SeedType::SEED_EXPLODE_O_NUT ||
                                   aSeed == SeedType::SEED_GRAVEBUSTER || aSeed == SeedType::SEED_COBCANNON);
                if (aExplosive)
                {
                    thePlant->mBoard->KillAllZombiesInRadius(thePlant->mRow, thePlant->mX, thePlant->mY,
                        120, 0, false, 900 * aChain);
                }
            }
        }
        }
    }

    // ¼á¹ûÇ½£º±»¿ĞËÀºó÷È»óËùÓĞÕıÔÚ¿ĞÊ³ËüµÄ½©Ê¬
    if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_GIANT_WALLNUT ||
        thePlant->mSeedType == SeedType::SEED_EXPLODE_O_NUT)
    {
        int aCharmed = 0;
        Zombie* aZombie = nullptr;
        while (thePlant->mBoard->IterateZombies(aZombie))
        {
            if (aZombie->IsDeadOrDying() || aZombie->mMindControlled || aZombie->mRow != thePlant->mRow)
                continue;

            if (fabsf(aZombie->mPosX - (float)thePlant->mX) <= 90.0f)
            {
                aZombie->mMindControlled = true;
                aCharmed++;
            }
        }

        if (aCharmed > 0)
        {
            thePlant->mBoard->mApp->AddTodParticle((float)thePlant->mX + 40.0f, (float)thePlant->mY + 40.0f,
                Board::MakeRenderOrder(RENDER_LAYER_PARTICLE, thePlant->mRow, 0), PARTICLE_MIND_CONTROL);
            thePlant->mBoard->mApp->PlaySample(SOUND_MINDCONTROLLED);
        }
    }

    // Ò¶×Ó±£»¤É¡£º·¶Î§ÄÚÖ²ÎïÕóÍöÊ±ÏûºÄ×ÔÉíÉúÃüµ¯¿ª¿ĞÊ³Õß
    if (thePlant->mSeedType != SeedType::SEED_UMBRELLA && thePlant->mBoard)
    {
        Plant* aUmbrella = thePlant->mBoard->FindUmbrellaPlant(thePlant->mPlantCol, thePlant->mRow);
        if (aUmbrella && aUmbrella->mPlantHealth > 5)
        {
            int aPushed = 0;
            Zombie* aZombie = nullptr;
            while (thePlant->mBoard->IterateZombies(aZombie))
            {
                if (aZombie->IsDeadOrDying() || aZombie->mRow != thePlant->mRow)
                    continue;

                if (fabsf(aZombie->mPosX - (float)thePlant->mX) <= 90.0f)
                {
                    aZombie->mPosX += 80.0f;
                    aZombie->TakeDamage(100, 0U);
                    aZombie->ApplyChill(false);
                    aPushed++;
                }
            }

            if (aPushed > 0)
            {
                aUmbrella->mPlantHealth -= 5 * aPushed;
                if (aUmbrella->mPlantHealth <= 0)
                {
                    aUmbrella->Die();
                }
            }
        }
    }

    int aGodNut = gThirtyFlags.GetStacks(TF_UPG_GOD_NUT);
    if (aGodNut > 0)
    {
        if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_TALLNUT ||
            thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL || thePlant->mSeedType == SeedType::SEED_EXPLODE_O_NUT)
        {
            thePlant->mBoard->KillAllZombiesInRadius(thePlant->mRow, thePlant->mX, thePlant->mY, 200, 0, false, 0);
        }
    }
}

int ThirtyFlagsSunflowerExtraSun(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 0;

    float aBonus = gThirtyFlags.GetSunflowerYieldBonus();
    int aExtra = (int)aBonus - 1;
    if (aExtra < 0)
        return 0;

    float aFraction = aBonus - (float)(int)aBonus;
    if (aFraction > 0.0f && Rand(1000) < (int)(aFraction * 1000.0f))
    {
        aExtra++;
    }

    return aExtra;
}

// ----------------------------------------------------------------------------------------------------
// ³¡µØ»æÖÆ£º²İÆ¤¹ö³ö¶¯»­ + ¾Ö²¿Ë®Ãæ + Î´¿ª·ÅĞĞºÚÄ»
// ----------------------------------------------------------------------------------------------------
// ÈıÊ®Æì³¡µØ»æÖÆ£ºÎ´¿ª·ÅĞĞ = ÂãÍÁ£¬ÒÑ¿ª·ÅĞĞ = ²İÆ¤£¨º¬¹ö¶¯ÆÌ¿ª¶¯»­£©£¬Ë®ĞĞ = ÕûĞĞË®Ãæ¡£
// È«²¿Ê¹ÓÃ²»À­ÉìµÄ DrawImage(img, x, y, srcRect)£¬ÈÆ¿ª DDImage::StretchBlt¡£
//
// ×ø±êËµÃ÷£ºDrawBackdrop ÔÚ Board µÄ×ø±êÏµÀïµ÷ÓÃ£¬Ô­µã±»Æ½ÒÆÁË -BOARD_OFFSET£¬
// Òò´Ë¡¸¿Í»§Çø x=0¡¹¶ÔÓ¦´Ë´¦µÄ x=-BOARD_OFFSET¡£²İÆº×ó½ç LAWN_XMIN=40 ÔÚ´Ë´¦ÊÇ 260¡£
static const int TF_BOARD_OFFSET = 220;
static const int TF_LAWN_LEFT = 260;          // ²İÆº×ó½çÔÚ Board ¾Ö²¿×ø±êÀïµÄ x
static const int TF_ROW_H = 100;              // ÈÕ¼ä²¼¾ÖĞĞ¸ß


// ====================================================================================================
// Final boss - "Doom Mech" (plan chapter 8)
//
// The stock ZOMBIE_BOSS already provides stomp / bungee / head-spit skills driven by DamageIndex,
// so this layer only adds what the plan asks for on top of it:
//   * a shared 3-phase health pool with an invulnerable phase transition,
//   * a telegraph-then-strike skill loop keyed to the current phase,
//   * P3 self destruct countdown.
// All effects reuse vanilla primitives (KillAllPlantsInRadius / AddZombieInRow / plant row rewrite),
// so no new art, animation or sound is required.
// ====================================================================================================

enum ThirtyFlagsBossSkill
{
    TF_BOSS_SKILL_NONE = 0,
    TF_BOSS_SKILL_SLAM,             // P1: crush a 2x2 block of plants
    TF_BOSS_SKILL_HORDE,            // P1: summon a wave of elites down two lanes
    TF_BOSS_SKILL_TERRAIN,          // P2: turn a whole row into water
    TF_BOSS_SKILL_EMP,              // P2: disable all shooters for 10s
    TF_BOSS_SKILL_ROAR,             // P2: board-wide zombie speed +50% for 8s
    TF_BOSS_SKILL_ROAR_ULTIMATE,    // P3: destroy every plant in the frontmost column
    TF_BOSS_SKILL_DOMAIN            // P3: zombies lifesteal and leave poison
};

constexpr int TF_BOSS_VANILLA_PHASE_HEALTH = TF_BOSS_HEALTH / 3;

bool ThirtyFlagsBossWantsBoss(int theFlag)
{
    return theFlag == TF_BOSS_FLAG;
}

bool ThirtyFlagsBossIsEmpActive()
{
    return ThirtyFlagsMode() && gThirtyFlags.mBossEmpTimer > 0;
}

bool ThirtyFlagsBossIsDomainActive()
{
    return ThirtyFlagsMode() && gThirtyFlags.mBossDomainTimer > 0;
}

static int TFBossPhaseForHealth(Zombie* theBoss)
{
    if (!theBoss || theBoss->mBodyMaxHealth <= 0)
        return 1;

    int aPercent = theBoss->mBodyHealth * 100 / theBoss->mBodyMaxHealth;
    if (aPercent <= TF_BOSS_PHASE3_PERCENT)
        return 3;
    if (aPercent <= TF_BOSS_PHASE2_PERCENT)
        return 2;
    return 1;
}

// Frames until the next skill for the given phase.
static int TFBossSkillInterval(int thePhase, int theSkill)
{
    switch (theSkill)
    {
    case TF_BOSS_SKILL_SLAM:     return TF_BOSS_SLAM_INTERVAL;
    case TF_BOSS_SKILL_HORDE:    return TF_BOSS_HORDE_INTERVAL;
    case TF_BOSS_SKILL_TERRAIN:  return TF_BOSS_TERRAIN_INTERVAL;
    case TF_BOSS_SKILL_EMP:      return 1200;
    case TF_BOSS_SKILL_ROAR:     return TF_BOSS_ROAR_INTERVAL;
    case TF_BOSS_SKILL_ROAR_ULTIMATE: return TF_BOSS_ROAR_INTERVAL;
    case TF_BOSS_SKILL_DOMAIN:   return TF_BOSS_DOMAIN_INTERVAL;
    default:                     return 600;
    }
}

// Pick the next skill for a phase (alternates its two signature skills).
static int TFBossPickSkill(int thePhase, int theLastSkill)
{
    if (thePhase == 1)
        return (theLastSkill == TF_BOSS_SKILL_SLAM) ? TF_BOSS_SKILL_HORDE : TF_BOSS_SKILL_SLAM;
    if (thePhase == 2)
    {
        if (theLastSkill == TF_BOSS_SKILL_TERRAIN) return TF_BOSS_SKILL_EMP;
        if (theLastSkill == TF_BOSS_SKILL_EMP)     return TF_BOSS_SKILL_ROAR;
        return TF_BOSS_SKILL_TERRAIN;
    }
    if (theLastSkill == TF_BOSS_SKILL_ROAR_ULTIMATE) return TF_BOSS_SKILL_DOMAIN;
    if (theLastSkill == TF_BOSS_SKILL_DOMAIN)        return TF_BOSS_SKILL_ROAR_ULTIMATE;
    return TF_BOSS_SKILL_ROAR_ULTIMATE;
}

static void TFBossSummonBoss(Board* theBoard)
{
    Zombie* aBoss = theBoard->AddZombieInRow(ZOMBIE_BOSS, Rand(TF_LAWN_ROWS), theBoard->mCurrentWave);
    if (!aBoss)
        return;

    aBoss->mBodyHealth = TF_BOSS_HEALTH;
    aBoss->mBodyMaxHealth = TF_BOSS_HEALTH;
    gThirtyFlags.mBossPtr = aBoss;
    gThirtyFlags.mBossPhase = 1;
    gThirtyFlags.mBossSkillTimer = TF_BOSS_SLAM_INTERVAL;
    gThirtyFlags.mBossSelfDestruct = 0;



    gLawnApp->TfLog("boss summoned hp=%d row=%d", aBoss->mBodyMaxHealth, aBoss->mRow);

}

// P1 slam: crush a 2x2 block of plants just in front of the boss.
static void TFBossDoSlam(Board* theBoard, Zombie* theBoss, int theRow)
{
    int aCol = TFClampI(theBoard->PixelToGridX((int)theBoss->mPosX, (int)theBoss->mPosY), 0, MAX_GRID_SIZE_X - 1);
    int aMinCol = max(0, aCol - 2);

    int aPixelX = theBoard->GridToPixelX(aMinCol, theRow);
    int aPixelY = theBoard->GridToPixelY(aMinCol, theRow);

    // ¡¾ÈıÊ®Æì¡¿ÔÒ»÷Ò²ÒªÓĞ¿É¼û·´À¡£¬·ñÔòÕû¿éÖ²ÎïÉÁµô¾ÍÊÇ¡¸Æ¾¿ÕÏûÊ§¡¹¡£
    theBoard->mApp->AddTodParticle((float)aPixelX, (float)aPixelY,
        Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0), ParticleEffect::PARTICLE_BLASTMARK);

    // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Ô­À´ÕâÀïÊÇ 2x2 Ö±½Ó Die() ÔÙµşÒ»´Î KillAllPlantsInRadius(60)£¬
    // Ë«ÖØÃëÉ±£»ÏÖÔÚÍ³Ò»×ß¡¸Ö±½ÓÉËº¦¡¹£º¿Û TF_BOSS_SLAM_DAMAGE£¬Ì¹¿ËÖ²Îï¿¸µÃ×¡£¬
    // ÇÒÄÏ¹ÏÓÅÏÈ³ĞÉË¡£
    int aKilled = TFDirectDamageInRadius(theBoard, aPixelX, aPixelY, 110, TF_BOSS_SLAM_DAMAGE);
    {
        char aBuf[160];
        sprintf(aBuf, "[TFPlant] boss slam at px=(%d,%d) dmg=%d killed=%d",
            aPixelX, aPixelY, TF_BOSS_SLAM_DAMAGE, aKilled);
        TFLog(aBuf);
    }
    theBoard->mApp->PlayFoley(FOLEY_THUNDER);
}

// P1 horde: six elites split across two lanes.
static void TFBossDoHorde(Board* theBoard, int theRow)
{
    static const ZombieType aPool[] = { ZOMBIE_NORMAL, ZOMBIE_PAIL, ZOMBIE_FOOTBALL, ZOMBIE_GARGANTUAR, ZOMBIE_DOOR, ZOMBIE_LADDER };
    int aOtherRow = (theRow + 2) % TF_LAWN_ROWS;

    for (int i = 0; i < 6; i++)
    {
        int aRow = (i % 2 == 0) ? theRow : aOtherRow;
        Zombie* aZombie = theBoard->AddZombieInRow(aPool[Rand(6)], aRow, theBoard->mCurrentWave);
        if (!aZombie)
            continue;

        // reuse the elite path so summoned zombies carry the same modifiers
        ThirtyFlagsZombieInit(aZombie);
        aZombie->mPosX = (float)(MAX_GRID_SIZE_X * 80 + 40 + i * 20);
    }
}

// P2 terrain: turn a row into water; anything without a lilypad drowns.
static void TFBossDoTerrain(Board* theBoard, int theRow)
{
    theBoard->mPlantRow[theRow] = PLANTROW_POOL;
    for (int aC = 0; aC < MAX_GRID_SIZE_X; aC++)
    {
        theBoard->mGridSquareType[aC][theRow] = GRIDSQUARE_POOL;

        Plant* aPlant = theBoard->GetTopPlantAt(aC, theRow, TOPPLANT_ANY);
        if (!aPlant || aPlant->mDead || aPlant->mSeedType == SEED_LILYPAD || aPlant->mSeedType == SEED_TANGLEKELP)
            continue;

        // ¡¾ÈıÊ®Æì¡¿ÑÍËÀµÄÖ²ÎïÒ²ÒªÓĞ¿É¼û·´À¡£¨·ñÔòÓÖÊÇÒ»¸ö¡¸Æ¾¿ÕÏûÊ§¡¹£©
        int aSplashOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
        theBoard->mApp->AddTodParticle((float)theBoard->GridToPixelX(aC, theRow),
            (float)theBoard->GridToPixelY(aC, theRow), aSplashOrder, ParticleEffect::PARTICLE_POOL_SPLASH);

        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿Ì×ÔÚÄÏ¹ÏÀïµÄÖ²Îï²»»áÑÍËÀ£ºÓÉÄÏ¹ÏÌæËü³ÁÏÂÈ¥¡£
        Plant* aDrown = TFDirectDamageTarget(theBoard, aC, theRow);
        if (aDrown)
        {
            theBoard->mPlantsEaten++;
            aDrown->Die();
        }
    }

    gThirtyFlags.mWaterRows |= (1 << theRow);
    gThirtyFlags.mWaterCols = MAX_GRID_SIZE_X;
}

// P3 ultimate: destroy every plant in the frontmost occupied column.
static void TFBossDoRoarUltimate(Board* theBoard)
{
    int aFrontCol = -1;
    for (int aC = 0; aC < MAX_GRID_SIZE_X && aFrontCol < 0; aC++)
    {
        for (int aR = 0; aR < TF_LAWN_ROWS; aR++)
        {
            Plant* aPlant = theBoard->GetTopPlantAt(aC, aR, TOPPLANT_ANY);
            if (aPlant && !aPlant->mDead)
            {
                aFrontCol = aC;
                break;
            }
        }
    }

    if (aFrontCol < 0)
        return;

    int aKilled = 0;
    for (int aR = 0; aR < TF_LAWN_ROWS; aR++)
    {
        Plant* aPlant = theBoard->GetTopPlantAt(aFrontCol, aR, TOPPLANT_ANY);
        if (!aPlant || aPlant->mDead)
            continue;

        // ¡¾ÈıÊ®Æì¡¿ÕûÁĞÖ²Îï±»´İ»ÙÒ²ÒªÓĞ·´À¡£¬·ñÔòÏñ¡¸ÕûÁĞÆ¾¿ÕÏûÊ§¡¹¡£
        theBoard->mApp->AddTodParticle((float)aPlant->mX, (float)aPlant->mY,
            Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, aR, 0), ParticleEffect::PARTICLE_BLASTMARK);

        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿ÕûÁĞÃëÉ± ¡ú Ö±½ÓÉËº¦£»ÄÏ¹ÏÓÅÏÈ³ĞÉË£¬×°ÔÚÄÏ¹ÏÀïµÄÖ²Îïµô²»¶¯
        Plant* aVictim = TFDirectDamageTarget(theBoard, aFrontCol, aR);
        if (!aVictim)
            continue;

        aVictim->mPlantHealth -= TF_BOSS_ULTIMATE_DAMAGE;
        if (aVictim->mPlantHealth <= 0)
        {
            theBoard->mPlantsEaten++;
            aVictim->Die();
            aKilled++;
        }
    }
    {
        char aBuf[160];
        sprintf(aBuf, "[TFPlant] boss ultimate col=%d dmg=%d killed=%d",
            aFrontCol, TF_BOSS_ULTIMATE_DAMAGE, aKilled);
        TFLog(aBuf);
    }
    theBoard->mApp->PlayFoley(FOLEY_THUNDER);
}

// Execute a telegraphed skill.
static void TFBossExecuteSkill(Board* theBoard, Zombie* theBoss, int theSkill, int theRow)
{
    switch (theSkill)
    {
    case TF_BOSS_SKILL_SLAM:
        TFBossDoSlam(theBoard, theBoss, theRow);
        break;

    case TF_BOSS_SKILL_HORDE:
        TFBossDoHorde(theBoard, theRow);
        break;

    case TF_BOSS_SKILL_TERRAIN:
        TFBossDoTerrain(theBoard, theRow);
        break;

    case TF_BOSS_SKILL_EMP:
        gThirtyFlags.mBossEmpTimer = TF_BOSS_EMP_FRAMES;
        break;

    case TF_BOSS_SKILL_ROAR:
        gThirtyFlags.mBossRoarTimer = TF_BOSS_ROAR_FRAMES;
        break;

    case TF_BOSS_SKILL_ROAR_ULTIMATE:
        TFBossDoRoarUltimate(theBoard);
        break;

    case TF_BOSS_SKILL_DOMAIN:
        gThirtyFlags.mBossDomainTimer = TF_BOSS_ROAR_FRAMES;
        break;

    default:
        break;
    }
}

// Zombie slots are recycled, so a cached pointer is only trusted while the zombie is
// still present in the board array and alive.
static Zombie* TFBossResolve(Board* theBoard)
{
    Zombie* aCached = gThirtyFlags.mBossPtr;
    if (aCached)
    {
        // ¡¾ĞÔÄÜ¡¿Ô­À´ÕâÀïÃ¿Ö¡ÏßĞÔÉ¨Ò»±é½©Ê¬³Ø£¨×î¶à 1024 ¸ö¼¸°Ù×Ö½ÚµÄ´ó²Û£©
        // Ö»ÎªÈ·ÈÏ"»º´æµÄÖ¸Õë»¹ÓĞĞ§"£»µ«»º´æÖ¸Õë¾ÍÊÇ³ØÀïµÄ²ÛµØÖ·£¬
        // ²Û×Ô¼ºµÄ mID ¾ÍÄÜÅĞ¶¨»î·ñ ¡ª¡ª O(1) ¾Í¹»¡£
        // £¨Ë³´øĞŞµôÔ­À´ÄÇ¸öÅĞ¶¨£º`mID & DATA_ARRAY_INDEX_MASK` È¡µÄÊÇÏÂ±ê£¬
        //   ºã²»µÈÓÚ 65535£¬µÈÓÚÃ»ÅĞ£»ÕıÈ·ÅĞ¾İÊÇ key ²¿·Ö·ÇÁã¡££©
        DataArray<Zombie>::DataArrayItem* aSlot = (DataArray<Zombie>::DataArrayItem*)aCached;
        bool aInRange = aSlot >= &theBoard->mZombies.mBlock[0] &&
                        aSlot < &theBoard->mZombies.mBlock[theBoard->mZombies.mMaxUsedCount];
        if (aInRange && (aSlot->mID & DATA_ARRAY_KEY_MASK))
        {
            return aCached->mDead ? NULL : aCached;
        }
    }

    // [ThirtyFlags] After a save reload every entity pointer is stale, so fall back to a scan.
    // Without this the boss would be lost and re-summoned as a second one on flag 30.
    Zombie* aZombie = nullptr;
    while (theBoard->IterateZombies(aZombie))
    {
        if (aZombie->mZombieType == ZOMBIE_BOSS && !aZombie->mDead)
        {
            gThirtyFlags.mBossPtr = aZombie;
            return aZombie;
        }
    }
    return NULL;
}

void ThirtyFlagsBossUpdate(Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    int aFlag = ThirtyFlagsCurrentFlag(theBoard);

    // spawn on the final flag
    if (gThirtyFlags.mBossPhase == 0)
    {
        if (!ThirtyFlagsBossWantsBoss(aFlag))
            return;
        // ½©ÍõµÄÃÀÊõ×ÊÔ´²»ÔÚ³£¹æÔ¤¼ÓÔØÇåµ¥Àï£¨°×Ãûµ¥Ö»¶ÔµÚ 30 Æì·Å¿ª£©£¬ÏÔÊ½²¹Ò»´Î¡£

        Zombie::PreloadZombieResources(ZOMBIE_BOSS);

        TFBossSummonBoss(theBoard);

        return;
    }

    Zombie* aBoss = TFBossResolve(theBoard);
    if (!aBoss || aBoss->mDead)
    {
        gThirtyFlags.mBossPhase = 0;
        gThirtyFlags.mBossPtr = NULL;
        return;
    }

    // phase transition (with a brief invulnerable window)
    int aWantPhase = TFBossPhaseForHealth(aBoss);
    if (aWantPhase > gThirtyFlags.mBossPhase)
    {
        gThirtyFlags.mBossPhase = aWantPhase;
        gThirtyFlags.mBossSkillTimer = TFBossSkillInterval(aWantPhase, TF_BOSS_SKILL_NONE);
        gThirtyFlags.mBossWarnTimer = 0;

        theBoard->mApp->PlayFoley(FOLEY_THUNDER);

        gLawnApp->TfLog("boss phase -> %d", aWantPhase);
    }

    // P3 self destruct countdown
    if (gThirtyFlags.mBossPhase == 3)
    {
        int aPercent = aBoss->mBodyHealth * 100 / max(1, aBoss->mBodyMaxHealth);
        if (aPercent <= TF_BOSS_SELF_DESTRUCT_PERCENT && gThirtyFlags.mBossSelfDestruct == 0)
            gThirtyFlags.mBossSelfDestruct = TF_BOSS_SELF_DESTRUCT_FRAMES;

        if (gThirtyFlags.mBossSelfDestruct > 0)
        {
            gThirtyFlags.mBossSelfDestruct--;
            if (gThirtyFlags.mBossSelfDestruct == 0)
            {
                // The mech detonates: it dies outright, which ends the fight as a win.
                aBoss->mBodyHealth = 0;
                aBoss->DieNoLoot();
                theBoard->mApp->PlayFoley(FOLEY_BOSS_EXPLOSION_SMALL);
            }
        }
    }

    // skill timers
    if (gThirtyFlags.mBossEmpTimer > 0)
        gThirtyFlags.mBossEmpTimer--;
    if (gThirtyFlags.mBossRoarTimer > 0)
        gThirtyFlags.mBossRoarTimer--;
    if (gThirtyFlags.mBossDomainTimer > 0)
        gThirtyFlags.mBossDomainTimer--;

    // telegraph resolves first
    if (gThirtyFlags.mBossWarnTimer > 0)
    {
        gThirtyFlags.mBossWarnTimer--;
        if (gThirtyFlags.mBossWarnTimer == 0)
        {
            TFBossExecuteSkill(theBoard, aBoss, gThirtyFlags.mBossWarnSkill, gThirtyFlags.mBossWarnRow);
            gThirtyFlags.mBossSkillTimer = TFBossSkillInterval(gThirtyFlags.mBossPhase, gThirtyFlags.mBossWarnSkill);
            gThirtyFlags.mBossWarnSkill = TF_BOSS_SKILL_NONE;
        }
        return;
    }

    if (gThirtyFlags.mBossSkillTimer > 0)
    {
        gThirtyFlags.mBossSkillTimer--;
        return;
    }

    // start a new telegraph
    int aSkill = TFBossPickSkill(gThirtyFlags.mBossPhase, gThirtyFlags.mBossWarnSkill);
    gThirtyFlags.mBossWarnSkill = aSkill;
    gThirtyFlags.mBossWarnRow = TFClampI(aBoss->mRow, 0, TF_LAWN_ROWS - 1);
    gThirtyFlags.mBossWarnTimer = TF_BOSS_WARN_FRAMES;
}

void ThirtyFlagsDrawBackdrop(Board* theBoard, Graphics* g)
{
    if (!ThirtyFlagsMode() || !theBoard || !g)
        return;

    const int aBgLeft = -BOARD_OFFSET;

    // ---- 1) Î´¿ª·ÅµÄĞĞ£ºÓÃ¡¸Î´ÆÌ²İÆ¤¡¹µ×Í¼µÄ¶ÔÓ¦Ìõ´ø¸²¸Ç£¬³ÊÏÖÂãÍÁ ----
    // Á½¿éÌùÍ¼£¨BACKGROUND1 / BACKGROUND1UNSODDED£©µÄ²İÆºÇøÓò¹¹Í¼Ò»ÖÂ£¬Î¨Ò»²î±ğ¾ÍÊÇÓĞÃ»ÓĞ²İÆ¤£¬
    // Òò´Ë°´Í¬ĞĞÍ¬ÁĞÈ¡Ìõ´ø¼´¿É¾«È·¶ÔÆë£¬ÇÒ**²»ĞèÒªÀ­Éì**¡£
    if (Sexy::IMAGE_BACKGROUND1UNSODDED != nullptr)
    {
        // ¡¾ĞÔÄÜ¡¿Á¬ĞøÎ´¿ª·ÅµÄĞĞºÏ²¢³ÉÒ»´Î blit¡£
        // ÌùÍ¼Óë³¡µØµÄ×İÏò²¼¾ÖÒ»ÖÂ£¨Ô­À´µÄÖğĞĞ°æ±¾¾ÍÊÇ¡¸Ô´ y = Ä¿±ê y¡¹£©£¬
        // ËùÒÔ°ÑÏàÁÚĞĞÆ´³ÉÒ»¸ö¸ü¸ßµÄÔ´¾ØĞÎ£¬»­³öÀ´µÄÏñËØÓëÖğĞĞ»­**ÍêÈ«Ò»ÖÂ**£¬
        // Ö»ÊÇ»æÖÆµ÷ÓÃ¸üÉÙ£¨5 ĞĞÀï 4 ĞĞËø×ÅÊ±£º4 ´Î ¡ú 2 ´Î£©¡£
        int aRowPitch = theBoard->GridToPixelY(0, 1) - theBoard->GridToPixelY(0, 0);
        int aRunStart = -1;
        int aSrcX = TF_LAWN_LEFT + BOARD_OFFSET;        // ×ª»ØÌùÍ¼×ø±ê

        for (int aRow = 0; aRow <= MAX_GRID_SIZE_Y && aRow <= TF_LAWN_ROWS; aRow++)
        {
            bool aLocked = (aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS) && !gThirtyFlags.IsRowUnlocked(aRow);

            if (aLocked)
            {
                if (aRunStart < 0)
                    aRunStart = aRow;
                continue;
            }

            if (aRunStart < 0)
                continue;

            int aY = theBoard->GridToPixelY(0, aRunStart);
            int aHeight = (aRow - aRunStart - 1) * aRowPitch + TF_ROW_H;
            Rect aSrc(aSrcX, aY, MAX_GRID_SIZE_X * 80, aHeight);
            g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, TF_LAWN_LEFT, aY, aSrc);
            aRunStart = -1;
        }
    }

    // ---- 2) ²İÆ¤¹ö¶¯ÆÌ¿ª£º°´½ø¶È´Ó×óÏòÓÒ½ÒÊ¾ IMAGE_SOD1ROW ----
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS; aRow++)
    {
        if (!gThirtyFlags.IsRowUnlocked(aRow))
            continue;

        int aProgress = gThirtyFlagsSodProgress[aRow];
        if (aProgress <= 0 || aProgress >= 1000)
            continue;                       // Î´¿ªÊ¼ / ÒÑÆÌÍê£¨±³¾°×Ô´ø²İÆ¤£©

        if (Sexy::IMAGE_SOD1ROW == nullptr)
            continue;

        int aY = theBoard->GridToPixelY(0, aRow);
        int aFullWidth = Sexy::IMAGE_SOD1ROW->GetWidth();
        int aWidth = (int)((float)aFullWidth * (float)aProgress / 1000.0f);
        if (aWidth <= 0)
            continue;

        Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD1ROW->GetHeight());
        g->DrawImage(Sexy::IMAGE_SOD1ROW, TF_LAWN_LEFT - 40, aY, aSrcRect);
    }

    // ---- 3) Ë®Â·£ºÕûĞĞÆÌ IMAGE_POOL ----
    if (gThirtyFlags.mWaterRows != 0 && Sexy::IMAGE_POOL != nullptr &&
        Sexy::IMAGE_POOL->GetWidth() > 0 && Sexy::IMAGE_POOL->GetHeight() > 0)
    {
        int aPoolW = Sexy::IMAGE_POOL->GetWidth();
        int aPoolH = Sexy::IMAGE_POOL->GetHeight();

        for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS; aRow++)
        {
            if (!gThirtyFlags.IsWaterRow(aRow))
                continue;

            int aY = theBoard->GridToPixelY(0, aRow);
            g->DrawImage(Sexy::IMAGE_POOL, TF_LAWN_LEFT - 30, aY - 5, Rect(0, 0, aPoolW, aPoolH));
        }
    }

#ifndef TF_PLAYER_BUILD
    {
        SexyString aLine1 = StrFormat(_S("FLAG %d/30"), gThirtyFlags.mFlag);
        SexyString aLine2 = StrFormat(_S("unlock %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? 1 : 0, gThirtyFlags.IsRowUnlocked(1) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(2) ? 1 : 0, gThirtyFlags.IsRowUnlocked(3) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(4) ? 1 : 0);
        SexyString aLine3 = StrFormat(_S("water %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? (gThirtyFlags.IsWaterRow(0) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(1) ? (gThirtyFlags.IsWaterRow(1) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(2) ? (gThirtyFlags.IsWaterRow(2) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(3) ? (gThirtyFlags.IsWaterRow(3) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(4) ? (gThirtyFlags.IsWaterRow(4) ? 1 : 0) : 9);
        SexyString aLine4 = StrFormat(_S("upg %d  elite %d%%  kill %d"),
            gThirtyFlags.mUpgradeCount, gThirtyFlags.GetEliteChancePercent(), gThirtyFlags.mTotalKills);

        g->SetFont(Sexy::FONT_DWARVENTODCRAFT12);
        g->SetColor(Color(0, 0, 0, 190));
        g->FillRect(2, 2, 152, 64);
        g->SetColor(Color(255, 255, 80));
        g->DrawString(aLine1, 8, 5);
        g->DrawString(aLine2, 8, 20);
        g->DrawString(aLine3, 8, 35);
        g->SetColor(Color(140, 255, 140));
        g->DrawString(aLine4, 8, 50);
    }
#endif
}
// -------------------------------------------------------------------------------------------
// [ThirtyFlags] Ğ¡ÍÆ³µ²¹Î»
//
// Ô­°æÖ»ÔÚ CutScene::PlaceLawnItems() Àïµ÷ Board::InitLawnMowers()£¬¶øËü±» IsSurvivalRepick()
// µ²×¡£¨ÈıÊ®ÆìËãÉú´æÄ£Ê½£¬ÇÒµÚ 2 ÆìÆğ mSurvivalStage > 0£©£¬ËùÒÔÔ­°æ´ÓµÚ 2 ÆìÆğÔÙÒ²²»½¨³µ¡£
//
// ¸üÒş±ÎµÄ¿Ó£ºLawnMowerInitialize() °Ñ mPosX Éè³É -160£¨ÆÁÄ»Íâ£©£¬ÕæÕı°Ñ³µÅ²½ø³¡µÄÊÇ
// CutScene::Update() ÀïµÄ mPosX = CalcPosition(..., -80, -21) ¡ª¡ª ÄÇ¶ÎÍ¬Ñù±» IsSurvivalRepick()
// Ìø¹ı¡£Ò²¾ÍÊÇËµÖ»×ö DataArrayAlloc + LawnMowerInitialize£¬³µ»á±»½¨³öÀ´µ«ÓÀÔ¶Í£ÔÚÆÁÄ»Íâ£¬
// ÈâÑÛ¿´ÉÏÈ¥¾ÍÊÇ¡¸Ğ¡ÍÆ³µ²»³öÏÖ¡¹¡£²¹³µÊ±±ØĞë×Ô¼º°Ñ³µÅ²µ½Î»¡£
// -------------------------------------------------------------------------------------------
static void TFSpawnLawnMower(Board* theBoard, int theRow)
{
    if (!theBoard || theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
        return;
    if (theBoard->mPlantRow[theRow] == PlantRowType::PLANTROW_DIRT)
        return;

    LawnMower* aMower = theBoard->mLawnMowers.DataArrayAlloc();
    if (!aMower)
    {
        TFLog("[TFMower] spawn FAILED: pool exhausted");
        return;
    }

    aMower->LawnMowerInitialize(theRow);            // ×¢Òâ£ºÄÚ²¿°Ñ mPosX ÉèÎª -160£¨ÆÁÍâ£©
    aMower->mVisible = true;
    aMower->mRollingInCounter = 0;
    aMower->mMowerState = LawnMowerState::MOWER_ROLLING_IN;   // Ô­°æÈë³¡¶¯»­£º100 Ö¡ÄÚ -160 -> -21

    // [patch] Éú³É½á¹ûÂäÈÕÖ¾£¬ÓÃÓÚÅÅ²é¡¸Ğ¡ÍÆ³µµ½µ×ÓĞÃ»ÓĞ±»½¨³öÀ´¡¹¡£
    {
        char aBuf[128];
        sprintf(aBuf, "[TFMower] spawn row=%d posX=%.1f visible=%d state=%d",
            theRow, aMower->mPosX, aMower->mVisible ? 1 : 0, (int)aMower->mMowerState);
        TFLog(aBuf);
    }

    // ¡¾ÈıÊ®Æì¡¿¼ÇÏÂ¡¸ÕâĞĞÒÑ·¢¹ı³µ¡¹£¬¶ª³µÒÔºó²»»áÆ¾¿Õ²¹»ØÀ´¡£
    gThirtyFlagsMowerGranted |= (1 << theRow);
}
// -------------------------------------------------------------------------------------------
// [ThirtyFlags] Ğ¡ÍÆ³µ°²È«Íø
//
// Ã¿Ö¡ÅĞ¶ÏÒ»±é£ºÒÑ½âËø¡¢Î´·¢¹ı³µ¡¢µ±Ç°Ã»³µ -> ²¹Ò»Á¾¡£
// ÎªÊ²Ã´ÒªÓÃÃ¿Ö¡ÅĞ¶Ï£¬¶ø²»ÊÇ¡¸½âËøÄÇÒ»Ö¡²¹Ò»´Î¡¹£º
//   * ½âËøÄÇÒ»Ö¡ mPlantRow ¿ÉÄÜ»¹Ã»´Ó DIRT ¸Ä¹ıÀ´£¬²¹³µ»á±»Ìõ¼şµ²×¡
//   * ¶Á¾É´æµµ½øÈëÊ±£¬ÒÑ½âËøµÄĞĞ²»»á×ß¡¸ĞÂ½âËøÄÇÒ»Ö¡¡¹µÄÂß¼­£¬ÓÀÔ¶²¹²»µ½³µ
// ¶ø granted Î»Í¼±£Ö¤¡¸ÒÑ¾­·¢¹ı¡¹µÄĞĞ²»»áÖØ¸´²¹£¬ËùÒÔÍæ¼Ò¶ªµôµÄ³µ²»»áÆ¾¿Õ¸´»î¡£
// -------------------------------------------------------------------------------------------
static void TFEnsureRowLawnMowers(Board* theBoard)
{
    if (!theBoard || !gLawnApp || gLawnApp->mGameScene != GameScenes::SCENE_PLAYING)
        return;

    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < 5; aRow++)
    {
        if (!gThirtyFlags.IsRowUnlocked(aRow))
            continue;
        if (gThirtyFlagsMowerGranted & (1 << aRow))
            continue;
        if (theBoard->FindLawnMowerInRow(aRow) != nullptr)
        {
            // Ô­°æ¿ª³¡ÑİÊ¾ÒÑ¾­°Ñ³µ·ÅºÃÁË£¬Ö±½ÓÈÏÁì
            {
                char aBuf[128];
                sprintf(aBuf, "[TFMower] adopt existing mower row=%d", aRow);
                TFLog(aBuf);
            }
            gThirtyFlagsMowerGranted |= (1 << aRow);
            continue;
        }
        TFSpawnLawnMower(theBoard, aRow);
    }
}
void ThirtyFlagsUpdateSod(Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    // ¼ì²âĞÂ½âËøµÄĞĞ -> Æô¶¯¹ö¶¯
    int aUnlocked = gThirtyFlags.mUnlockedRows;
    int aNewly = aUnlocked & ~gThirtyFlagsPrevUnlockedRows;
    if (aNewly != 0)
    {
        for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < 5; aRow++)
        {
            if (aNewly & (1 << aRow))
            {
                gThirtyFlagsSodProgress[aRow] = 1;
                {
                    char aBuf[160];
                    sprintf(aBuf, "[TFMower] row %d unlocked, plantRow=%d hasMower=%d",
                        aRow, (int)theBoard->mPlantRow[aRow],
                        theBoard->FindLawnMowerInRow(aRow) != nullptr ? 1 : 0);
                    TFLog(aBuf);
                }
            }
        }
        gThirtyFlagsPrevUnlockedRows = aUnlocked;
    }

    // ¡¾ÈıÊ®Æì¡¿Ğ¡ÍÆ³µ°²È«Íø£ºÃ¿Ö¡²¹Æë¡£
    TFEnsureRowLawnMowers(theBoard);

    // ÍÆ½ø¶¯»­
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < 5; aRow++)
    {
        if (gThirtyFlagsSodProgress[aRow] > 0 && gThirtyFlagsSodProgress[aRow] < 1000)
        {
            gThirtyFlagsSodProgress[aRow] = min(1000, gThirtyFlagsSodProgress[aRow] + 25);
        }
    }

    // Ë®¸ñµ­Èë
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            int aTarget = gThirtyFlags.IsWaterCell(y, x) ? 100 : 0;
            if (gThirtyFlagsWaterFade[x][y] < aTarget)
            {
                gThirtyFlagsWaterFade[x][y] = min(aTarget, gThirtyFlagsWaterFade[x][y] + 5);
            }
            else if (gThirtyFlagsWaterFade[x][y] > aTarget)
            {
                gThirtyFlagsWaterFade[x][y] = max(aTarget, gThirtyFlagsWaterFade[x][y] - 5);
            }
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// ³¡µØÃ¿Ö¡
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsBoardUpdate(Board* theBoard)
{
    ThirtyFlagsTickMarks();   // ThirtyFlags v5: break-mark timer
    ThirtyFlagsTickCorrupt(); // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¸¯»¯¼ÆÊ±Æ÷
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    // ¡¾±íÏÖ²ã¡¿ÔªËØµ¯ÍÏÎ²£º´øÔªËØµÄÖ±Éäµ¯Ã¿ 3 Ö¡ÁôÒ»Ã¶¼ÓÉ«¹âµã£¨°´ÔªËØÅäÉ«£©¡£
    // ÓÃ mMainCounter ÏŞÁ÷£¬±ÜÃâÃ¿Ö¡Ã¿µ¯¶¼Éú³É£»Í¬Í¼Í¬Ä£Ê½£¬×îÖÕ»á±»ºÏÅú¡£
    if ((theBoard->mMainCounter % 3) == 0)
    {
        Projectile* aProj = nullptr;
        while (theBoard->IterateProjectiles(aProj))
        {
            if (aProj->mDead || aProj->mElement == 0)
                continue;

            int aR, aG, aB;
            TfFxGetElementColor(aProj->mElement, aR, aG, aB);
            TfFxSpawn(TF_FX_TRAIL, aProj->mPosX + 12.0f, aProj->mPosY + 10.0f,
                14.0f, -0.35f, 10, 110, aR, aG, aB);
        }
    }

    // ¡¾ÈıÊ®Æì¡¿¹Ø¿¨½áÊøÅĞ¶¨ĞŞÕı£¨²ß»®°¸µÚ 2 ÕÂ£©£º
    // ²¨´Î±íÒ»´Î½¨Âú 180 ²¨£¬µ«¡°Ò»ÃæÆì¡±Ö»ÓĞ 6 ²¨¡£°Ñ mNumWaves Ğ£ÕıÎª**±¾ÆìÄ©²¨**£¬
    // ÕâÑùÃ¿´òÂú 6 ²¨ ¡ú ³¡ÃæÇå¿Õ ¡ú FadeOutLevel ¡ú CheckForGameEnd ¡ú
    // mSurvivalStage++ / AdvanceFlag ¡ú InitSurvivalStage ¡ú ShowSeedChooserScreen£¨Ñ¡¿¨£©¡£
    // Ô­ÏÈ mNumWaves ºãÎª 180£¬µ¼ÖÂÒª´òÂú 180 ²¨²ÅµÚÒ»´Î½øÑ¡¿¨£¬Æì´Î/²İÆ¤/Ç¿»¯È«²¿²»ÍÆ½ø¡£
    theBoard->mNumWaves = gThirtyFlags.mFlag * TF_WAVES_PER_FLAG;

    // ¡¾ÈıÊ®Æì¡¿Á÷³ÌÈÕÖ¾£ºÆì´Î/²¨ºÅ±ä»¯Ê±¼ÇÂ¼Ò»´Î£¨ÅÅ²éÊµ²âÎÊÌâÓÃ£©
    {
        static int aLastFlag = -1;
        static int aLastWave = -1;
        if (theBoard->mCurrentWave != aLastWave || gThirtyFlags.mFlag != aLastFlag)
        {
            aLastWave = theBoard->mCurrentWave;
            aLastFlag = gThirtyFlags.mFlag;
            char aBuf[128];
            sprintf(aBuf, "flag=%d wave=%d numWaves=%d stage=%d", gThirtyFlags.mFlag,
                theBoard->mCurrentWave, theBoard->mNumWaves, theBoard->mChallenge->mSurvivalStage);
            TFLog(aBuf);
        }
    }

    ThirtyFlagsUpdateSod(theBoard);
    gThirtyFlags.UpdateKillStreak();
    ThirtyFlagsBossUpdate(theBoard);

    if (gThirtyFlags.mIntermission && gThirtyFlags.mPrepTimeLeft > 0)
    {
        gThirtyFlags.mPrepTimeLeft--;
    }

    // Õû±¸½×¶ÎÇÒ³¡ÉÏÒÑÇå¿ÕÊ±£¬µ¯³öÈâ¸ëÇ¿»¯ÈıÑ¡Ò»
    if (gThirtyFlags.mIntermission && !gThirtyFlags.mUpgradeChosen && !gThirtyFlags.mUpgradeDialogShown)
    {
        bool aAnyZombie = false;
        Zombie* aCheck = nullptr;
        while (theBoard->IterateZombies(aCheck))
        {
            if (!aCheck->IsDeadOrDying())
            {
                aAnyZombie = true;
                break;
            }
        }

        if (!aAnyZombie)
        {
            gThirtyFlags.mUpgradeDialogShown = true;
            TFLog("upgrade dialog SHOWN");
            theBoard->mApp->DoDialog(Dialogs::DIALOG_THIRTY_FLAGS, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE);
        }
    }

    int aStopFrames = gThirtyFlags.GetTimeStopSeconds() * 10;
    bool aTimeStopped = (aStopFrames > 0) && (theBoard->mMainCounter < aStopFrames);

    float aGlobalSpeed = gThirtyFlags.GetSpeedScale();

    Zombie* aZombie = nullptr;
    while (theBoard->IterateZombies(aZombie))
    {
        if (aZombie->IsDeadOrDying() || aZombie->mZombieType == ZOMBIE_BOSS)
            continue;

        float aMul = aGlobalSpeed;

        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿¸¯»¯¼õËÙ£ºÃ¿²ã -12%£¨×î¶à 4 ²ã = -48%£©
        {
            int aCorrupt = ThirtyFlagsGetCorrupt(aZombie, theBoard);
            if (aCorrupt > 0)
            {
                aMul *= max(0.40f, 1.0f - (float)TF_CORRUPT_SLOW_PERCENT / 100.0f * (float)aCorrupt);
            }
        }

        if (TFHasElite(aZombie, ELITE_SWIFT))

        {

            aMul *= 1.60f;

        }



        // ¡¾ÈıÊ®Æì¡¿½©Íõ P2¡¸Ê¬ÍõºÅÁî¡¹£ººÅÁîÆÚ¼äÈ«Ìå½©Ê¬ÒÆËÙ +50%¡£

        if (gThirtyFlags.mBossRoarTimer > 0)

        {

            aMul *= (1.0f + (float)TF_BOSS_ROAR_SPEED_PERCENT / 100.0f);

        }

        float aPrev = aZombie->mVelZ;
        if (aPrev <= 0.01f)
            aPrev = 1.0f;

        if (aZombie->mVelX > 0.0f)
        {
            aZombie->mVelX = (aZombie->mVelX / aPrev) * aMul;
        }
        aZombie->mVelZ = aMul;

        if (aTimeStopped)
        {
            aZombie->mVelX = 0.0f;
        }

        ThirtyFlagsZombieUpdate(aZombie);
    }

    // Í¶Ã¬ÇåÀí
    {
        Projectile* aProjectile = nullptr;
        while (theBoard->IterateProjectiles(aProjectile))
        {
            if (aProjectile->mProjectileType == PROJECTILE_CACTUS_SPEAR && aProjectile->mPosX < 20.0f)
            {
                aProjectile->mDead = true;
            }
        }
    }

    // Ê¬¶¾
    bool aHasPoison = false;
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            if (gThirtyFlagsPoison[x][y] > 0)
            {
                gThirtyFlagsPoison[x][y]--;
                aHasPoison = true;
            }
        }
    }

    if (aHasPoison && (theBoard->mMainCounter % 25) == 0)
    {
        // ¡¾ÈıÊ®Æì¡¤Æ½ºâ¡¿°´¸ñ½áËã£¬Ã¿¸ñÖ»´òÒ»´Î£»ÄÏ¹ÏÓÅÏÈ³ĞÉË
        for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
        {
            for (int aPoolRow = 0; aPoolRow < MAX_GRID_SIZE_Y; aPoolRow++)
            {
                if (gThirtyFlagsPoison[aCol][aPoolRow] <= 0)
                    continue;

                Plant* aVictim = TFDirectDamageTarget(theBoard, aCol, aPoolRow);
                if (!aVictim || aVictim->mDead)
                    continue;

                aVictim->mPlantHealth -= TF_POISON_TICK_DAMAGE;
                if (aVictim->mPlantHealth <= 0)
                {
                    theBoard->mPlantsEaten++;
                    aVictim->Die();
                    {
                        char aBuf[160];
                        sprintf(aBuf, "[TFPlant] poison killed plant %d at (%d,%d)",
                            (int)aVictim->mSeedType, aCol, aPoolRow);
                        TFLog(aBuf);
                    }
                }
            }
        }
    }
}
