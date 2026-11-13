/* UI languages. English strings in the code are the keys: tr() returns the
   Simplified Chinese text while Chinese is selected. text_align() in draw.c
   translates every string it draws, so plain literals need no tr(); format
   strings and names built with snprintf must be wrapped in tr() before
   formatting. The NVS "language" key stores the index: append languages only.
   tools/check_localization.py checks format tokens and font coverage;
   tools/pack_zh_font.py regenerates zh_font.h after the table changes. */
#include "ui.h"
#include "zh_font.h"

const char *const language_names[LANGUAGE_COUNT] = {"English", "简体中文"};
uint8_t language;

typedef struct {
    const char *en, *zh;
} Phrase;
static const Phrase zh_cn[] = {
        /* notices */
        {"Saving failed", "保存失败"},
        {"Bag full! Ore lost", "矿袋满了！矿石丢失"},
        {"+%lu gold", "+%lu 金币"},
        {"Gas! -%lu energy", "毒气！体力 -%lu"},
        {"Gas!", "毒气！"},
        {"Lava! Too hot to go there", "岩浆！太烫，过不去"},
        {"Lantern low! Rope up soon", "体力不足！尽快拉绳上井"},
        {"No dynamite: buy it at the shop", "没有炸药：去交易站购买"},
        {"%s bought", "已购买%s"},
        {"%s upgraded", "%s已升级"},
        {"Dynamite box is full", "炸药箱已满"},
        {"Already at the best level", "已达最高等级"},
        {"Finish the dive first", "请先结束本次下井"},
        {"Need %llu more gold", "还差 %llu 金币"},
        {"Elevator stop opened", "电梯站已开通"},
        {"Dig to %u m first", "请先挖到 %u 米"},
        {"Need %llu gold", "需要 %llu 金币"},
        {"Progress erased", "进度已清除"},
        {"Save is newer: please update", "存档版本较新：请更新固件"},
        {"Save unreadable", "存档无法读取"},
        {"Storage unavailable", "存储不可用"},
        /* tiles (core/digger.c) */
        {"Air", "空气"},
        {"Dirt", "泥土"},
        {"Clay", "黏土"},
        {"Stone", "石头"},
        {"Granite", "花岗岩"},
        {"Basalt", "玄武岩"},
        {"Obsidian", "黑曜石"},
        {"Bedrock", "基岩"},
        {"Coal", "煤"},
        {"Copper", "铜"},
        {"Silver", "银"},
        {"Gold", "金"},
        {"Ruby", "红宝石"},
        {"Diamond", "钻石"},
        {"Chest", "宝箱"},
        {"Gas pocket", "毒气穴"},
        {"Lava", "岩浆"},
        /* upgrades (core/digger.c) */
        {"Pickaxe", "镐"},
        {"Lantern", "矿灯"},
        {"Stamina", "体力"},
        {"Ore bag", "矿袋"},
        {"Lucky charm", "幸运符"},
        {"Dynamite", "炸药"},
        {"Fewer hits per tile", "每格所需敲击更少"},
        {"Wider light; reveals gas at level 3", "照明更广；3 级可看见毒气"},
        {"More energy per dive", "每次下井体力更多"},
        {"Carries more ore", "能装更多矿石"},
        {"+5% ore value per level", "每级矿石价值 +5%"},
        {"Blasts the 3x3 around you (pause menu)", "炸开身边 3x3 格（暂停菜单）"},
        /* surface */
        {"Gold Digger", "掘金矿工"},
        {"Trading Post", "交易站"},
        {"Mine Shaft", "矿井"},
        {"Elevator", "电梯"},
        {"Settings", "设置"},
        {"Resume your dive", "继续下井"},
        {"Dive from the surface", "从地面下井"},
        {"Best depth %u m  -  Dives %lu", "最深 %u 米  -  下井 %lu 次"},
        {"Upgrade pickaxe, lantern, bag", "升级镐、矿灯、矿袋"},
        {"Pickaxe Lv %u  Lantern Lv %u  Bag Lv %u", "镐 %u 级  矿灯 %u 级  矿袋 %u 级"},
        {"Start deeper: a stop every %u m", "从更深处出发：每 %u 米一站"},
        {"%u of %u stops open", "已开通 %u / %u 站"},
        {"Sound, language, sleep", "声音、语言、休眠"},
        {"Lifetime gold %s", "累计金币 %s"},
        {"Tap a place to go there", "点按地点前往"},
        {"UP/DOWN choose   OK enter", "上/下 选择   OK 进入"},
        /* mine */
        {"%u m", "%u 米"},
        {"Tap left / below / right      ", "点按左边 / 下方 / 右边      "},
        {"Menu", "菜单"},
        {"UP left  DOWN right  OK dig  hold OK menu", "上 左移  下 右移  OK 挖  长按 OK 菜单"},
        {"Paused", "已暂停"},
        {"Keep digging", "继续挖掘"},
        {"Light dynamite", "点燃炸药"},
        {"Rope up and sell", "拉绳上井并出售"},
        {"%s gold", "%s 金币"},
        {"Fainting loses half the bag; lava loses all of it.", "昏倒会失去一半矿石；掉进岩浆会全部失去。"},
        {"Tap an option", "点按选项"},
        {"UP/DOWN choose   OK confirm", "上/下 选择   OK 确认"},
        /* dive report */
        {"Dive report", "下井报告"},
        {"Back on the surface", "回到地面"},
        {"Hauled to the surface", "满载回到地面"},
        {"Fainted! Rescued", "昏倒了！已获救"},
        {"Lost in the lava!", "掉进了岩浆！"},
        {"Rock", "矿石"},
        {"Qty", "数量"},
        {"Price", "单价"},
        {"Total", "合计"},
        {"Treasure", "宝藏"},
        {"Empty bag this time.", "这次空手而归。"},
        {"Nothing made it out.", "什么也没带出来。"},
        {"Lost %u piece of ore", "丢失 %u 块矿石"},
        {"Lost %u pieces of ore", "丢失 %u 块矿石"},
        {"Earned", "收入"},
        {"Deepest %u m", "最深 %u 米"},
        {"NEW BEST!", "新纪录！"},
        {"Tap to continue", "点按继续"},
        {"OK continue", "OK 继续"},
        /* trading post */
        {"%s  Lv %u", "%s  %u 级"},
        {"Full", "已满"},
        {"Max", "满级"},
        {"Back", "返回"},
        {"Stone %u hits  ->  %u", "石头 %u 下  ->  %u"},
        {"Light radius %u  ->  %u", "照明半径 %u  ->  %u"},
        {"Energy %u  ->  %u", "体力 %u  ->  %u"},
        {"Holds %u  ->  %u pieces", "容量 %u  ->  %u 块"},
        {"Ore value +%u%%  ->  +%u%%", "矿石价值 +%u%%  ->  +%u%%"},
        {"Owned %u of %u", "拥有 %u / %u"},
        {"Return to the surface camp.", "返回地面营地。"},
        {"Tap to buy", "点按购买"},
        {"OK buy   hold OK back", "OK 购买   长按 OK 返回"},
        /* elevator */
        {"Stop  %u m", "%u 米站"},
        {"Surface  0 m", "地面  0 米"},
        {"Ride", "乘坐"},
        {"Locked", "未解锁"},
        {"Ride down and start digging at %u m.", "乘电梯下到 %u 米开始挖掘。"},
        {"Open this stop once; ride it on every later dive.", "开通一次，以后每次下井都能乘坐。"},
        {"Reach %u m in a dive to open this stop.", "在一次下井中到达 %u 米即可开通此站。"},
        {"Best depth %u m", "最深 %u 米"},
        {"Tap a stop", "点按站点"},
        {"OK ride / open   hold OK back", "OK 乘坐 / 开通   长按 OK 返回"},
        /* settings */
        {"Music", "音乐"},
        {"Sound effects", "音效"},
        {"Brightness", "亮度"},
        {"Sleep when idle", "闲置休眠"},
        {"Language", "语言"},
        {"Erase progress", "清除进度"},
        {"30 s", "30 秒"},
        {"1 min", "1 分钟"},
        {"2 min", "2 分钟"},
        {"5 min", "5 分钟"},
        {"Never", "从不"},
        {"Passport only", "仅 Passport"},
        {"Gold Digger %s", "掘金矿工 %s"},
        {"Progress saves automatically.", "进度自动保存。"},
        {"Saving is off for this session.", "本次运行不保存进度。"},
        {"Original game for FoloToy AI Passport.", "FoloToy AI Passport 原创游戏。"},
        {"Tap to change", "点按更改"},
        {"OK change   hold OK back", "OK 更改   长按 OK 返回"},
        {"Erase all gold, upgrades, elevator stops and records?", "清除所有金币、升级、电梯站和纪录？"},
        {"This cannot be undone.", "此操作无法撤销。"},
        {"Tap a language", "点按语言"},
        {"Keep my progress", "保留我的进度"},
        {"Erase everything", "全部清除"},
};

void set_language(unsigned index) { language = index < LANGUAGE_COUNT ? (uint8_t)index : 0; }
const char *tr(const char *en) {
    if (language != 1 || !en) return en;
    for (unsigned i = 0; i < sizeof(zh_cn) / sizeof(zh_cn[0]); ++i)
        if (!strcmp(zh_cn[i].en, en)) return zh_cn[i].zh;
    return en;
}
/* Montserrat has no CJK glyphs: any text with a CJK or full-width character
   is drawn in the Source Han Sans subset instead. LVGL symbols (U+F000 to
   U+F8FF) and plain ASCII keep the requested Montserrat size. */
const lv_font_t *font_for(const char *s, const lv_font_t *latin) {
    for (const unsigned char *p = (const unsigned char *)s; p && *p; ++p) {
        if (*p < 0xe0 || *p >= 0xf0 || !p[1] || !p[2]) continue;
        unsigned cp = (unsigned)(*p & 0x0f) << 12 | (unsigned)(p[1] & 0x3f) << 6 | (p[2] & 0x3f);
        if (cp >= 0x2e80 && (cp < 0xf000 || cp > 0xf8ff)) return &gd_zh_font;
        p += 2;
    }
    return latin;
}
