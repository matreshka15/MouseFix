#include "localization.h"
#include <windows.h>
#include <stdbool.h>

/* Keep the two tables in the same order as LocId. */
static const wchar_t *const STRINGS_EN[LOC_COUNT] = {
    L"Total Blocked: %I32u events",
    L"Enable",
    L"Left (%dms)",
    L"Right (%dms)",
    L"Middle (%dms)",
    L"Side 1 (%dms)",
    L"Side 2 (%dms)",
    L"Wheel (%dms)",
    L"Custom...",
    L"Disable All",
    L"Enable All",
    L"Smart Drag Protection",
    L"Reset Statistics",
    L"Default (50ms)",
    L"Office Mode (60ms)",
    L"Strict Mode (40ms)",
    L"Presets",
    L"About",
    L"Exit",
    L"Set Threshold",
    L"Enter threshold for %s (%d-%dms):",
    L"OK",
    L"Cancel",
    L"MouseFix is already running",
    L"MouseFix is already running. Click the tray icon to configure it.",
    L"Could not install the mouse hook. Error code: %lu",
    L"Initialization failed. Error code: %lu",
    L"Threshold must be between %d and %d ms.",
    L"The mutex could not be created. Error code: %lu",
    L"The window class could not be registered. Error code: %lu",
    L"The window could not be created. Error code: %lu",
    L"An unexpected error has occurred. Error code: %lu",
    L"MouseFix - click to configure",
    L"MouseFix is running",
    L"All buttons are on. The icon is in the notification area, possibly under ^. Click it to switch buttons and set thresholds. It starts with Windows; turn that off in Settings > Apps > Startup.",
    L"Left",
    L"Right",
    L"Middle",
    L"Side 1",
    L"Side 2",
    L"Wheel",
    L"Filters double clicks, reversed wheel steps and dropped drags",
    L"Runs fully offline. No network access, no data collected.",
    L"Microsoft Store page",
    L"© 2026 Hank Li",
    L"Could not open the Microsoft Store page.",
    L"Trial: %d days left",
    L"Trial ended \u00b7 Buy full version",
    L"MouseFix trial ended",
    L"Your free trial has ended. Buy the full version to keep filtering mouse bounce.",
    L"Enjoying MouseFix?",
    L"Thanks for using MouseFix. Would you take a moment to rate it in the Store?",
    L"Rate now",
    L"Later",
};

static const wchar_t *const STRINGS_ZH[LOC_COUNT] = {
    L"已拦截 %I32u 次",
    L"启用",
    L"左键 (%dms)",
    L"右键 (%dms)",
    L"中键 (%dms)",
    L"侧键 1 (%dms)",
    L"侧键 2 (%dms)",
    L"滚轮 (%dms)",
    L"自定义…",
    L"全部停用",
    L"全部启用",
    L"拖拽保护",
    L"清零统计",
    L"默认 (50ms)",
    L"办公模式 (60ms)",
    L"严格模式 (40ms)",
    L"预设",
    L"关于",
    L"退出",
    L"设置阈值",
    L"输入%s的阈值（%d-%d 毫秒）：",
    L"确定",
    L"取消",
    L"MouseFix 已在运行",
    L"MouseFix 已经在运行，点一下托盘图标查看设置。",
    L"无法安装鼠标钩子，错误码：%lu",
    L"初始化失败，错误码：%lu",
    L"阈值需要在 %d 到 %d 毫秒之间。",
    L"无法创建互斥量，错误码：%lu",
    L"无法注册窗口类，错误码：%lu",
    L"无法创建窗口，错误码：%lu",
    L"发生意外错误，错误码：%lu",
    L"MouseFix — 点一下查看设置",
    L"MouseFix 已启动",
    L"已全部启用。图标在任务栏的通知区域，可能收进 ^ 里。点一下图标可以开关按键、调整阈值。它默认随 Windows 启动；不想自启，在 设置 > 应用 > 启动 里关掉。",
    L"左键",
    L"右键",
    L"中键",
    L"侧键 1",
    L"侧键 2",
    L"滚轮",
    L"过滤鼠标老化后的连击、滚轮倒跳和误拖拽",
    L"完全离线运行，不联网，不收集任何数据",
    L"Microsoft Store 页面",
    L"© 2026 Hank Li",
    L"无法打开 Microsoft Store 页面。",
    L"试用剩 %d 天",
    L"试用已结束 · 购买完整版",
    L"MouseFix 试用已结束",
    L"免费试用已结束，购买完整版可继续过滤鼠标误触。",
    L"喜欢 MouseFix 吗？",
    L"感谢使用 MouseFix，方便的话去商店给个评价？",
    L"去评价",
    L"以后再说",
};

/* Compile-time check that both tables cover every LocId. */
typedef char strings_en_count_check[(sizeof(STRINGS_EN) / sizeof(STRINGS_EN[0]) == LOC_COUNT) ? 1 : -1];
typedef char strings_zh_count_check[(sizeof(STRINGS_ZH) / sizeof(STRINGS_ZH[0]) == LOC_COUNT) ? 1 : -1];

/* -1 = not checked yet, 0 = English, 1 = Simplified Chinese. */
static int language = -1;

static bool use_chinese(void)
{
    if (language < 0)
    {
        LANGID lang = GetUserDefaultUILanguage();
        bool zh = PRIMARYLANGID(lang) == LANG_CHINESE &&
                  (SUBLANGID(lang) == SUBLANG_CHINESE_SIMPLIFIED ||
                   SUBLANGID(lang) == SUBLANG_CHINESE_SINGAPORE);
        language = zh ? 1 : 0;
    }
    return language == 1;
}

const wchar_t *loc_str(LocId id)
{
    if (id < 0 || id >= LOC_COUNT)
        id = LOC_TOTAL_BLOCKED;

    return use_chinese() ? STRINGS_ZH[id] : STRINGS_EN[id];
}

const wchar_t *loc_button_name(int button)
{
    switch (button)
    {
    case 0: return loc_str(LOC_BTN_LEFT);
    case 1: return loc_str(LOC_BTN_RIGHT);
    case 2: return loc_str(LOC_BTN_MIDDLE);
    case 3: return loc_str(LOC_BTN_X1);
    case 4: return loc_str(LOC_BTN_X2);
    case 5: return loc_str(LOC_BTN_WHEEL);
    default: return loc_str(LOC_BTN_LEFT);
    }
}
