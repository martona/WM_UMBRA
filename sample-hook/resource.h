#pragma once

// ---------------------------------------------------------------------------
// umbra-hook — resource identifiers
// A diagnostic harness that segregates Windows' color-determination paths onto
// tabs (A: raw user32/GDI, B: export-vs-internal, C: uxtheme/comctl32,
// E: DirectUI) so each can be judged independently once the Detours hooks land.
// ---------------------------------------------------------------------------

// Dialogs / menu
#define IDD_FRAME           101
#define IDM_MAIN            102
#define IDD_PAGE_A          103
#define IDD_PAGE_B          104
#define IDD_PAGE_C          105
#define IDD_PAGE_E          106

// Frame chrome (created programmatically)
#define IDC_TAB             1000
#define IDC_TOOLBAR         1001
#define IDC_STATUSBAR       1002

// Tab A — raw user32 / GDI controls
#define IDC_A_GROUP         1100
#define IDC_A_BTN           1101
#define IDC_A_CHK1          1102
#define IDC_A_CHK2          1103
#define IDC_A_RAD1          1104
#define IDC_A_RAD2          1105
#define IDC_A_EDIT          1106
#define IDC_A_EDIT_MULTI    1107
#define IDC_A_COMBO         1108
#define IDC_A_LIST          1109
#define IDC_A_SCROLL        1110

// Tab B — export-vs-internal exhibit
#define IDC_B_LBL_EXPORT    1200
#define IDC_B_LBL_INTERNAL  1201
#define IDC_B_PANEL_EXPORT  1202
#define IDC_B_PANEL_INTERNAL 1203

// Tab C — uxtheme / comctl32
#define IDC_C_LIST          1300
#define IDC_C_TREE          1301
#define IDC_C_PROGRESS      1302
#define IDC_C_TRACK         1303
#define IDC_C_SPIN          1304
#define IDC_C_SPINBUDDY     1305
#define IDC_C_DTP           1306
#define IDC_C_HOTKEY        1307
#define IDC_C_IPADDR        1308
#define IDC_C_SYSLINK       1309
#define IDC_C_CANARYGROUP   1310

// Tab E — DirectUI
#define IDC_E_BTN_FILEDLG   1400
#define IDC_E_BTN_COLORDLG  1401
#define IDC_E_HOSTLBL       1402
#define IDC_E_BTN_PERMS     1403

// Menu commands
#define IDM_FILE_EXIT       40001
#define IDM_MODE_SYSTEM     40002
#define IDM_MODE_DARK       40003
#define IDM_MODE_LIGHT      40004
#define IDM_HELP_ABOUT      40005
