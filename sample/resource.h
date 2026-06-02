#pragma once

// Dialogs / menus
#define IDD_MAIN            101
#define IDM_MAIN            102

// Child controls (laid out in sample.rc)
#define IDC_GROUP           1001
#define IDC_BTN             1002
#define IDC_BTN_MSGBOX      1003
#define IDC_CHK1            1004
#define IDC_CHK2            1005
#define IDC_RAD1            1006
#define IDC_RAD2            1007
#define IDC_EDIT            1008
#define IDC_EDIT_MULTI      1009
#define IDC_COMBO           1010
#define IDC_LIST            1011
#define IDC_TREE            1012
#define IDC_PROGRESS        1013
#define IDC_TRACK           1014
#define IDC_INFO            1015
#define IDC_SYSLINK         1016

// Created programmatically
#define IDC_TOOLBAR         1100
#define IDC_STATUSBAR       1101

// Menu commands
#define IDM_FILE_EXIT       40001
#define IDM_MODE_SYSTEM     40002
#define IDM_MODE_DARK       40003
#define IDM_MODE_LIGHT      40004
#define IDM_HELP_ABOUT      40005

// Dark-mode color tones (must stay in umbra::ColorTone order: black=0 .. olive=6)
#define IDM_TONE_BLACK      40010
#define IDM_TONE_RED        40011
#define IDM_TONE_GREEN      40012
#define IDM_TONE_BLUE       40013
#define IDM_TONE_PURPLE     40014
#define IDM_TONE_CYAN       40015
#define IDM_TONE_OLIVE      40016
