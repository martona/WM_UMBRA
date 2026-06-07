// SPDX-License-Identifier: BSD-3-Clause license

/*
 * Copyright (c) 2025 Anthony Lee Stark. All rights reserved.
 * 
 * This project is based on and includes modified code from:
 * project 'win32-darkmode' by ysc3839 (MIT License),
 * available at: https://github.com/ysc3839/win32-darkmode
 * and project 'darkmodelib' by ozone10 (MPL-2.0 or MIT License),
 * available at: https://github.com/ozone10/darkmodelib
 *
 * The respective original licenses apply to portions of this code.
 * See the `licenses/` folder for more information.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *	  list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *	  this list of conditions and the following disclaimer in the documentation
 *	  and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Anthony Lee Stark (@anthonyleestark) nor the names of 
 *	  its contributors may be used to endorse or promote products derived from 
 *	  this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  * This file is based on the Notepad++ dark mode code, which is licensed under GPLv3.
  * Original source by Adam D. Walling (@adzm), with modifications by oZone10 and the Notepad++ team.
  * Further modified by Anthony Lee Stark (@anthonyleestark) in 2025.
  * Used with permission to relicense under the BSD-3-Clause license.
  */


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "umbra.h"
#include "SysColorHook.h"
#include "WinVerHelper.h"

#if !defined(_DARKMODE_NOT_USED)

#include <dwmapi.h>
#include <richedit.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <windowsx.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include "DarkMode.h"
#include "UAHMenuBar.h"

#include "Version.h"

#if defined(__GNUC__)
#include <cstdint>
//static constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static constexpr int CP_DROPDOWNITEM = 9; // for some reason mingw use only enum up to 8
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

//#ifndef WM_DPICHANGED_BEFOREPARENT
//#define WM_DPICHANGED_BEFOREPARENT 0x02E2
//#endif

#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

//#ifndef WM_GETDPISCALEDSIZE
//#define WM_GETDPISCALEDSIZE 0x02E4
//#endif

/// Converts 0xRRGGBB to COLORREF (0xBBGGRR) for GDI usage.
static constexpr COLORREF HEXRGB(DWORD rrggbb)
{
	return
		((rrggbb & 0xFF0000) >> 16) |
		((rrggbb & 0x00FF00)) |
		((rrggbb & 0x0000FF) << 16);
}

/**
 * @brief Retrieves the class name of a given window.
 *
 * This function wraps the Win32 API `GetClassNameW` to return the class name
 * of a window as a wide string (`std::wstring`).
 *
 * @param hWnd Handle to the target window.
 * @return The class name of the window as a `std::wstring`.
 *
 * @note The maximum length is capped at 32 characters (including the null terminator),
 *       which suffices for standard Windows window classes.
 */
static std::wstring GetWndClassName(HWND hWnd)
{
	static constexpr int strLen = 32;
	std::wstring className(strLen, L'\0');
	className.resize(static_cast<size_t>(::GetClassNameW(hWnd, className.data(), strLen)));
	return className;
}

/**
 * @brief Compares the class name of a window with a specified string.
 *
 * This function retrieves the class name of the given window handle
 * and compares it to the provided class name.
 *
 * @param hWnd Handle to the window whose class name is to be checked.
 * @param classNameToCmp Pointer to a null-terminated wide string representing the class name to compare against.
 * @return `true` if the window's class name matches the specified string; otherwise `false`.
 *
 * @see GetWndClassName()
 */
static bool CmpWndClassName(HWND hWnd, const wchar_t* classNameToCmp)
{
	return (GetWndClassName(hWnd) == classNameToCmp);
}


namespace umbra
{
	/**
	 * @brief Returns library version information or compile-time feature flags.
	 *
	 * Responds to the specified query by returning either:
	 * - Version numbers (`verMajor`, `verMinor`, `verPatch`, `verRevision`)
	 * - Build configuration flags (returns `TRUE` or `FALSE`)
	 * - A constant value (`featureCheck`, `maxValue`) used for validation
	 *
	 * @param libInfoType Enum value specifying which piece of information to retrieve.
	 * @return Integer value:
	 * - Version: as defined by `DM_VERSION_MAJOR`, etc.
	 * - Boolean flags: `TRUE` (1) if the feature is enabled, `FALSE` (0) otherwise.
	 * - `featureCheck`, `maxValue`: returns the numeric max enum value.
	 * - `-1`: for invalid or unhandled enum cases (should not occur in correct usage).
	 *
	 * @see LibInfo
	 */
	int getLibInfo(LibInfo libInfoType) noexcept
	{
		switch (libInfoType)
		{
			case LibInfo::maxValue:
			case LibInfo::featureCheck:
			{
				return static_cast<int>(LibInfo::maxValue);
			}

			case LibInfo::verMajor:
			{
				return DM_VERSION_MAJOR;
			}

			case LibInfo::verMinor:
			{
				return DM_VERSION_MINOR;
			}

			case LibInfo::verPatch:
			{
				return DM_VERSION_PATCH;
			}

			case LibInfo::verRevision:
			{
				return DM_VERSION_REVISION;
			}

			case LibInfo::iathookExternal:
			{
#if defined(_DARKMODE_EXTERNAL_IATHOOK)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::allowOldOS:
			{
#if defined(_DARKMODE_SUPPORT_OLDER_OS)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::useDlgProcCtl:
			{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::preferTheme:
			{
#if defined(_DARKMODE_PREFER_THEME)
				return TRUE;
#else
				return FALSE;
#endif
			}
		}

		return -1; // should never happen
	}

	/**
	 * @brief Describes how the application responds to the system theme.
	 *
	 * Used to determine behavior when following the system's light/dark mode setting.
	 * - `disabled`: Do not follow system; use manually selected appearance.
	 * - `light`: Follow system mode; apply light theme when system is in light mode.
	 * - `classic`: Follow system mode; apply classic style when system is in light mode.
	 */
	enum class WinMode : std::uint8_t
	{
		disabled,  ///< Manual — system mode is ignored.
		light,     ///< Use light theme if system is in light mode.
		classic    ///< Use classic style if system is in light mode.
	};

	static constexpr UINT_PTR kButtonSubclassID                 = 42;
	static constexpr UINT_PTR kGroupboxSubclassID               = 1;
	static constexpr UINT_PTR kUpDownSubclassID                 = 2;
	static constexpr UINT_PTR kTabPaintSubclassID               = 3;
	static constexpr UINT_PTR kTabUpDownSubclassID              = 4;
	static constexpr UINT_PTR kCustomBorderSubclassID           = 5;
	static constexpr UINT_PTR kComboBoxSubclassID               = 6;
	static constexpr UINT_PTR kComboBoxExSubclassID             = 7;
	static constexpr UINT_PTR kListViewSubclassID               = 8;
	static constexpr UINT_PTR kHeaderSubclassID                 = 9;
	static constexpr UINT_PTR kStatusBarSubclassID              = 10;
	static constexpr UINT_PTR kProgressBarSubclassID            = 11;
	static constexpr UINT_PTR kStaticTextSubclassID             = 12;
	static constexpr UINT_PTR kWindowEraseBgSubclassID          = 13;
	static constexpr UINT_PTR kWindowCtlColorSubclassID         = 14;
	static constexpr UINT_PTR kWindowNotifySubclassID           = 15;
	static constexpr UINT_PTR kWindowMenuBarSubclassID          = 16;
	static constexpr UINT_PTR kWindowSettingChangeSubclassID    = 17;
	static constexpr UINT_PTR kAcluiCheckListSubclassID         = 18;

	/**
	 * @struct DarkModeParams
	 * @brief Defines theming and subclassing parameters for child controls.
	 *
	 * Members:
	 * - `_themeClassName`: Optional theme class name (e.g. `"DarkMode_Explorer"`), or `nullptr` to skip theming.
	 * - `_subclass`: Whether to apply custom subclassing for dark-mode painting and behavior.
	 * - `_theme`: Whether to apply a themed visual style to applicable controls.
	 *
	 * Used during enumeration to configure dark mode application on a per-control basis.
	 */
	struct DarkModeParams
	{
		const wchar_t* _themeClassName = nullptr;
		bool _subclass = false;
		bool _theme = false;
	};

	/// Base roundness value for various controls, such as toolbar iconic buttons and combo boxes
	static constexpr int kWin11CornerRoundness = 4;

	/// Threshold range around 50.0 where TreeView uses classic style instead of light/dark.
	static constexpr double kMiddleGrayRange = 2.0;

	namespace // anonymous
	{
		/// Global struct
		struct
		{
			DWM_WINDOW_CORNER_PREFERENCE _roundCorner = DWMWCP_DEFAULT;
			COLORREF _borderColor = DWMWA_COLOR_DEFAULT;
			DWM_SYSTEMBACKDROP_TYPE _mica = DWMSBT_AUTO;
			COLORREF _tvBackground = RGB(41, 49, 52);
			double _lightness = 50.0;
			TreeViewStyle _tvStylePrev = TreeViewStyle::classic;
			TreeViewStyle _tvStyle = TreeViewStyle::classic;
			bool _micaExtend = false;
			bool _colorizeTitleBar = false;
			DarkModeType _dmType = DarkModeType::dark;
			WinMode _windowsMode = WinMode::disabled;
			bool _isInit = false;
			bool _isInitExperimental = false;
		} g_dmCfg;
	}; // anonymous namespace

	struct Brushes
	{
		HBRUSH _background = nullptr;
		HBRUSH _ctrlBackground = nullptr;
		HBRUSH _hotBackground = nullptr;
		HBRUSH _dlgBackground = nullptr;
		HBRUSH _errorBackground = nullptr;

		HBRUSH _edge = nullptr;
		HBRUSH _hotEdge = nullptr;
		HBRUSH _disabledEdge = nullptr;

		Brushes() = delete;

		explicit Brushes(const Colors& colors) noexcept
			: _background(::CreateSolidBrush(colors.background))
			, _ctrlBackground(::CreateSolidBrush(colors.ctrlBackground))
			, _hotBackground(::CreateSolidBrush(colors.hotBackground))
			, _dlgBackground(::CreateSolidBrush(colors.dlgBackground))
			, _errorBackground(::CreateSolidBrush(colors.errorBackground))

			, _edge(::CreateSolidBrush(colors.edge))
			, _hotEdge(::CreateSolidBrush(colors.hotEdge))
			, _disabledEdge(::CreateSolidBrush(colors.disabledEdge))
		{}

		Brushes(const Brushes&) = delete;
		Brushes& operator=(const Brushes&) = delete;

		Brushes(Brushes&&) = delete;
		Brushes& operator=(Brushes&&) = delete;

		~Brushes()
		{
			::DeleteObject(_background);         _background = nullptr;
			::DeleteObject(_ctrlBackground);     _ctrlBackground = nullptr;
			::DeleteObject(_hotBackground);      _hotBackground = nullptr;
			::DeleteObject(_dlgBackground);      _dlgBackground = nullptr;
			::DeleteObject(_errorBackground);    _errorBackground = nullptr;

			::DeleteObject(_edge);               _edge = nullptr;
			::DeleteObject(_hotEdge);            _hotEdge = nullptr;
			::DeleteObject(_disabledEdge);       _disabledEdge = nullptr;
		}

		void updateBrushes(const Colors& colors) noexcept
		{
			::DeleteObject(_background);
			::DeleteObject(_ctrlBackground);
			::DeleteObject(_hotBackground);
			::DeleteObject(_dlgBackground);
			::DeleteObject(_errorBackground);

			::DeleteObject(_edge);
			::DeleteObject(_hotEdge);
			::DeleteObject(_disabledEdge);

			_background = ::CreateSolidBrush(colors.background);
			_ctrlBackground = ::CreateSolidBrush(colors.ctrlBackground);
			_hotBackground = ::CreateSolidBrush(colors.hotBackground);
			_dlgBackground = ::CreateSolidBrush(colors.dlgBackground);
			_errorBackground = ::CreateSolidBrush(colors.errorBackground);

			_edge = ::CreateSolidBrush(colors.edge);
			_hotEdge = ::CreateSolidBrush(colors.hotEdge);
			_disabledEdge = ::CreateSolidBrush(colors.disabledEdge);
		}
	};

	struct Pens
	{
		HPEN _darkerText = nullptr;
		HPEN _edge = nullptr;
		HPEN _hotEdge = nullptr;
		HPEN _disabledEdge = nullptr;

		Pens() = delete;

		explicit Pens(const Colors& colors) noexcept
			: _darkerText(::CreatePen(PS_SOLID, 1, colors.darkerText))
			, _edge(::CreatePen(PS_SOLID, 1, colors.edge))
			, _hotEdge(::CreatePen(PS_SOLID, 1, colors.hotEdge))
			, _disabledEdge(::CreatePen(PS_SOLID, 1, colors.disabledEdge))
		{}

		Pens(const Pens&) = delete;
		Pens& operator=(const Pens&) = delete;

		Pens(Pens&&) = delete;
		Pens& operator=(Pens&&) = delete;

		~Pens()
		{
			::DeleteObject(_darkerText);    _darkerText = nullptr;
			::DeleteObject(_edge);          _edge = nullptr;
			::DeleteObject(_hotEdge);       _hotEdge = nullptr;
			::DeleteObject(_disabledEdge);  _disabledEdge = nullptr;
		}

		void updatePens(const Colors& colors) noexcept
		{
			::DeleteObject(_darkerText);
			::DeleteObject(_edge);
			::DeleteObject(_hotEdge);
			::DeleteObject(_disabledEdge);

			_darkerText = ::CreatePen(PS_SOLID, 1, colors.darkerText);
			_edge = ::CreatePen(PS_SOLID, 1, colors.edge);
			_hotEdge = ::CreatePen(PS_SOLID, 1, colors.hotEdge);
			_disabledEdge = ::CreatePen(PS_SOLID, 1, colors.disabledEdge);
		}
	};

	/// Black tone (default)
	static constexpr Colors darkColors{
		HEXRGB(0x202020),   // background
		HEXRGB(0x383838),   // ctrlBackground
		HEXRGB(0x454545),   // hotBackground
		HEXRGB(0x202020),   // dlgBackground
		HEXRGB(0xB00000),   // errorBackground
		HEXRGB(0xE0E0E0),   // textColor
		HEXRGB(0xC0C0C0),   // darkerTextColor
		HEXRGB(0x808080),   // disabledTextColor
		HEXRGB(0xFFFF00),   // linkTextColor
		HEXRGB(0x646464),   // edgeColor
		HEXRGB(0x9B9B9B),   // hotEdgeColor
		HEXRGB(0x484848)    // disabledEdgeColor
	};

	static constexpr DWORD offsetEdge = HEXRGB(0x1C1C1C);

	/// Red tone
	static constexpr DWORD offsetRed = HEXRGB(0x100000);
	static constexpr Colors darkRedColors{
		darkColors.background + offsetRed,
		darkColors.ctrlBackground + offsetRed,
		darkColors.hotBackground + offsetRed,
		darkColors.dlgBackground + offsetRed,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetRed,
		darkColors.hotEdge + offsetRed,
		darkColors.disabledEdge + offsetRed
	};

	/// Green tone
	static constexpr DWORD offsetGreen = HEXRGB(0x001000);
	static constexpr Colors darkGreenColors{
		darkColors.background + offsetGreen,
		darkColors.ctrlBackground + offsetGreen,
		darkColors.hotBackground + offsetGreen,
		darkColors.dlgBackground + offsetGreen,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetGreen,
		darkColors.hotEdge + offsetGreen,
		darkColors.disabledEdge + offsetGreen
	};

	/// Blue tone
	static constexpr DWORD offsetBlue = HEXRGB(0x000020);
	static constexpr Colors darkBlueColors{
		darkColors.background + offsetBlue,
		darkColors.ctrlBackground + offsetBlue,
		darkColors.hotBackground + offsetBlue,
		darkColors.dlgBackground + offsetBlue,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetBlue,
		darkColors.hotEdge + offsetBlue,
		darkColors.disabledEdge + offsetBlue
	};

	/// Purple tone
	static constexpr DWORD offsetPurple = HEXRGB(0x100020);
	static constexpr Colors darkPurpleColors{
		darkColors.background + offsetPurple,
		darkColors.ctrlBackground + offsetPurple,
		darkColors.hotBackground + offsetPurple,
		darkColors.dlgBackground + offsetPurple,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetPurple,
		darkColors.hotEdge + offsetPurple,
		darkColors.disabledEdge + offsetPurple
	};

	/// Cyan tone
	static constexpr DWORD offsetCyan = HEXRGB(0x001020);
	static constexpr Colors darkCyanColors{
		darkColors.background + offsetCyan,
		darkColors.ctrlBackground + offsetCyan,
		darkColors.hotBackground + offsetCyan,
		darkColors.dlgBackground + offsetCyan,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetCyan,
		darkColors.hotEdge + offsetCyan,
		darkColors.disabledEdge + offsetCyan
	};

	/// Olive tone
	static constexpr DWORD offsetOlive = HEXRGB(0x101000);
	static constexpr Colors darkOliveColors{
		darkColors.background + offsetOlive,
		darkColors.ctrlBackground + offsetOlive,
		darkColors.hotBackground + offsetOlive,
		darkColors.dlgBackground + offsetOlive,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetOlive,
		darkColors.hotEdge + offsetOlive,
		darkColors.disabledEdge + offsetOlive
	};

	/// Light tone
	static Colors getLightColors()
	{
		const Colors lightColors{
		::GetSysColor(COLOR_3DFACE),        // background
		::GetSysColor(COLOR_WINDOW),        // ctrlBackground
		HEXRGB(0xC0DCF3),                   // hotBackground
		::GetSysColor(COLOR_3DFACE),        // dlgBackground
		HEXRGB(0xA01000),                   // errorBackground
		::GetSysColor(COLOR_WINDOWTEXT),    // textColor
		::GetSysColor(COLOR_BTNTEXT),       // darkerTextColor
		::GetSysColor(COLOR_GRAYTEXT),      // disabledTextColor
		::GetSysColor(COLOR_HOTLIGHT),      // linkTextColor
		HEXRGB(0x8D8D8D),                   // edgeColor
		::GetSysColor(COLOR_HIGHLIGHT),     // hotEdgeColor
		::GetSysColor(COLOR_GRAYTEXT)       // disabledEdgeColor
		};

		return lightColors;
	}

	class Theme
	{
	public:
		Theme() noexcept
			: _colors(darkColors)
			, _brushes(darkColors)
			, _pens(darkColors)
		{}

		explicit Theme(const Colors& colors) noexcept
			: _colors(colors)
			, _brushes(colors)
			, _pens(colors)
		{}

		void updateTheme() noexcept
		{
			_brushes.updateBrushes(_colors);
			_pens.updatePens(_colors);
		}

		void updateTheme(Colors colors) noexcept
		{
			_colors = colors;
			Theme::updateTheme();
		}

		[[nodiscard]] Colors getToneColors() const noexcept
		{
			switch (_tone)
			{
				case ColorTone::red:
				{
					return darkRedColors;
				}

				case ColorTone::green:
				{
					return darkGreenColors;
				}

				case ColorTone::blue:
				{
					return darkBlueColors;
				}

				case ColorTone::purple:
				{
					return darkPurpleColors;
				}

				case ColorTone::cyan:
				{
					return darkCyanColors;
				}

				case ColorTone::olive:
				{
					return darkOliveColors;
				}

				case ColorTone::black:
				case ColorTone::max:
				{
					break;
				}
			}
			return darkColors;
		}

		void setToneColors(ColorTone colorTone) noexcept
		{
			_tone = colorTone;

			switch (_tone)
			{
				case ColorTone::red:
				{
					_colors = darkRedColors;
					break;
				}

				case ColorTone::green:
				{
					_colors = darkGreenColors;
					break;
				}

				case ColorTone::blue:
				{
					_colors = darkBlueColors;
					break;
				}

				case ColorTone::purple:
				{
					_colors = darkPurpleColors;
					break;
				}

				case ColorTone::cyan:
				{
					_colors = darkCyanColors;
					break;
				}

				case ColorTone::olive:
				{
					_colors = darkOliveColors;
					break;
				}

				case ColorTone::black:
				case ColorTone::max:
				{
					_colors = darkColors;
					break;
				}
			}

			Theme::updateTheme();
		}

		void setToneColors() noexcept
		{
			_colors = Theme::getToneColors();
			Theme::updateTheme();
		}

		[[nodiscard]] const Brushes& getBrushes() const noexcept
		{
			return _brushes;
		}

		[[nodiscard]] const Pens& getPens() const noexcept
		{
			return _pens;
		}

		[[nodiscard]] const ColorTone& getColorTone() const noexcept
		{
			return _tone;
		}

		Colors _colors;

	private:
		Brushes _brushes;
		Pens _pens;
		ColorTone _tone = umbra::ColorTone::black;
	};

	static Theme& getTheme() noexcept
	{
		static Theme tMain{};
		return tMain;
	}

	/**
	 * @brief Sets the color tone and its color set for the active theme.
	 *
	 * Applies a color tone (e.g. red, blue, olive) its color set.
	 *
	 * @param colorTone The tone to apply (see @ref ColorTone enum).
	 *
	 * @see umbra::getColorTone()
	 * @see umbra::Theme
	 */
	void setColorTone(ColorTone colorTone) noexcept
	{
		umbra::getTheme().setToneColors(colorTone);
	}

	/**
	 * @brief Retrieves the currently active color tone for the theme.
	 *
	 * @return The currently selected @ref ColorTone value.
	 *
	 * @see umbra::setColorTone()
	 */
	ColorTone getColorTone() noexcept
	{
		return umbra::getTheme().getColorTone();
	}

	/// Dark views colors
	static constexpr ColorsView darkColorsView{
		HEXRGB(0x293134),   // background
		HEXRGB(0xE0E2E4),   // text
		HEXRGB(0x646464),   // gridlines
		HEXRGB(0x202020),   // Header background
		HEXRGB(0x454545),   // Header hot background
		HEXRGB(0xC0C0C0),   // header text
		HEXRGB(0x646464)    // header divider
	};

	/// Light views colors
	static constexpr ColorsView lightColorsView{
		HEXRGB(0xFFFFFF),   // background
		HEXRGB(0x000000),   // text
		HEXRGB(0xF0F0F0),   // gridlines
		HEXRGB(0xFFFFFF),   // header background
		HEXRGB(0xD9EBF9),   // header hot background
		HEXRGB(0x000000),   // header text
		HEXRGB(0xE5E5E5)    // header divider
	};

	struct BrushesAndPensView
	{
		HBRUSH _background = nullptr;
		HBRUSH _gridlines = nullptr;
		HBRUSH _headerBackground = nullptr;
		HBRUSH _headerHotBackground = nullptr;

		HPEN _headerEdge = nullptr;

		BrushesAndPensView() = delete;

		explicit BrushesAndPensView(const ColorsView& colors) noexcept
			: _background(::CreateSolidBrush(colors.background))
			, _gridlines(::CreateSolidBrush(colors.gridlines))
			, _headerBackground(::CreateSolidBrush(colors.headerBackground))
			, _headerHotBackground(::CreateSolidBrush(colors.headerHotBackground))

			, _headerEdge(::CreatePen(PS_SOLID, 1, colors.headerEdge))
		{}

		BrushesAndPensView(const BrushesAndPensView&) = delete;
		BrushesAndPensView& operator=(const BrushesAndPensView&) = delete;

		BrushesAndPensView(BrushesAndPensView&&) = delete;
		BrushesAndPensView& operator=(BrushesAndPensView&&) = delete;

		~BrushesAndPensView()
		{
			::DeleteObject(_background);             _background = nullptr;
			::DeleteObject(_gridlines);              _gridlines = nullptr;
			::DeleteObject(_headerBackground);       _headerBackground = nullptr;
			::DeleteObject(_headerHotBackground);    _headerHotBackground = nullptr;

			::DeleteObject(_headerEdge);             _headerEdge = nullptr;
		}

		void update(const ColorsView& colors)
		{
			::DeleteObject(_background);
			::DeleteObject(_gridlines);
			::DeleteObject(_headerBackground);
			::DeleteObject(_headerHotBackground);

			_background = ::CreateSolidBrush(colors.background);
			_gridlines = ::CreateSolidBrush(colors.gridlines);
			_headerBackground = ::CreateSolidBrush(colors.headerBackground);
			_headerHotBackground = ::CreateSolidBrush(colors.headerHotBackground);

			::DeleteObject(_headerEdge);

			_headerEdge = ::CreatePen(PS_SOLID, 1, colors.headerEdge);
		}
	};

	class ThemeView
	{
	public:
		ThemeView() noexcept
			: _clrView(darkColorsView)
			, _hbrPnView(darkColorsView)
		{}

		explicit ThemeView(const ColorsView& colorsView) noexcept
			: _clrView(colorsView)
			, _hbrPnView(colorsView)
		{}

		void updateView()
		{
			_hbrPnView.update(_clrView);
		}

		void updateView(ColorsView colors)
		{
			_clrView = colors;
			ThemeView::updateView();
		}

		[[nodiscard]] const BrushesAndPensView& getViewBrushesAndPens() const noexcept
		{
			return _hbrPnView;
		}

		ColorsView _clrView;

	private:
		BrushesAndPensView _hbrPnView;
	};

	static ThemeView& getThemeView() noexcept
	{
		static ThemeView tView{};
		return tView;
	}

	static COLORREF setNewColor(COLORREF* clrOld, COLORREF clrNew)
	{
		const auto clrTmp = *clrOld;
		*clrOld = clrNew;
		return clrTmp;
	}

	COLORREF setBackgroundColor(COLORREF clrNew)        { return umbra::setNewColor(&umbra::getTheme()._colors.background, clrNew); }
	COLORREF setCtrlBackgroundColor(COLORREF clrNew)    { return umbra::setNewColor(&umbra::getTheme()._colors.ctrlBackground, clrNew); }
	COLORREF setHotBackgroundColor(COLORREF clrNew)     { return umbra::setNewColor(&umbra::getTheme()._colors.hotBackground, clrNew); }
	COLORREF setDlgBackgroundColor(COLORREF clrNew)     { return umbra::setNewColor(&umbra::getTheme()._colors.dlgBackground, clrNew); }
	COLORREF setErrorBackgroundColor(COLORREF clrNew)   { return umbra::setNewColor(&umbra::getTheme()._colors.errorBackground, clrNew); }
	COLORREF setTextColor(COLORREF clrNew)              { return umbra::setNewColor(&umbra::getTheme()._colors.text, clrNew); }
	COLORREF setDarkerTextColor(COLORREF clrNew)        { return umbra::setNewColor(&umbra::getTheme()._colors.darkerText, clrNew); }
	COLORREF setDisabledTextColor(COLORREF clrNew)      { return umbra::setNewColor(&umbra::getTheme()._colors.disabledText, clrNew); }
	COLORREF setLinkTextColor(COLORREF clrNew)          { return umbra::setNewColor(&umbra::getTheme()._colors.linkText, clrNew); }
	COLORREF setEdgeColor(COLORREF clrNew)              { return umbra::setNewColor(&umbra::getTheme()._colors.edge, clrNew); }
	COLORREF setHotEdgeColor(COLORREF clrNew)           { return umbra::setNewColor(&umbra::getTheme()._colors.hotEdge, clrNew); }
	COLORREF setDisabledEdgeColor(COLORREF clrNew)      { return umbra::setNewColor(&umbra::getTheme()._colors.disabledEdge, clrNew); }

	void setThemeColors(Colors colors) noexcept
	{
		umbra::getTheme().updateTheme(colors);
	}

	void updateThemeBrushesAndPens() noexcept
	{
		umbra::getTheme().updateTheme();
	}

	COLORREF getBackgroundColor() noexcept		{ return getTheme()._colors.background; }
	COLORREF getCtrlBackgroundColor() noexcept	{ return getTheme()._colors.ctrlBackground; }
	COLORREF getHotBackgroundColor() noexcept	{ return getTheme()._colors.hotBackground; }
	COLORREF getDlgBackgroundColor() noexcept	{ return getTheme()._colors.dlgBackground; }
	COLORREF getErrorBackgroundColor() noexcept	{ return getTheme()._colors.errorBackground; }
	COLORREF getTextColor() noexcept			{ return getTheme()._colors.text; }
	COLORREF getDarkerTextColor() noexcept		{ return getTheme()._colors.darkerText; }
	COLORREF getDisabledTextColor() noexcept	{ return getTheme()._colors.disabledText; }
	COLORREF getLinkTextColor() noexcept		{ return getTheme()._colors.linkText; }
	COLORREF getEdgeColor() noexcept			{ return getTheme()._colors.edge; }
	COLORREF getHotEdgeColor() noexcept			{ return getTheme()._colors.hotEdge; }
	COLORREF getDisabledEdgeColor() noexcept	{ return getTheme()._colors.disabledEdge; }

	HBRUSH getBackgroundBrush() noexcept		{ return getTheme().getBrushes()._background; }
	HBRUSH getCtrlBackgroundBrush() noexcept	{ return getTheme().getBrushes()._ctrlBackground; }
	HBRUSH getHotBackgroundBrush() noexcept		{ return getTheme().getBrushes()._hotBackground; }
	HBRUSH getDlgBackgroundBrush() noexcept		{ return getTheme().getBrushes()._dlgBackground; }
	HBRUSH getErrorBackgroundBrush() noexcept	{ return getTheme().getBrushes()._errorBackground; }

	HBRUSH getEdgeBrush() noexcept				{ return getTheme().getBrushes()._edge; }
	HBRUSH getHotEdgeBrush() noexcept			{ return getTheme().getBrushes()._hotEdge; }
	HBRUSH getDisabledEdgeBrush() noexcept		{ return getTheme().getBrushes()._disabledEdge; }

	HPEN getDarkerTextPen() noexcept			{ return getTheme().getPens()._darkerText; }
	HPEN getEdgePen() noexcept					{ return getTheme().getPens()._edge; }
	HPEN getHotEdgePen() noexcept				{ return getTheme().getPens()._hotEdge; }
	HPEN getDisabledEdgePen() noexcept			{ return getTheme().getPens()._disabledEdge; }

	COLORREF setViewBackgroundColor(COLORREF clrNew)	{ return umbra::setNewColor(&umbra::getThemeView()._clrView.background, clrNew); }
	COLORREF setViewTextColor(COLORREF clrNew)			{ return umbra::setNewColor(&umbra::getThemeView()._clrView.text, clrNew); }
	COLORREF setViewGridlinesColor(COLORREF clrNew)		{ return umbra::setNewColor(&umbra::getThemeView()._clrView.gridlines, clrNew); }

	COLORREF setHeaderBackgroundColor(COLORREF clrNew)		{ return umbra::setNewColor(&umbra::getThemeView()._clrView.headerBackground, clrNew); }
	COLORREF setHeaderHotBackgroundColor(COLORREF clrNew) 	{ return umbra::setNewColor(&umbra::getThemeView()._clrView.headerHotBackground, clrNew); }
	COLORREF setHeaderTextColor(COLORREF clrNew) 			{ return umbra::setNewColor(&umbra::getThemeView()._clrView.headerText, clrNew); }
	COLORREF setHeaderEdgeColor(COLORREF clrNew) 			{ return umbra::setNewColor(&umbra::getThemeView()._clrView.headerEdge, clrNew); }

	void setViewColors(ColorsView colors)
	{
		umbra::getThemeView().updateView(colors);
	}

	void updateViewBrushesAndPens()
	{
		umbra::getThemeView().updateView();
	}

	COLORREF getViewBackgroundColor() noexcept		{ return umbra::getThemeView()._clrView.background; }
	COLORREF getViewTextColor() noexcept			{ return umbra::getThemeView()._clrView.text; }
	COLORREF getViewGridlinesColor() noexcept		{ return umbra::getThemeView()._clrView.gridlines; }

	COLORREF getHeaderBackgroundColor() noexcept	{ return umbra::getThemeView()._clrView.headerBackground; }
	COLORREF getHeaderHotBackgroundColor() noexcept { return umbra::getThemeView()._clrView.headerHotBackground; }
	COLORREF getHeaderTextColor() noexcept			{ return umbra::getThemeView()._clrView.headerText; }
	COLORREF getHeaderEdgeColor() noexcept			{ return umbra::getThemeView()._clrView.headerEdge; }

	HBRUSH getViewBackgroundBrush() noexcept		{ return umbra::getThemeView().getViewBrushesAndPens()._background; }
	HBRUSH getViewGridlinesBrush() noexcept			{ return umbra::getThemeView().getViewBrushesAndPens()._gridlines; }

	HBRUSH getHeaderBackgroundBrush() noexcept		{ return umbra::getThemeView().getViewBrushesAndPens()._headerBackground; }
	HBRUSH getHeaderHotBackgroundBrush() noexcept	{ return umbra::getThemeView().getViewBrushesAndPens()._headerHotBackground; }

	HPEN getHeaderEdgePen() noexcept				{ return umbra::getThemeView().getViewBrushesAndPens()._headerEdge; }

	/**
	 * @brief Initializes default color set based on the current mode type.
	 *
	 * Sets up control and view colors depending on the active theme:
	 * - `dark`: Applies dark tone color set and view dark color set.
	 * - `light`: Applies the predefined light color set and view light color set.
	 * - `classic`: Applies only system color on views, other controls are not affected
	 *              by theme colors.
	 *
	 * If `updateBrushesAndOther` is `true`, also updates
	 * brushes, pens, and view styles (unless in classic mode).
	 *
	 * @param updateBrushesAndOther Whether to refresh GDI brushes and pens, and tree view styling.
	 *
	 * @see umbra::setToneColors
	 * @see umbra::updateThemeBrushesAndPens
	 * @see umbra::calculateTreeViewStyle
	 */
	void setDefaultColors(bool updateBrushesAndOther)
	{
		switch (g_dmCfg._dmType)
		{
			case DarkModeType::dark:
			{
				umbra::getTheme().setToneColors();
				umbra::getThemeView()._clrView = umbra::darkColorsView;
				break;
			}

			case DarkModeType::light:
			{
				umbra::getTheme()._colors = umbra::getLightColors();
				umbra::getThemeView()._clrView = umbra::lightColorsView;
				break;
			}

			case DarkModeType::classic:
			{
				umbra::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
				umbra::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
				break;
			}
		}

		if (updateBrushesAndOther)
		{
			if (g_dmCfg._dmType != DarkModeType::classic)
			{
				umbra::updateThemeBrushesAndPens();
				umbra::updateViewBrushesAndPens();
			}

			umbra::calculateTreeViewStyle();
		}
	}

	/**
	 * @brief Initializes the dark mode configuration based on the selected mode.
	 *
	 * Sets the active dark mode rendering and system-following behavior according to the specified `dmType`:
	 * - `0`: Light mode, do not follow system.
	 * - `1` or default: Dark mode, do not follow system.
	 * - `2`: *[Internal]* Follow system — light or dark depending on registry (see `umbra::isDarkModeReg()`).
	 * - `3`: Classic mode, do not follow system.
	 * - `4`: *[Internal]* Follow system — classic or dark depending on registry.
	 *
	 * @param dmType Integer representing the desired mode.
	 *
	 * @see DarkModeType
	 * @see WinMode
	 * @see umbra::isDarkModeReg()
	 */
	void initDarkModeConfig(UINT dmType)
	{
		switch (dmType)
		{
			case 0:
			{
				g_dmCfg._dmType = DarkModeType::light;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}

			case 2:
			{
				g_dmCfg._dmType = umbra::isDarkModeReg() ? DarkModeType::dark : DarkModeType::light;
				g_dmCfg._windowsMode = WinMode::light;
				break;
			}

			case 3:
			{
				g_dmCfg._dmType = DarkModeType::classic;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}

			case 4:
			{
				g_dmCfg._dmType = umbra::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic;
				g_dmCfg._windowsMode = WinMode::classic;
				break;
			}

			case 1:
			default:
			{
				g_dmCfg._dmType = DarkModeType::dark;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}
		}
	}

	/**
	 * @brief Sets the preferred window corner style on Windows 11.
	 *
	 * Assigns a valid `DWM_WINDOW_CORNER_PREFERENCE` value to the config,
	 * falling back to `DWMWCP_DEFAULT` if the input is out of range.
	 *
	 * @param roundCornerStyle Integer value representing a `DWM_WINDOW_CORNER_PREFERENCE`.
	 *
	 * @see https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setRoundCornerConfig(UINT roundCornerStyle) noexcept
	{
		const auto cornerStyle = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(roundCornerStyle);
		if (cornerStyle > DWMWCP_ROUNDSMALL) // || cornerStyle < DWMWCP_DEFAULT) // should never be < 0
		{
			g_dmCfg._roundCorner = DWMWCP_DEFAULT;
		}
		else
		{
			g_dmCfg._roundCorner = cornerStyle;
		}
	}

	static constexpr DWORD kDwmwaClrDefaultRGBCheck = 0x00FFFFFF;

	/**
	 * @brief Sets the preferred border color for window edge on Windows 11.
	 *
	 * Assigns the given `COLORREF` to the configuration. If the value matches
	 * `kDwmwaClrDefaultRGBCheck`, the color is reset to `DWMWA_COLOR_DEFAULT`.
	 *
	 * @param clr Border color value, or sentinel to reset to system default.
	 *
	 * @see DWMWA_BORDER_COLOR
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setBorderColorConfig(COLORREF clr) noexcept
	{
		if (clr == kDwmwaClrDefaultRGBCheck)
		{
			g_dmCfg._borderColor = DWMWA_COLOR_DEFAULT;
		}
		else
		{
			g_dmCfg._borderColor = clr;
		}
	}

	/**
	 * @brief Sets the Mica effects on Windows 11 setting.
	 *
	 * Assigns a valid `DWM_SYSTEMBACKDROP_TYPE` to the configuration. If the value exceeds
	 * `DWMSBT_TABBEDWINDOW`, it falls back to `DWMSBT_AUTO`.
	 *
	 * @param mica Integer value representing a `DWM_SYSTEMBACKDROP_TYPE`.
	 *
	 * @see DWM_SYSTEMBACKDROP_TYPE
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setMicaConfig(UINT mica) noexcept
	{
		const auto micaType = static_cast<DWM_SYSTEMBACKDROP_TYPE>(mica);
		if (micaType > DWMSBT_TABBEDWINDOW) // || micaType < DWMSBT_AUTO)  // should never be < 0
		{
			g_dmCfg._mica = DWMSBT_AUTO;
		}
		else
		{
			g_dmCfg._mica = micaType;
		}
	}

	/**
	 * @brief Sets Mica effects on the full window setting.
	 *
	 * Controls whether Mica should be applied to the entire window
	 * or limited to the title bar only.
	 *
	 * @param extendMica `true` to apply Mica to the full window, `false` for title bar only.
	 *
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setMicaExtendedConfig(bool extendMica) noexcept
	{
		g_dmCfg._micaExtend = extendMica;
	}

	/**
	 * @brief Sets dialog colors on title bar on Windows 11 setting.
	 *
	 * Controls whether title bar should have same colors as dialog window.
	 *
	 * @param colorize `true` to have title bar to have same colors as dialog window.
	 *
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setColorizeTitleBarConfig(bool colorize) noexcept
	{
		g_dmCfg._colorizeTitleBar = colorize;
	}

	/**
	 * @brief Initializes undocumented dark mode API.
	 *
	 * Wraps `InitDarkMode()` from DarkMode.h.
	 */
	static void initExperimentalDarkMode()
	{
		DarkModeHelper::InitDarkMode();
	}

	/**
	 * @brief Enables or disables dark mode using undocumented API.
	 *
	 * Optionally applies a scrollbar fix for dark mode inconsistencies.
	 *
	 * @param useDark Enable dark mode when `true`, disable when `false`.
	 * @param fixDarkScrollbar Apply scrollbar fix if `true`.
	 */
	static void setDarkMode(bool useDark, bool fixDarkScrollbar = true)
	{
		DarkModeHelper::SetDarkMode(useDark, fixDarkScrollbar);
	}

	/**
	 * @brief Enables or disables dark mode support for a specific window.
	 *
	 * @param hWnd Window handle to apply dark mode.
	 * @param allow Whether to allow (`true`) or disallow (`false`) dark mode.
	 * @return `true` if successfully applied.
	 */
	static bool allowDarkModeForWindow(HWND hWnd, bool allow) noexcept
	{
		return DarkModeHelper::AllowDarkModeForWindow(hWnd, allow);
	}

#if defined(_DARKMODE_SUPPORT_OLDER_OS)
	/**
	 * @brief Refreshes the title bar theme color for legacy systems.
	 *
	 * Used only on old Windows 10 systems when `_DARKMODE_SUPPORT_OLDER_OS` is defined.
	 *
	 * @param hWnd Handle to the window to update.
	 */
	static void setTitleBarThemeColor(HWND hWnd)
	{
		DarkModeHelper::RefreshTitleBarThemeColor(hWnd);
	}
#endif

	/**
	 * @brief Checks whether a `WM_SETTINGCHANGE` message indicates a color scheme switch.
	 *
	 * @param lParam LPARAM from a system message.
	 * @return `true` if the message signals a theme mode change.
	 */
	[[nodiscard]] static bool isColorSchemeChangeMessage(LPARAM lParam)
	{
		return DarkModeHelper::IsColorSchemeChangeMessage(lParam);
	}

	/**
	 * @brief Determines if high contrast mode is currently active.
	 *
	 * @return `true` if high contrast is enabled via system accessibility settings.
	 */
	static bool isHighContrast()
	{
		return DarkModeHelper::IsHighContrast();
	}

	/**
	 * @brief Determines if themed styling should be preferred over subclassing.
	 *
	 * Requires support for experimental theming and Windows 10 or later.
	 *
	 * @return `true` if themed appearance is preferred and supported.
	 */
	static bool isThemePrefered() noexcept
	{
		return (umbra::getLibInfo(LibInfo::preferTheme) == TRUE)
			&& umbra::isAtLeastWindows10()
			&& umbra::isExperimentalSupported();
	}


	/**
	 * @brief Applies dark mode settings based on the given configuration type.
	 *
	 * Initializes the dark mode type settings and system-following behavior.
	 * Enables or disables dark mode depending on whether `DarkModeType::dark` is selected.
	 * It is recommended to use together with @ref umbra::setDefaultColors to also set colors.
	 *
	 * @param dmType Dark mode configuration type; see @ref umbra::initDarkModeConfig for values.
	 *
	 * @see umbra::initDarkModeConfig()
	 * @see umbra::setDefaultColors()
	 */
	void setDarkModeConfig(UINT dmType)
	{
		umbra::initDarkModeConfig(dmType);

		const bool useDark = g_dmCfg._dmType == DarkModeType::dark;
		umbra::setDarkMode(useDark, true);
	}

	/**
	 * @brief Applies dark mode settings based on system mode preference.
	 *
	 * Determines the appropriate mode using @ref umbra::isDarkModeReg and forwards
	 * the result to @ref umbra::setDarkModeConfig.
	 * It is recommended to use together with @ref umbra::setDefaultColors to also set colors.
	 *
	 * Uses:
	 * - `DarkModeType::dark` if registry prefers dark mode.
	 * - `DarkModeType::classic` otherwise.
	 *
	 * @see umbra::setDefaultColors()
	 */
	void setDarkModeConfig()
	{
		const auto dmType = static_cast<UINT>(umbra::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic);
		umbra::setDarkModeConfig(dmType);
	}

	/**
	 * @brief Initializes dark mode experimental features, colors, and system colors.
	 *
	 * Performs one-time setup for dark mode, including:
	 * - Initializing experimental features if not yet done.
	 * - Applying the appearance that follows the current system light/dark setting.
	 * - Initializing TreeView style and applying dark mode settings.
	 * - Preparing system colors (e.g. `COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_BTNFACE`)
	 *   for hooking.
	 *
	 * @note This function is only run once per session;
	 *       subsequent calls have no effect, unless follow system mode is used,
	 *       then only colors are updated each time the system changes mode.
	 *
	 * @see umbra::calculateTreeViewStyle()
	 */
	void initDarkMode()
	{
		if (!g_dmCfg._isInit)
		{
			if (!g_dmCfg._isInitExperimental)
			{
				umbra::initExperimentalDarkMode();
				g_dmCfg._isInitExperimental = true;
			}

			umbra::setDarkModeConfig();
			umbra::setDefaultColors(true);

			umbra::setSysColor(COLOR_WINDOW, umbra::getBackgroundColor());
			umbra::setSysColor(COLOR_WINDOWTEXT, umbra::getTextColor());
			umbra::setSysColor(COLOR_BTNFACE, umbra::getViewGridlinesColor());

			g_dmCfg._isInit = true;
		}
	}

	/**
	 * @brief Checks if non-classic mode is enabled.
	 *
	 * If `_DARKMODE_SUPPORT_OLDER_OS` is defined, this skips Windows version checks.
	 * Otherwise, dark mode is only enabled on Windows 10 or newer.
	 *
	 * @return `true` if a supported dark mode type is active, otherwise `false`.
	 */
	bool isEnabled() noexcept
	{
#if defined(_DARKMODE_SUPPORT_OLDER_OS)
		return g_dmCfg._dmType != DarkModeType::classic;
#else
		return umbra::isAtLeastWindows10() && g_dmCfg._dmType != DarkModeType::classic;
#endif
	}

	/**
	 * @brief Checks if experimental dark mode features are currently active.
	 *
	 * @return `true` if experimental dark mode is enabled.
	 */
	bool isExperimentalActive() noexcept
	{
		return DarkModeHelper::g_darkModeEnabled;
	}

	/**
	 * @brief Checks if experimental dark mode features are supported by the system.
	 *
	 * @return `true` if dark mode experimental APIs are available.
	 */
	bool isExperimentalSupported() noexcept
	{
		return DarkModeHelper::g_darkModeSupported;
	}

	/**
	 * @brief Checks if follow the system mode behavior is enabled.
	 *
	 * @return `true` if "mode" is not `WinMode::disabled`, i.e. system mode is followed.
	 */
	bool isWindowsModeEnabled() noexcept
	{
		return g_dmCfg._windowsMode != WinMode::disabled;
	}

	/**
	 * @brief Checks if the host OS is at least Windows 10.
	 *
	 * @return `true` if running on Windows 10 or newer.
	 */
	bool isAtLeastWindows10() noexcept
	{
		return WinVerHelper::isWindows10_OrLater();
	}
	/**
	 * @brief Checks if the host OS is at least Windows 11.
	 *
	 * @return `true` if running on Windows 11 or newer.
	 */
	bool isAtLeastWindows11() noexcept
	{
		return WinVerHelper::isWindows11_OrLater();
	}

	/**
	 * @brief Retrieves the current Windows build number.
	 *
	 * @return Windows build number reported by the system.
	 */
	DWORD getWindowsBuildNumber() noexcept
	{
		DWORD major = 0, minor = 0, buildNumber = 0;
		WinVerHelper::getOSVersionNumber(major, minor, buildNumber);
		return buildNumber;
	}

	/**
	 * @brief Handles system setting changes related to dark mode.
	 *
	 * Responds to system messages indicating a color scheme change. If the current
	 * dark mode state no longer matches the system registry preference, dark mode is
	 * re-initialized.
	 *
	 * - Skips processing if experimental dark mode is unsupported.
	 * - Relies on @ref umbra::isDarkModeReg for theme preference and skips during high contrast.
	 *
	 * @param lParam Message parameter (typically from `WM_SETTINGCHANGE`).
	 * @return `true` if a dark mode change was handled; otherwise `false`.
	 *
	 * @see umbra::isDarkModeReg()
	 * @see umbra::initDarkMode()
	 */
	bool handleSettingChange(LPARAM lParam)
	{
		if (umbra::isExperimentalSupported() && umbra::isColorSchemeChangeMessage(lParam))
		{
			// ShouldAppsUseDarkMode() is not reliable from 1903+, use umbra::isDarkModeReg() instead
			const bool isDarkModeUsed = umbra::isDarkModeReg() && !umbra::isHighContrast();
			if (umbra::isExperimentalActive() != isDarkModeUsed)
			{
				if (g_dmCfg._isInit)
				{
					g_dmCfg._isInit = false;
					umbra::initDarkMode();
				}
			}
			return true;
		}
		return false;
	}

	/**
	 * @brief Checks if dark mode is enabled in the Windows registry.
	 *
	 * Queries `HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\AppsUseLightTheme`.
	 *
	 * @return `true` if dark mode is preferred (value is `0`); otherwise `false`.
	 */
	bool isDarkModeReg()
	{
		DWORD data{};
		DWORD dwBufSize = sizeof(data);
		LPCWSTR lpSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
		LPCWSTR lpValue = L"AppsUseLightTheme";

		const auto result = ::RegGetValueW(HKEY_CURRENT_USER, lpSubKey, lpValue, RRF_RT_REG_DWORD, nullptr, &data, &dwBufSize);
		if (result != ERROR_SUCCESS)
		{
			return false;
		}

		// dark mode is 0, light mode is 1
		return data == 0UL;
	}

	// from DarkMode.h

	/**
	 * @brief Overrides a specific system color with a custom color.
	 *
	 * Currently supports:
	 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
	 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
	 * - `COLOR_BTNFACE`: Gridline color in ListView (when applicable).
	 *
	 * @param nIndex One of the supported system color indices.
	 * @param color Custom `COLORREF` value to apply.
	 */
	void setSysColor(int nIndex, COLORREF color) noexcept
	{
		SysColorHook::SetMySysColor(nIndex, color);
	}

	/**
	 * @brief Hooks system color to support runtime customization.
	 *
	 * @return `true` if the hook was installed successfully.
	 */
	static bool hookSysColor()
	{
		return SysColorHook::HookSysColor();
	}

	/**
	 * @brief Unhooks system color overrides and restores default color behavior.
	 *
	 * This function is safe to call even if no color hook is currently installed.
	 * It ensures that system colors return to normal without requiring
	 * prior state checks.
	 */
	static void unhookSysColor()
	{
		SysColorHook::UnhookSysColor();
	}

	/**
	 * @brief Makes scroll bars on the specified window and all its children consistent.
	 *
	 * Currently not widely used by default.
	 *
	 * @param hWnd Handle to the parent window.
	 */
	void enableDarkScrollBarForWindowAndChildren(HWND hWnd)
	{
		DarkModeHelper::EnableDarkScrollBarForWindowAndChildren(hWnd);
	}

	/**
	 * @brief Paints a rounded rectangle using the specified pen and brush.
	 *
	 * Draws a rounded rectangle defined by `rect`, using the provided pen (`hpen`) and brush (`hBrush`)
	 * for the edge and fill, respectively. Preserves previous GDI object selections.
	 *
	 * @param hdc Handle to the device context.
	 * @param rect Rectangle bounds for the shape.
	 * @param hpen Pen used to draw the edge.
	 * @param hBrush Brush used to inner fill.
	 * @param width Horizontal corner radius.
	 * @param height Vertical corner radius.
	 */
	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width, int height)
	{
		auto holdBrush = ::SelectObject(hdc, hBrush);
		auto holdPen = ::SelectObject(hdc, hpen);
		::RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, width, height);
		::SelectObject(hdc, holdBrush);
		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Paints an unfilled rounded rectangle (frame only).
	 *
	 * Uses a `NULL_BRUSH` to omit the inner fill, drawing only the rounded frame.
	 *
	 * @param hdc Handle to the device context.
	 * @param rect Rectangle bounds for the frame.
	 * @param hpen Pen used to draw the edge.
	 * @param width Horizontal corner radius.
	 * @param height Vertical corner radius.
	 */
	void paintRoundFrameRect(HDC hdc, const RECT& rect, HPEN hpen, int width, int height)
	{
		umbra::paintRoundRect(hdc, rect, hpen, static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), width, height);
	}

	/**
	 * @class ThemeData
	 * @brief RAII-style wrapper for `HTHEME` handle tied to a specific theme class.
	 *
	 * Prevents leaks by managing the lifecycle of a theme handle opened via `OpenThemeData()`.
	 * Ensures handles are released properly in the destructor via `CloseThemeData()`.
	 *
	 * Usage:
	 * - Construct with a valid theme class name (e.g. `L"Button"`).
	 * - Call `ensureTheme(HWND)` before drawing to open the theme handle.
	 * - Access the active handle via `getHTheme()`.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
	class ThemeData
	{
	public:
		// Constructors
		ThemeData() = delete;
		explicit ThemeData(const wchar_t* themeClass) noexcept
			: _themeClass(themeClass) {};

		// No copyable
		ThemeData(const ThemeData&) = delete;
		ThemeData& operator=(const ThemeData&) = delete;

		// No movable
		ThemeData(ThemeData&&) = delete;
		ThemeData& operator=(ThemeData&&) = delete;

		// Destructor
		~ThemeData() {
			closeTheme();
		}

		bool ensureTheme(HWND hWnd)
		{
			if (_hTheme == nullptr && _themeClass != nullptr)
			{
				_hTheme = ::OpenThemeData(hWnd, _themeClass);
			}
			return _hTheme != nullptr;
		}

		void closeTheme() noexcept
		{
			if (_hTheme != nullptr)
			{
				::CloseThemeData(_hTheme);
				_hTheme = nullptr;
			}
		}

		[[nodiscard]] const HTHEME& getHTheme() const noexcept
		{
			return _hTheme;
		}

	private:
		const wchar_t* _themeClass = nullptr;
		HTHEME _hTheme = nullptr;
	};

	/**
	 * @class BufferData
	 * @brief RAII-style utility for double buffer technique.
	 *
	 * Allocates and resizes an offscreen buffer for flicker-free GDI drawing. When
	 * `ensureBuffer()` is called with a target HDC and client rect, it creates or resizes
	 * a memory device context and bitmap accordingly. Automatically releases resources
	 * via `releaseBuffer()` and destructor.
	 *
	 * Usage:
	 * - Call `ensureBuffer()` before painting.
	 * - Draw to `getHMemDC()`.
	 * - BitBlt back to screen in WM_PAINT.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
	class BufferData
	{
	public:
		BufferData() = default;

		BufferData(const BufferData&) = delete;
		BufferData& operator=(const BufferData&) = delete;

		BufferData(BufferData&&) = delete;
		BufferData& operator=(BufferData&&) = delete;

		~BufferData()
		{
			releaseBuffer();
		}

		bool ensureBuffer(HDC hdc, const RECT& rcClient)
		{
			const int width = rcClient.right - rcClient.left;
			const int height = rcClient.bottom - rcClient.top;

			if (_szBuffer.cx != width || _szBuffer.cy != height)
			{
				releaseBuffer();
				_hMemDC = ::CreateCompatibleDC(hdc);
				_hMemBmp = ::CreateCompatibleBitmap(hdc, width, height);
				_holdBmp = static_cast<HBITMAP>(::SelectObject(_hMemDC, _hMemBmp));
				_szBuffer = { width, height };
			}

			return _hMemDC != nullptr && _hMemBmp != nullptr;
		}

		void releaseBuffer() noexcept
		{
			if (_hMemDC != nullptr)
			{
				::SelectObject(_hMemDC, _holdBmp);
				::DeleteObject(_hMemBmp);
				::DeleteDC(_hMemDC);

				_hMemDC = nullptr;
				_hMemBmp = nullptr;
				_holdBmp = nullptr;
				_szBuffer = { 0, 0 };
			}
		}

		[[nodiscard]] const HDC& getHMemDC() const noexcept
		{
			return _hMemDC;
		}

	private:
		HDC _hMemDC = nullptr;
		HBITMAP _hMemBmp = nullptr;
		HBITMAP _holdBmp = nullptr;
		SIZE _szBuffer{};
	};

	/**
	 * @class FontData
	 * @brief RAII-style wrapper for managing a GDI font (`HFONT`) resource.
	 *
	 * Ensures safe creation, assignment, and destruction of fonts in GDI-based UI code.
	 * Automatically deletes the font in the destructor or when replaced via `setFont()`.
	 *
	 * Usage:
	 * - Use `setFont()` to assign a new font, deleting any previous one.
	 * - `getFont()` provides access to the current `HFONT`.
	 * - `hasFont()` checks if a valid font is currently held.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
	class FontData
	{
	public:
		FontData() = default;

		explicit FontData(HFONT hFont) noexcept
			: _hFont(hFont)
		{}

		FontData(const FontData&) = delete;
		FontData& operator=(const FontData&) = delete;

		FontData(FontData&&) = delete;
		FontData& operator=(FontData&&) = delete;

		~FontData()
		{
			FontData::destroyFont();
		}

		void setFont(HFONT newFont) noexcept
		{
			FontData::destroyFont();
			_hFont = newFont;
		}

		[[nodiscard]] const HFONT& getFont() const noexcept
		{
			return _hFont;
		}

		[[nodiscard]] bool hasFont() const noexcept
		{
			return _hFont != nullptr;
		}

		void destroyFont() noexcept
		{
			if (FontData::hasFont())
			{
				::DeleteObject(_hFont);
				_hFont = nullptr;
			}
		}

	private:
		HFONT _hFont = nullptr;
	};

	/**
	 * @brief Attaches a typed subclass procedure with custom data to a window.
	 *
	 * If the subclass ID is not already attached, allocates a `T` instance using the given
	 * `param` and stores it as subclass reference data. Ownership is transferred to the system.
	 *
	 * @tparam T The user-defined data type associated with the subclass.
	 * @tparam Param Type used to initialize `T`.
	 * @param hWnd Target window.
	 * @param subclassProc Subclass procedure.
	 * @param subclassID Identifier for the subclass instance.
	 * @param param Constructor argument forwarded to `T`.
	 * @return TRUE on success, FALSE on failure, -1 if subclass already set.
	 */
	template <typename T, typename Param>
	static auto setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID, const Param& param) -> int
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			auto pData = std::make_unique<T>(param);
			if (::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	/**
	 * @brief Attaches a typed subclass procedure with default-constructed data.
	 *
	 * Same logic as the other overload, but constructs `T` using its default constructor.
	 *
	 * @tparam T The user-defined data type associated with the subclass.
	 * @param hWnd Target window.
	 * @param subclassProc Subclass procedure.
	 * @param subclassID Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if already subclassed.
	 */
	template <typename T>
	static auto setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID) -> int
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			auto pData = std::make_unique<T>();
			if (::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	/**
	 * @brief Attaches an untyped subclass (no reference data).
	 *
	 * Sets a subclass with no associated custom data.
	 *
	 * @param hWnd Target window.
	 * @param subclassProc Subclass procedure.
	 * @param subclassID Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if already subclassed.
	 */
	static int setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID)
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			return ::SetWindowSubclass(hWnd, subclassProc, subclassID, 0);
		}
		return -1;
	}

	/**
	 * @brief Removes a subclass and deletes associated user data (if provided).
	 *
	 * Retrieves and deletes user-defined `T` data stored in subclass reference
	 * (unless `T = void`, in which case no delete is performed). Then removes the subclass.
	 *
	 * @tparam T Optional type of reference data to delete.
	 * @param hWnd Window handle.
	 * @param subclassProc Subclass procedure.
	 * @param subclassID Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if not present.
	 */
	template <typename T = void>
	static auto removeSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID) -> int
	{
		T* pData = nullptr;

		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR*>(&pData)) == TRUE)
		{
			if constexpr (!std::is_void_v<T>)
			{
				if (pData != nullptr)
				{
					delete pData;
					pData = nullptr;
				}
			}
			return ::RemoveWindowSubclass(hWnd, subclassProc, subclassID);
		}
		return -1;
	}

	/**
	 * @struct ButtonData
	 * @brief Stores button theming state and original size metadata.
	 *
	 * Used for checkbox, radio, tri-state, or group box buttons. Used in conjunction
	 * with subclassing of button controls to preserve original layout dimensions
	 * and apply consistent visual styling. Captures the control's client size
	 * for checkbox, radio, or tri-state buttons.
	 *
	 * Members:
	 * - `_themeData` : RAII-managed theme handle for `VSCLASS_BUTTON`.
	 * - `_szBtn` : Original size extracted from the button rectangle.
	 * - `_iStateID` : Current visual state ID (e.g. pressed, disabled, ...).
	 * - `_isSizeSet` : Indicates whether `_szBtn` holds a valid measurement.
	 *
	 * Constructor behavior:
	 * - When constructed with an `HWND`, attempts to extract the initial size if the button
	 *   is a checkbox/radio/tri-state type without `BS_MULTILINE`.
	 */
	struct ButtonData
	{
		ThemeData _themeData{ VSCLASS_BUTTON };
		SIZE _szBtn{};

		int _iStateID = 0;
		bool _isSizeSet = false;

		ButtonData() = default;

		// Saves width and height from the resource file for use as restrictions.
		// Currently unused / have no effect.
		explicit ButtonData(HWND hWnd)
		{
			const auto nBtnStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			switch (nBtnStyle & BS_TYPEMASK)
			{
				case BS_CHECKBOX:
				case BS_AUTOCHECKBOX:
				case BS_3STATE:
				case BS_AUTO3STATE:
				case BS_RADIOBUTTON:
				case BS_AUTORADIOBUTTON:
				{
					if ((nBtnStyle & BS_MULTILINE) != BS_MULTILINE)
					{
						RECT rcBtn{};
						::GetClientRect(hWnd, &rcBtn);
						_szBtn.cx = rcBtn.right - rcBtn.left;
						_szBtn.cy = rcBtn.bottom - rcBtn.top;
						_isSizeSet = (_szBtn.cx != 0 && _szBtn.cy != 0);
					}
					break;
				}

				default:
				{
					break;
				}
			}
		}
	};

	/**
	 * @brief Draws a themed owner drawn checkbox, radio, or tri-state button (excluding push-like buttons).
	 *
	 * Internally used by @ref umbra::paintButton to draw visual elements such as checkbox glyphs
	 * or radio indicators alongside styled text. Not used for buttons with `BS_PUSHLIKE`,
	 * which require different handling and theming logic.
	 *
	 * - Retrieves themed or fallback font for consistent appearance.
	 * - Handles alignment, word wrapping, and prefix visibility per style flags.
	 * - Draws themed background and glyph using `DrawThemeBackground`.
	 * - Uses dark mode-aware text rendering and applies focus cue when needed.
	 *
	 * @param hWnd Handle to the button control.
	 * @param hdc Device context for drawing.
	 * @param hTheme Active visual style theme handle.
	 * @param iPartID Part ID (`BP_CHECKBOX`, `BP_RADIOBUTTON`, etc.).
	 * @param iStateID State ID (`CBS_CHECKEDHOT`, `RBS_UNCHECKEDNORMAL`, etc.).
	 *
	 * @see umbra::paintButton()
	 */
	static void renderButton(HWND hWnd, HDC hdc, HTHEME hTheme, int iPartID, int iStateID)
	{
		// Font part

		HFONT hFont = nullptr;
		bool isFontCreated = false;
		LOGFONT lf{};
		if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
		{
			hFont = ::CreateFontIndirect(&lf);
			isFontCreated = true;
		}

		if (hFont == nullptr)
		{
			hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		}

		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		// Style part

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isMultiline = (nStyle & BS_MULTILINE) == BS_MULTILINE;
		const bool isTop = (nStyle & BS_TOP) == BS_TOP;
		const bool isBottom = (nStyle & BS_BOTTOM) == BS_BOTTOM;
		const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;
		const bool isRight = (nStyle & BS_RIGHT) == BS_RIGHT;
		const bool isVCenter = (nStyle & BS_VCENTER) == BS_VCENTER;

		DWORD dtFlags = DT_LEFT;
		if (isMultiline)
		{
			dtFlags |= DT_WORDBREAK;
		}
		else
		{
			dtFlags |= DT_SINGLELINE;
		}

		if (isCenter)
		{
			dtFlags |= DT_CENTER;
		}
		else if (isRight)
		{
			dtFlags |= DT_RIGHT;
		}

		if (isVCenter || (!isMultiline && !isBottom && !isTop))
		{
			dtFlags |= DT_VCENTER;
		}
		else if (isBottom)
		{
			dtFlags |= DT_BOTTOM;
		}

		const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));
		const bool hidePrefix = (uiState & UISF_HIDEACCEL) == UISF_HIDEACCEL;
		if (hidePrefix)
		{
			dtFlags |= DT_HIDEPREFIX;
		}

		// Text and box part

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		std::wstring buffer;
		const auto bufferLen = static_cast<size_t>(::GetWindowTextLength(hWnd));
		buffer.resize(bufferLen + 1, L'\0');
		::GetWindowText(hWnd, buffer.data(), static_cast<int>(buffer.length()));

		SIZE szBox{};
		::GetThemePartSize(hTheme, hdc, iPartID, iStateID, nullptr, TS_DRAW, &szBox);

		RECT rcText{};
		::GetThemeBackgroundContentRect(hTheme, hdc, iPartID, iStateID, &rcClient, &rcText);

		RECT rcBackground{ rcClient };
		if (!isMultiline)
		{
			rcBackground.top += (rcText.bottom - rcText.top - szBox.cy) / 2;
		}
		rcBackground.bottom = rcBackground.top + szBox.cy;
		rcBackground.right = rcBackground.left + szBox.cx;
		rcText.left = rcBackground.right + 3;

		::DrawThemeParentBackground(hWnd, hdc, &rcClient);
		::DrawThemeBackground(hTheme, hdc, iPartID, iStateID, &rcBackground, nullptr); // draw box

		DTTOPTS dtto{};
		dtto.dwSize = sizeof(DTTOPTS);
		dtto.dwFlags = DTT_TEXTCOLOR;
		dtto.crText = (::IsWindowEnabled(hWnd) == FALSE) ? umbra::getDisabledTextColor() : umbra::getTextColor();

		::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags, &rcText, &dtto);

		// Focus rect

		const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
		if (((nState & BST_FOCUS) == BST_FOCUS) && ((uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS))
		{
			dtto.dwFlags |= DTT_CALCRECT;
			::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags | DT_CALCRECT, &rcText, &dtto);
			const RECT rcFocus{ rcText.left - 1, rcText.top, rcText.right + 1, rcText.bottom + 1 };
			::DrawFocusRect(hdc, &rcFocus);
		}

		// Cleanup

		::SelectObject(hdc, holdFont);
		if (isFontCreated)
		{
			::DeleteObject(hFont);
		}
	}

	/**
	 * @brief Paints a checkbox, radio, or tri-state button with state-based visuals.
	 *
	 * Determines the appropriate themed part and state ID based on the control’s
	 * style (e.g. `BS_CHECKBOX`, `BS_RADIOBUTTON`) and current button state flags
	 * such as `BST_CHECKED`, `BST_PUSHED`, or `BST_HOT`.
	 *
	 * - Uses buffered animation (if available) to smoothly transition between states.
	 * - Falls back to direct drawing via @ref umbra::renderButton if animation is not used.
	 * - Internally updates the `buttonData._iStateID` to preserve the last rendered state.
	 * - Not used for `BS_PUSHLIKE` buttons.
	 *
	 * @param hWnd Handle to the checkbox or radio button control.
	 * @param hdc Device context used for rendering.
	 * @param buttonData Theming and state info, including current theme and last state.
	 *
	 * @see umbra::renderButton()
	 */
	static void paintButton(HWND hWnd, HDC hdc, ButtonData& buttonData)
	{
		const auto& hTheme = buttonData._themeData.getHTheme();

		const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const auto nBtnStyle = nStyle & BS_TYPEMASK;

		int iPartID = 0;
		int iStateID = 0;

		// Get style
		switch (nBtnStyle)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
			{
				iPartID = BP_CHECKBOX;

				if (::IsWindowEnabled(hWnd) == FALSE)           { iStateID = CBS_UNCHECKEDDISABLED; }
				else if ((nState & BST_PUSHED) == BST_PUSHED)   { iStateID = CBS_UNCHECKEDPRESSED; }
				else if ((nState & BST_HOT) == BST_HOT)         { iStateID = CBS_UNCHECKEDHOT; }
				else                                            { iStateID = CBS_UNCHECKEDNORMAL; }

				static constexpr int checkedOffset = 4;
				static constexpr int mixedOffset = 8;
				if ((nState & BST_CHECKED) == BST_CHECKED)      { iStateID += checkedOffset; }
				else if ((nState & BST_INDETERMINATE) == BST_INDETERMINATE) { iStateID += mixedOffset; }

				break;
			}

			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			{
				iPartID = BP_RADIOBUTTON;

				if (::IsWindowEnabled(hWnd) == FALSE)           { iStateID = RBS_UNCHECKEDDISABLED; }
				else if ((nState & BST_PUSHED) == BST_PUSHED)   { iStateID = RBS_UNCHECKEDPRESSED; }
				else if ((nState & BST_HOT) == BST_HOT)         { iStateID = RBS_UNCHECKEDHOT; }
				else                                            { iStateID = RBS_UNCHECKEDNORMAL; }

				if ((nState & BST_CHECKED) == BST_CHECKED)      { iStateID += 4; }

				break;
			}

			default: // should never happen
			{
				iPartID = BP_CHECKBOX;
				iStateID = CBS_UNCHECKEDDISABLED;
				break;
			}
		}

		if (::BufferedPaintRenderAnimation(hWnd, hdc) == TRUE)
		{
			return;
		}

		// Animation part - hover transition

		BP_ANIMATIONPARAMS animParams{};
		animParams.cbSize = sizeof(BP_ANIMATIONPARAMS);
		animParams.style = BPAS_LINEAR;
		if (iStateID != buttonData._iStateID)
		{
			::GetThemeTransitionDuration(hTheme, iPartID, buttonData._iStateID, iStateID, TMT_TRANSITIONDURATIONS, &animParams.dwDuration);
		}

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		HDC hdcFrom = nullptr;
		HDC hdcTo = nullptr;
		HANIMATIONBUFFER hbpAnimation = ::BeginBufferedAnimation(hWnd, hdc, &rcClient, BPBF_COMPATIBLEBITMAP, nullptr, &animParams, &hdcFrom, &hdcTo);
		if (hbpAnimation != nullptr)
		{
			if (hdcFrom != nullptr)
			{
				umbra::renderButton(hWnd, hdcFrom, hTheme, iPartID, buttonData._iStateID);
			}
			if (hdcTo != nullptr)
			{
				umbra::renderButton(hWnd, hdcTo, hTheme, iPartID, iStateID);
			}

			buttonData._iStateID = iStateID;

			::EndBufferedAnimation(hbpAnimation, TRUE);
		}
		else
		{
			umbra::renderButton(hWnd, hdc, hTheme, iPartID, iStateID);

			buttonData._iStateID = iStateID;
		}
	}

	/**
	 * @brief Window subclass procedure for themed owner drawn checkbox, radio, and tri-state buttons.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData ButtonData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setCheckboxOrRadioBtnCtrlSubclass()
	 * @see umbra::removeCheckboxOrRadioBtnCtrlSubclass()
	 */
	static LRESULT CALLBACK ButtonSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
		auto& themeData = pButtonData->_themeData;

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ButtonSubclass, uIdSubclass);
				delete pButtonData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}
				return TRUE;
			}

			case WM_PRINTCLIENT:
			case WM_PAINT:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				PAINTSTRUCT ps{};
				auto hdc = reinterpret_cast<HDC>(wParam);
				if (hdc == nullptr)
				{
					hdc = ::BeginPaint(hWnd, &ps);
				}

				umbra::paintButton(hWnd, hdc, *pButtonData);

				if (ps.hdc != nullptr)
				{
					::EndPaint(hWnd, &ps);
				}

				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_SIZE:
			case WM_DESTROY:
			{
				::BufferedPaintStopAllAnimations(hWnd);
				break;
			}

			case WM_ENABLE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				// Skip the button's normal wndproc so it won't redraw out of wm_paint
				const LRESULT retVal = ::DefWindowProc(hWnd, uMsg, wParam, lParam);
				::InvalidateRect(hWnd, nullptr, FALSE);
				return retVal;
			}

			case WM_UPDATEUISTATE:
			{
				if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
				{
					::InvalidateRect(hWnd, nullptr, FALSE);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies themed owner drawn subclassing to a checkbox, radio, or tri-state button control.
	 *
	 * Associates a `ButtonData` instance with the control.
	 *
	 * @param hWnd Handle to the checkbox, radio, or tri-state button control.
	 *
	 * @see umbra::ButtonSubclass()
	 * @see umbra::removeCheckboxOrRadioBtnCtrlSubclass()
	 */
	void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<ButtonData>(hWnd, ButtonSubclass, kButtonSubclassID, hWnd);
	}

	/**
	 * @brief Removes the owner drawn subclass from a checkbox, radio, or tri-state button control.
	 *
	 * Cleans up the `ButtonData` instance and detaches the control's subclass proc.
	 *
	 * @param hWnd Handle to the control previously subclassed.
	 *
	 * @see umbra::ButtonSubclass()
	 * @see umbra::setCheckboxOrRadioBtnCtrlSubclass()
	 */
	void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<ButtonData>(hWnd, ButtonSubclass, kButtonSubclassID);
	}

	/**
	 * @brief Paints a group box frame and text with custom colors.
	 *
	 * Handles drawing a themed group box with optional centered text, styled borders,
	 * and font fallback. If a caption text is present, the frame is clipped to avoid overdrawing
	 * behind the text. The function adapts layout for both centered and left-aligned titles.
	 *
	 * Rendering steps:
	 * - Determines current visual state (`GBS_DISABLED`, `GBS_NORMAL`).
	 * - Retrieves themed font via `GetThemeFont` or falls back to dialog font.
	 * - Measures caption text, computes layout and exclusion for frame clipping.
	 * - Paints the outer rounded frame via @ref umbra::paintRoundFrameRect
	 *   using `umbra::getEdgePen()`.
	 * - Restores clip region and renders text using `DrawThemeTextEx` with custom colors.
	 *
	 * @param hWnd Handle to the group box control.
	 * @param hdc Device context used for painting.
	 * @param buttonData Reference to the theming and state info (theme handle).
	 *
	 * @note Ensures proper cleanup of temporary GDI objects (font, clip region).
	 *
	 * @see umbra::paintRoundFrameRect()
	 */
	static void paintGroupbox(HWND hWnd, HDC hdc, const ButtonData& buttonData)
	{
		const auto& hTheme = buttonData._themeData.getHTheme();

		// Style part

		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		static constexpr int iPartID = BP_GROUPBOX;
		const int iStateID = isDisabled ? GBS_DISABLED : GBS_NORMAL;

		// Font part

		bool isFontCreated = false;
		HFONT hFont = nullptr;
		LOGFONT lf{};
		if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
		{
			hFont = ::CreateFontIndirect(&lf);
			isFontCreated = true;
		}

		if (hFont == nullptr)
		{
			hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
			isFontCreated = false;
		}

		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		// Text rectangle part

		std::wstring buffer;
		const auto bufferLen = static_cast<size_t>(::GetWindowTextLength(hWnd));
		if (bufferLen > 0)
		{
			buffer.resize(bufferLen + 1, L'\0');
			::GetWindowText(hWnd, buffer.data(), static_cast<int>(buffer.length()));
		}

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		rcClient.bottom -= 1;

		RECT rcText{ rcClient };
		RECT rcBackground{ rcClient };
		if (!buffer.empty())
		{
			SIZE szText{};
			::GetTextExtentPoint32(hdc, buffer.c_str(), static_cast<int>(bufferLen), &szText);

			const int centerPosX = isCenter ? ((rcClient.right - rcClient.left - szText.cx) / 2) : 7;

			rcBackground.top += szText.cy / 2;
			rcText.left += centerPosX;
			rcText.bottom = rcText.top + szText.cy;
			rcText.right = rcText.left + szText.cx + 4;

			::ExcludeClipRect(hdc, rcText.left, rcText.top, rcText.right, rcText.bottom);
		}
		else // There is no text, use "M" to get metrics to move top edge down
		{
			SIZE szText{};
			::GetTextExtentPoint32(hdc, L"M", 1, &szText);
			rcBackground.top += szText.cy / 2;
		}

		RECT rcContent = rcBackground;
		::GetThemeBackgroundContentRect(hTheme, hdc, BP_GROUPBOX, iStateID, &rcBackground, &rcContent);
		::ExcludeClipRect(hdc, rcContent.left, rcContent.top, rcContent.right, rcContent.bottom);

		umbra::paintRoundFrameRect(hdc, rcBackground, umbra::getEdgePen()); // main frame

		::SelectClipRgn(hdc, nullptr);

		// Text part

		if (!buffer.empty())
		{
			::InflateRect(&rcText, -2, 0);

			DTTOPTS dtto{};
			dtto.dwSize = sizeof(DTTOPTS);
			dtto.dwFlags = DTT_TEXTCOLOR;
			dtto.crText = isDisabled ? umbra::getDisabledTextColor() : umbra::getTextColor();

			DWORD dtFlags = isCenter ? DT_CENTER : DT_LEFT;

			if (::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0) != 0) // NULL
			{
				dtFlags |= DT_HIDEPREFIX;
			}

			::DrawThemeTextEx(hTheme, hdc, BP_GROUPBOX, iStateID, buffer.c_str(), -1, dtFlags | DT_SINGLELINE, &rcText, &dtto);
		}

		::SelectObject(hdc, holdFont);
		if (isFontCreated)
		{
			::DeleteObject(hFont);
		}
	}

	/**
	 * @brief Window subclass procedure for owner drawn groupbox button control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData ButtonData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setGroupboxCtrlSubclass()
	 * @see umbra::removeGroupboxCtrlSubclass()
	 */
	static LRESULT CALLBACK GroupboxSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
		auto& themeData = pButtonData->_themeData;

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, GroupboxSubclass, uIdSubclass);
				delete pButtonData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}
				return TRUE;
			}

			case WM_PRINTCLIENT:
			case WM_PAINT:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				PAINTSTRUCT ps{};
				auto hdc = reinterpret_cast<HDC>(wParam);
				if (hdc == nullptr)
				{
					hdc = ::BeginPaint(hWnd, &ps);
				}

				umbra::paintGroupbox(hWnd, hdc, *pButtonData);

				if (ps.hdc != nullptr)
				{
					::EndPaint(hWnd, &ps);
				}

				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_ENABLE:
			{
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies owner drawn subclassing to a groupbox button control.
	 *
	 * Associates a `ButtonData` instance with the control.
	 *
	 * @param hWnd Handle to the groupbox button control.
	 *
	 * @see umbra::GroupboxSubclass()
	 * @see umbra::removeGroupboxCtrlSubclass()
	 */
	void setGroupboxCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<ButtonData>(hWnd, GroupboxSubclass, kGroupboxSubclassID);
	}

	/**
	 * @brief Removes the owner drawn subclass from a groupbox button control.
	 *
	 * Cleans up the `ButtonData` instance and detaches the control's subclass proc.
	 *
	 * @param hWnd Handle to the control previously subclassed.
	 *
	 * @see umbra::GroupboxSubclass()
	 * @see umbra::setGroupboxCtrlSubclass()
	 */
	void removeGroupboxCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<ButtonData>(hWnd, GroupboxSubclass, kGroupboxSubclassID);
	}

	/**
	 * @brief Applies theming and/or subclassing to a button control based on its style.
	 *
	 * Inspects the control's style (`BS_*`) to determine its visual category and applies
	 * apropriate theming and/or subclassing accordingly. Handles:
	 * - Checkbox/radio/tri-state buttons: Applies theme (optional) and optional subclassing
	 * - Group boxes: Applies subclassing for dark mode drawing
	 * - Push buttons: Applies visual theming if requested
	 *
	 * The behavior varies depending on dark mode support, Windows version, and the flags
	 * provided in @ref DarkModeParams.
	 *
	 * @param hWnd Handle to the target button control.
	 * @param p Parameters defining theming and subclassing behavior.
	 *
	 * @see DarkModeParams
	 * @see umbra::setCheckboxOrRadioBtnCtrlSubclass()
	 * @see umbra::setGroupboxCtrlSubclass()
	 */
	static void setBtnCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		const auto nBtnStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		switch (nBtnStyle & BS_TYPEMASK)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			{
				if ((nBtnStyle & BS_PUSHLIKE) == BS_PUSHLIKE)
				{
					if (p._theme)
					{
						::SetWindowTheme(hWnd, p._themeClassName, nullptr);
					}
					break;
				}

				if (umbra::isAtLeastWindows11() && p._theme)
				{
					::SetWindowTheme(hWnd, p._themeClassName, nullptr);
				}

				if (p._subclass)
				{
					umbra::setCheckboxOrRadioBtnCtrlSubclass(hWnd);
				}
				break;
			}

			case BS_GROUPBOX:
			{
				if (p._subclass)
				{
					umbra::setGroupboxCtrlSubclass(hWnd);
				}
				break;
			}

			case BS_PUSHBUTTON:
			case BS_DEFPUSHBUTTON:
			case BS_SPLITBUTTON:
			case BS_DEFSPLITBUTTON:
			{
				if (p._theme)
				{
					::SetWindowTheme(hWnd, p._themeClassName, nullptr);
				}
				break;
			}

			default:
			{
				break;
			}
		}
	}

	/**
	 * @struct UpDownData
	 * @brief Stores layout and rendering state for a owner drawn updown (spinner) control.
	 *
	 * Used to manage rectangle, buffer, and hit-test regions for owner-drawn subclassed
	 * up-down controls, supporting both vertical and horizontal layouts.
	 *
	 * Key members:
	 * - `_bufferData`: Offscreen back buffer for flicker-free rendering.
	 * - `_rcClient`: Current client rectangle of the control.
	 * - `_rcPrev`, `_rcNext`: Rectangles for the up/down or left/right arrow buttons.
	 * - `_cornerRoundness`: Optional roundness for corners (used in Windows 11+ with tabs).
	 * - `_isHorizontal`: `true` if the control is horizontal (`UDS_HORZ` style).
	 * - `_wasHotNext`: Last hover state (used for hover feedback/rendering).
	 *
	 * Construction:
	 * - Detects orientation from `GWL_STYLE`.
	 * - Initializes corner styling based on OS and parent class.
	 * - Extracts rectangles for arrow segments immediately.
	 *
	 * Usage:
	 * - `updateRect(HWND)`: Refreshes rectangle from control handle.
	 * - `updateRect(RECT)`: Checks for rectangle change and updates it.
	 */
	struct UpDownData
	{
		BufferData _bufferData;

		RECT _rcClient{};
		RECT _rcPrev{};
		RECT _rcNext{};
		int _cornerRoundness = 0;
		bool _isHorizontal = false;
		bool _wasHotNext = false;

		UpDownData() = delete;

		explicit UpDownData(HWND hWnd)
			: _cornerRoundness((umbra::isAtLeastWindows11() && CmpWndClassName(::GetParent(hWnd), WC_TABCONTROL)) ? (kWin11CornerRoundness + 1) : 0)
			, _isHorizontal((::GetWindowLongPtr(hWnd, GWL_STYLE) & UDS_HORZ) == UDS_HORZ)
		{
			updateRect(hWnd);
		}

		void updateRectUpDown() noexcept
		{
			if (_isHorizontal)
			{
				const RECT rcArrowLeft{
					_rcClient.left, _rcClient.top,
					_rcClient.right - ((_rcClient.right - _rcClient.left) / 2) - 1, _rcClient.bottom
				};

				const RECT rcArrowRight{
					rcArrowLeft.right + 1, _rcClient.top,
					_rcClient.right, _rcClient.bottom
				};

				_rcPrev = rcArrowLeft;
				_rcNext = rcArrowRight;
			}
			else
			{
				static constexpr LONG offset = 2;

				const RECT rcArrowTop{
					_rcClient.left + offset, _rcClient.top,
					_rcClient.right, _rcClient.bottom - ((_rcClient.bottom - _rcClient.top) / 2)
				};

				const RECT rcArrowBottom{
					_rcClient.left + offset, rcArrowTop.bottom,
					_rcClient.right, _rcClient.bottom
				};

				_rcPrev = rcArrowTop;
				_rcNext = rcArrowBottom;
			}
		}

		void updateRect(HWND hWnd)
		{
			::GetClientRect(hWnd, &_rcClient);
			updateRectUpDown();
		}

		bool updateRect(RECT rcClientNew)
		{
			if (::EqualRect(&_rcClient, &rcClientNew) == FALSE)
			{
				_rcClient = rcClientNew;
				updateRectUpDown();
				return true;
			}
			return false;
		}
	};

	/**
	 * @brief Custom paints a updown (spinner) control.
	 *
	 * Renders the two-button control using custom color brushes, pen styles, and directional
	 * arrows. Adapts to both vertical and horizontal orientation based on @ref UpDownData.
	 * Applies hover highlighting and draws appropriate glyphs (`<`/`>` or `˄`/`˅`) using
	 * the control's font.
	 *
	 * Paint logic includes:
	 * - Background fill with dialog background brush
	 * - Rounded corners (optional, based on Windows 11 and parent class)
	 * - Direction-aware layout and glyph placement
	 *
	 * @param hWnd Handle to the updown control being painted.
	 * @param hdc Target device context.
	 * @param upDownData Reference to layout and state information (segments, orientation, corner radius).
	 *
	 * @note Assumes the DC has already been prepared for painting. Uses `WM_GETFONT` to
	 *       match the host UI font.
	 *
	 * @see UpDownData
	 */
	static void paintUpDown(HWND hWnd, HDC hdc, UpDownData& upDownData)
	{
		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		const int roundness = upDownData._cornerRoundness;

		::FillRect(hdc, &upDownData._rcClient, umbra::getDlgBackgroundBrush());
		::SetBkMode(hdc, TRANSPARENT);

		// Button part

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isHotPrev = ::PtInRect(&upDownData._rcPrev, ptCursor) == TRUE;
		const bool isHotNext = ::PtInRect(&upDownData._rcNext, ptCursor) == TRUE;

		upDownData._wasHotNext = !isHotPrev && (::PtInRect(&upDownData._rcClient, ptCursor) == TRUE);

		auto paintUpDownBtn = [&](const RECT& rect, bool isHot) -> void {
			HBRUSH hBrush = nullptr;
			HPEN hPen = nullptr;
			if (isDisabled)
			{
				hBrush = umbra::getDlgBackgroundBrush();
				hPen = umbra::getDisabledEdgePen();
			}
			else if (isHot)
			{
				hBrush = umbra::getHotBackgroundBrush();
				hPen = umbra::getHotEdgePen();
			}
			else
			{
				hBrush = umbra::getCtrlBackgroundBrush();
				hPen = umbra::getEdgePen();
			}

			umbra::paintRoundRect(hdc, rect, hPen, hBrush, roundness, roundness);
		};

		paintUpDownBtn(upDownData._rcPrev, isHotPrev);
		paintUpDownBtn(upDownData._rcNext, isHotNext);

		// Glyph part

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
		const COLORREF clrText = isDisabled ? umbra::getDisabledTextColor() : umbra::getDarkerTextColor();

		const LONG offset = upDownData._isHorizontal ? 1 : 0;
		RECT rcTectPrev{ upDownData._rcPrev.left, upDownData._rcPrev.top, upDownData._rcPrev.right, upDownData._rcPrev.bottom - offset };
		::SetTextColor(hdc, isHotPrev ? umbra::getTextColor() : clrText);
		::DrawText(hdc, upDownData._isHorizontal ? L"<" : L"˄", -1, &rcTectPrev, dtFlags);

		RECT rcTectNext{ upDownData._rcNext.left + offset, upDownData._rcNext.top, upDownData._rcNext.right, upDownData._rcNext.bottom - offset };
		::SetTextColor(hdc, isHotNext ? umbra::getTextColor() : clrText);
		::DrawText(hdc, upDownData._isHorizontal ? L">" : L"˅", -1, &rcTectNext, dtFlags);

		::SelectObject(hdc, holdFont);
	}

	/**
	 * @brief Window subclass procedure for owner drawn updown (spinner) control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData UpDownData instance .
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setUpDownCtrlSubclass()
	 * @see umbra::removeUpDownCtrlSubclass()
	 */
	static LRESULT CALLBACK UpDownSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pUpDownData = reinterpret_cast<UpDownData*>(dwRefData);
		auto& bufferData = pUpDownData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, UpDownSubclass, uIdSubclass);
				delete pUpDownData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				if (!pUpDownData->_isHorizontal)
				{
					::OffsetRect(&ps.rcPaint, 2, 0);
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				pUpDownData->updateRect(rcClient);
				if (!pUpDownData->_isHorizontal)
				{
					::OffsetRect(&rcClient, 2, 0);
				}

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					umbra::paintUpDown(hWnd, hMemDC, *pUpDownData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				pUpDownData->updateRect(hWnd);
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				if (pUpDownData->_wasHotNext)
				{
					pUpDownData->_wasHotNext = false;
					::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				}

				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				pUpDownData->_wasHotNext = false;
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);

				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies owner drawn subclassing and theming to an updown (spinner) control.
	 *
	 * Associates a `UpDownData` instance with the control.
	 *
	 * @param hWnd Handle to the updown (spinner) control.
	 *
	 * @see umbra::UpDownSubclass()
	 * @see umbra::removeUpDownCtrlSubclass()
	 */
	void setUpDownCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<UpDownData>(hWnd, UpDownSubclass, kUpDownSubclassID, hWnd);
		umbra::setDarkExplorerTheme(hWnd);
	}

	/**
	 * @brief Removes the owner drawn subclass from a updown (spinner) control.
	 *
	 * Cleans up the `UpDownData` instance and detaches the control's subclass proc.
	 *
	 * @param hWnd Handle to the control previously subclassed.
	 *
	 * @see umbra::UpDownSubclass()
	 * @see umbra::setUpDownCtrlSubclass()
	 */
	void removeUpDownCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<UpDownData>(hWnd, UpDownSubclass, kUpDownSubclassID);
	}

	/**
	 * @brief Applies updown (spinner) control theming and/or subclassing based on specified parameters.
	 *
	 * Conditionally applies custom subclassing and/or themed appearance depending on
	 * `DarkModeParams`. Subclassing takes priority if both are requested.
	 *
	 * @param hWnd Handle to the up-down control.
	 * @param p Parameters controlling whether to apply theming and/or subclassing.
	 *
	 * @see DarkModeParams
	 * @see umbra::setUpDownCtrlSubclass()
	 */
	static void setUpDownCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			umbra::setUpDownCtrlSubclass(hWnd);
		}
		else if (p._theme)
		{
			::SetWindowTheme(hWnd, p._themeClassName, nullptr);
		}
	}

	static void paintTab(HWND hWnd, HDC hdc, const RECT& rect)
	{
		::FillRect(hdc, &rect, umbra::getDlgBackgroundBrush());

		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, umbra::getEdgePen()));

		auto holdClip = ::CreateRectRgn(0, 0, 0, 0);
		if (::GetClipRgn(hdc, holdClip) != 1)
		{
			::DeleteObject(holdClip);
			holdClip = nullptr;
		}

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = ::SelectObject(hdc, hFont);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		bool hasFocusRect = false;
		if (::GetFocus() == hWnd)
		{
			const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));
			hasFocusRect = ((uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS);
		}

		const int iSelTab = TabCtrl_GetCurSel(hWnd);
		const int nTabs = TabCtrl_GetItemCount(hWnd);
		for (int i = 0; i < nTabs; ++i)
		{
			RECT rcItem{};
			TabCtrl_GetItemRect(hWnd, i, &rcItem);
			RECT rcFrame{ rcItem };

			RECT rcIntersect{};
			if (::IntersectRect(&rcIntersect, &rect, &rcItem) == TRUE)
			{
				const bool isHot = ::PtInRect(&rcItem, ptCursor) == TRUE;
				const bool isSelectedTab = (i == iSelTab);

				::SetBkMode(hdc, TRANSPARENT);

				HRGN hClip = ::CreateRectRgnIndirect(&rcItem);
				::SelectClipRgn(hdc, hClip);

				::InflateRect(&rcItem, -1, -1);
				rcItem.right += 1;

				std::wstring label(MAX_PATH, L'\0');
				TCITEM tci{};
				tci.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
				tci.dwStateMask = TCIS_HIGHLIGHTED;
				tci.pszText = label.data();
				tci.cchTextMax = MAX_PATH - 1;

				TabCtrl_GetItem(hWnd, i, &tci);

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				const bool isBtn = (nStyle & TCS_BUTTONS) == TCS_BUTTONS;
				if (isBtn)
				{
					const bool isHighlighted = (tci.dwState & TCIS_HIGHLIGHTED) == TCIS_HIGHLIGHTED;
					::FillRect(hdc, &rcItem, isHighlighted ? umbra::getHotBackgroundBrush() : umbra::getDlgBackgroundBrush());
					::SetTextColor(hdc, isHighlighted ? umbra::getLinkTextColor() : umbra::getDarkerTextColor());
				}
				else
				{
					// for consistency getBackgroundBrush()
					// would be better, than getCtrlBackgroundBrush(),
					// however default getBackgroundBrush() has same color
					// as getDlgBackgroundBrush()
					auto getBrush = [&]() -> HBRUSH {
						if (isSelectedTab)
						{
							return umbra::getDlgBackgroundBrush();
						}

						if (isHot)
						{
							return umbra::getHotBackgroundBrush();
						}
						return umbra::getCtrlBackgroundBrush();
					};

					::FillRect(hdc, &rcItem, getBrush());
					::SetTextColor(hdc, (isHot || isSelectedTab) ? umbra::getTextColor() : umbra::getDarkerTextColor());
				}

				RECT rcText{ rcItem };
				if (!isBtn)
				{
					if (isSelectedTab)
					{
						::OffsetRect(&rcText, 0, -1);
						::InflateRect(&rcFrame, 0, 1);
					}

					if (i != nTabs - 1)
					{
						rcFrame.right += 1;
					}
				}

				if (tci.iImage != -1)
				{
					int cx = 0;
					int cy = 0;
					auto hImagelist = TabCtrl_GetImageList(hWnd);
					static constexpr int offset = 2;
					::ImageList_GetIconSize(hImagelist, &cx, &cy);
					::ImageList_Draw(hImagelist, tci.iImage, hdc, rcText.left + offset, rcText.top + (((rcText.bottom - rcText.top) - cy) / 2), ILD_NORMAL);
					rcText.left += cx;
				}

				::DrawText(hdc, label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				::FrameRect(hdc, &rcFrame, umbra::getEdgeBrush());

				if (isSelectedTab && hasFocusRect)
				{
					::InflateRect(&rcFrame, -2, -1);
					::DrawFocusRect(hdc, &rcFrame);
				}

				::SelectClipRgn(hdc, holdClip);
				::DeleteObject(hClip);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectClipRgn(hdc, holdClip);
		if (holdClip != nullptr)
		{
			::DeleteObject(holdClip);
			holdClip = nullptr;
		}
		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Window subclass procedure for owner drawn tab control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData BufferData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setTabCtrlPaintSubclass()
	 * @see umbra::removeTabCtrlPaintSubclass()
	 */
	static LRESULT CALLBACK TabPaintSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pTabBufferData = reinterpret_cast<BufferData*>(dwRefData);
		const auto& hMemDC = pTabBufferData->getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, TabPaintSubclass, uIdSubclass);
				delete pTabBufferData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				if ((nStyle & TCS_VERTICAL) == TCS_VERTICAL)
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (pTabBufferData->ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					umbra::paintTab(hWnd, hMemDC, rcClient);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_UPDATEUISTATE:
			{
				if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
				{
					::InvalidateRect(hWnd, nullptr, FALSE);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static void setTabCtrlPaintSubclass(HWND hWnd)
	{
		umbra::setSubclass<BufferData>(hWnd, TabPaintSubclass, kTabPaintSubclassID);
	}

	static void removeTabCtrlPaintSubclass(HWND hWnd)
	{
		umbra::removeSubclass<BufferData>(hWnd, TabPaintSubclass, kTabPaintSubclassID);
	}

	/**
	 * @brief Window subclass procedure for tab control's updown control subclassing.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setTabCtrlUpDownSubclass()
	 * @see umbra::removeTabCtrlUpDownSubclass()
	 */
	static LRESULT CALLBACK TabUpDownSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, TabUpDownSubclass, uIdSubclass);
				break;
			}

			case WM_PARENTNOTIFY:
			{
				if (LOWORD(wParam) == WM_CREATE)
				{
					auto hUpDown = reinterpret_cast<HWND>(lParam);
					if (CmpWndClassName(hUpDown, UPDOWN_CLASS))
					{
						umbra::setUpDownCtrlSubclass(hUpDown);
						return 0;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setTabCtrlUpDownSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, TabUpDownSubclass, kTabUpDownSubclassID);
	}

	void removeTabCtrlUpDownSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, TabUpDownSubclass, kTabUpDownSubclassID);
	}

	void setTabCtrlSubclass(HWND hWnd)
	{
		umbra::setTabCtrlPaintSubclass(hWnd);
		umbra::setTabCtrlUpDownSubclass(hWnd);
	}

	void removeTabCtrlSubclass(HWND hWnd)
	{
		umbra::removeTabCtrlPaintSubclass(hWnd);
		umbra::removeTabCtrlUpDownSubclass(hWnd);
	}

	static void setTabCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkTooltips(hWnd, ToolTipsType::tabbar);
		}

		if (p._subclass)
		{
			umbra::setTabCtrlSubclass(hWnd);
		}
	}

	struct BorderMetricsData
	{
		UINT _dpi = USER_DEFAULT_SCREEN_DPI;
		LONG _xEdge = ::GetSystemMetrics(SM_CXEDGE);
		LONG _yEdge = ::GetSystemMetrics(SM_CYEDGE);
		LONG _xScroll = ::GetSystemMetrics(SM_CXVSCROLL);
		LONG _yScroll = ::GetSystemMetrics(SM_CYVSCROLL);
		bool _isHot = false;
	};

	static void ncPaintCustomBorder(HWND hWnd, const BorderMetricsData& borderMetricsData)
	{
		HDC hdc = ::GetWindowDC(hWnd);
		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);
		rcClient.right += (2 * borderMetricsData._xEdge);

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasVerScrollbar = (nStyle & WS_VSCROLL) == WS_VSCROLL;
		if (hasVerScrollbar)
		{
			rcClient.right += borderMetricsData._xScroll;
		}

		rcClient.bottom += (2 * borderMetricsData._yEdge);

		const bool hasHorScrollbar = (nStyle & WS_HSCROLL) == WS_HSCROLL;
		if (hasHorScrollbar)
		{
			rcClient.bottom += borderMetricsData._yScroll;
		}

		HPEN hPen = ::CreatePen(PS_SOLID, 1, (::IsWindowEnabled(hWnd) == TRUE) ? umbra::getBackgroundColor() : umbra::getDlgBackgroundColor());
		RECT rcInner{ rcClient };
		::InflateRect(&rcInner, -1, -1);
		umbra::paintRoundFrameRect(hdc, rcInner, hPen);
		::DeleteObject(hPen);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE;
		const bool hasFocus = ::GetFocus() == hWnd;

		HPEN hEnabledPen = ((borderMetricsData._isHot && isHot) || hasFocus ? umbra::getHotEdgePen() : umbra::getEdgePen());

		umbra::paintRoundFrameRect(hdc, rcClient, (::IsWindowEnabled(hWnd) == TRUE) ? hEnabledPen : umbra::getDisabledEdgePen());

		::ReleaseDC(hWnd, hdc);
	}

	/**
	 * @brief Window subclass procedure for owner drawn border for list box and edit control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData BorderMetricsData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setCustomBorderForListBoxOrEditCtrlSubclass()
	 * @see umbra::removeCustomBorderForListBoxOrEditCtrlSubclass()
	 */
	static LRESULT CALLBACK CustomBorderSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pBorderMetricsData = reinterpret_cast<BorderMetricsData*>(dwRefData);

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, CustomBorderSubclass, uIdSubclass);
				delete pBorderMetricsData;
				break;
			}

			case WM_NCPAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				::DefSubclassProc(hWnd, uMsg, wParam, lParam);

				umbra::ncPaintCustomBorder(hWnd, *pBorderMetricsData);

				return 0;
			}

			case WM_NCCALCSIZE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				auto* lpRect = reinterpret_cast<LPRECT>(lParam);
				::InflateRect(lpRect, -(pBorderMetricsData->_xEdge), -(pBorderMetricsData->_yEdge));

				break;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				umbra::redrawWindowFrame(hWnd);
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				if (::GetFocus() == hWnd)
				{
					break;
				}

				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				::TrackMouseEvent(&tme);

				if (!pBorderMetricsData->_isHot)
				{
					pBorderMetricsData->_isHot = true;
					umbra::redrawWindowFrame(hWnd);
				}
				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				if (pBorderMetricsData->_isHot)
				{
					pBorderMetricsData->_isHot = false;
					umbra::redrawWindowFrame(hWnd);
				}

				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE | TME_CANCEL;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				::TrackMouseEvent(&tme);
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, kCustomBorderSubclassID);
	}

	void removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, kCustomBorderSubclassID);
	}

	static void setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p, bool isListBox)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasScrollBar = ((nStyle & WS_HSCROLL) == WS_HSCROLL) || ((nStyle & WS_VSCROLL) == WS_VSCROLL);

		// edit control without scroll bars
		if (umbra::isThemePrefered()
			&& p._theme
			&& !isListBox
			&& !hasScrollBar)
		{
			umbra::setDarkThemeExperimental(hWnd, L"CFD");
		}
		else
		{
			if (p._theme && (isListBox || hasScrollBar))
			{
				// dark scroll bars for list box or edit control
				::SetWindowTheme(hWnd, p._themeClassName, nullptr);
			}

			const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
			const bool hasClientEdge = (nExStyle & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE;
			const bool isCBoxListBox = isListBox && (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;

			if (p._subclass && hasClientEdge && !isCBoxListBox)
			{
				umbra::setCustomBorderForListBoxOrEditCtrlSubclass(hWnd);
			}

			if (::GetWindowSubclass(hWnd, CustomBorderSubclass, kCustomBorderSubclassID, nullptr) == TRUE)
			{
				const bool enableClientEdge = !umbra::isEnabled();
				umbra::setWindowExStyle(hWnd, enableClientEdge, WS_EX_CLIENTEDGE);
			}
		}
	}

	struct ComboBoxData
	{
		ThemeData _themeData{ VSCLASS_COMBOBOX };
		BufferData _bufferData;

		LONG_PTR _cbStyle = CBS_SIMPLE;

		ComboBoxData() = delete;

		explicit ComboBoxData(LONG_PTR cbStyle)
			: _cbStyle(cbStyle) {};
	};

	static void paintCombobox(HWND hWnd, HDC hdc, ComboBoxData& comboBoxData)
	{
		auto& themeData = comboBoxData._themeData;
		const auto& hTheme = themeData.getHTheme();

		const bool hasTheme = themeData.ensureTheme(hWnd);

		COMBOBOXINFO cbi{};
		cbi.cbSize = sizeof(COMBOBOXINFO);
		::GetComboBoxInfo(hWnd, &cbi);

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE && !isDisabled;

		bool hasFocus = false;

		::SelectObject(hdc, reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0)));
		::SetBkMode(hdc, TRANSPARENT); // for non-theme DrawText

		RECT rcArrow{ cbi.rcButton };
		rcArrow.left -= 1;

		auto getBrush = [&]() -> HBRUSH {
			if (isDisabled)
			{
				return umbra::getDlgBackgroundBrush();
			}

			if (isHot)
			{
				return umbra::getHotBackgroundBrush();
			}
			return umbra::getCtrlBackgroundBrush();
		};

		HBRUSH hBrush = getBrush();

		// Text part

		// CBS_DROPDOWN and CBS_SIMPLE text is handled by parent by WM_CTLCOLOREDIT
		if (comboBoxData._cbStyle == CBS_DROPDOWNLIST)
		{
			// erase background on item change
			::FillRect(hdc, &rcClient, hBrush);

			const auto index = static_cast<int>(::SendMessage(hWnd, CB_GETCURSEL, 0, 0));
			if (index != CB_ERR)
			{
				const auto bufferLen = static_cast<size_t>(::SendMessage(hWnd, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0));
				std::wstring buffer(bufferLen + 1, L'\0');
				::SendMessage(hWnd, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer.data()));

				RECT rcText{ cbi.rcItem };
				::InflateRect(&rcText, -2, 0);

				static constexpr DWORD dtFlags = DT_NOPREFIX | DT_LEFT | DT_VCENTER | DT_SINGLELINE;
				if (hasTheme)
				{
					DTTOPTS dtto{};
					dtto.dwSize = sizeof(DTTOPTS);
					dtto.dwFlags = DTT_TEXTCOLOR;
					dtto.crText = isDisabled ? umbra::getDisabledTextColor() : umbra::getTextColor();

					::DrawThemeTextEx(hTheme, hdc, CP_DROPDOWNITEM, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, buffer.c_str(), -1, dtFlags, &rcText, &dtto);
				}
				else
				{
					::SetTextColor(hdc, isDisabled ? umbra::getDisabledTextColor() : umbra::getTextColor());
					::DrawText(hdc, buffer.c_str(), -1, &rcText, dtFlags);
				}
			}

			hasFocus = ::GetFocus() == hWnd;
			if (!isDisabled && hasFocus && ::SendMessage(hWnd, CB_GETDROPPEDSTATE, 0, 0) == FALSE)
			{
				::DrawFocusRect(hdc, &cbi.rcItem);
			}
		}
		else if (cbi.hwndItem != nullptr)
		{
			hasFocus = ::GetFocus() == cbi.hwndItem;

			::FillRect(hdc, &rcArrow, hBrush);
		}

		HPEN hPen = nullptr;
		if (isDisabled)
		{
			hPen = umbra::getDisabledEdgePen();
		}
		else if ((isHot || hasFocus || comboBoxData._cbStyle == CBS_SIMPLE))
		{
			hPen = umbra::getHotEdgePen();
		}
		else
		{
			hPen = umbra::getEdgePen();
		}
		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, hPen));

		// Drop down arrow part
		if (comboBoxData._cbStyle != CBS_SIMPLE)
		{
			if (hasTheme)
			{
				const RECT rcThemedArrow{ rcArrow.left, rcArrow.top - 1, rcArrow.right, rcArrow.bottom - 1 };
				::DrawThemeBackground(hTheme, hdc, CP_DROPDOWNBUTTONRIGHT, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, &rcThemedArrow, nullptr);
			}
			else
			{
				auto getTextClr = [&]() -> COLORREF {
					if (isDisabled)
					{
						return umbra::getDisabledTextColor();
					}

					if (isHot)
					{
						return umbra::getTextColor();
					}
					return umbra::getDarkerTextColor();
				};

				::SetTextColor(hdc, getTextClr());
				::DrawText(hdc, L"˅", -1, &rcArrow, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
			}
		}

		// Frame part
		if (comboBoxData._cbStyle == CBS_DROPDOWNLIST)
		{
			::ExcludeClipRect(hdc, rcClient.left + 1, rcClient.top + 1, rcClient.right - 1, rcClient.bottom - 1);
		}
		else
		{
			::ExcludeClipRect(hdc, cbi.rcItem.left, cbi.rcItem.top, cbi.rcItem.right, cbi.rcItem.bottom);

			if (comboBoxData._cbStyle == CBS_SIMPLE && cbi.hwndList != nullptr)
			{
				RECT rcItem{ cbi.rcItem };
				::MapWindowPoints(cbi.hwndItem, hWnd, reinterpret_cast<LPPOINT>(&rcItem), 2);
				rcClient.bottom = rcItem.bottom;
			}

			RECT rcInner{ rcClient };
			::InflateRect(&rcInner, -1, -1);

			if (comboBoxData._cbStyle == CBS_DROPDOWN)
			{
				const std::array<POINT, 2> edge{ {
					{ rcArrow.left - 1, rcArrow.top },
					{ rcArrow.left - 1, rcArrow.bottom }
				} };
				::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

				::ExcludeClipRect(hdc, rcArrow.left - 1, rcArrow.top, rcArrow.right, rcArrow.bottom);

				rcInner.right = rcArrow.left - 1;
			}

			HPEN hInnerPen = ::CreatePen(PS_SOLID, 1, isDisabled ? umbra::getDlgBackgroundColor() : umbra::getBackgroundColor());
			umbra::paintRoundFrameRect(hdc, rcInner, hInnerPen);
			::DeleteObject(hInnerPen);
			::InflateRect(&rcInner, -1, -1);
			::FillRect(hdc, &rcInner, isDisabled ? umbra::getDlgBackgroundBrush() : umbra::getCtrlBackgroundBrush());
		}

		static const int roundness = umbra::isAtLeastWindows11() ? kWin11CornerRoundness : 0;
		umbra::paintRoundFrameRect(hdc, rcClient, hPen, roundness, roundness);

		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Window subclass procedure for owner drawn combo box control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData ComboBoxData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setComboBoxCtrlSubclass()
	 * @see umbra::removeComboBoxCtrlSubclass()
	 */
	static LRESULT CALLBACK ComboBoxSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pComboboxData = reinterpret_cast<ComboBoxData*>(dwRefData);
		auto& themeData = pComboboxData->_themeData;
		auto& bufferData = pComboboxData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ComboBoxSubclass, uIdSubclass);
				delete pComboboxData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (pComboboxData->_cbStyle != CBS_DROPDOWN && hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (pComboboxData->_cbStyle != CBS_DROPDOWN)
				{
					if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
					{
						::EndPaint(hWnd, &ps);
						return 0;
					}

					RECT rcClient{};
					::GetClientRect(hWnd, &rcClient);

					if (bufferData.ensureBuffer(hdc, rcClient))
					{
						const int savedState = ::SaveDC(hMemDC);
						::IntersectClipRect(
							hMemDC,
							ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
						);

						umbra::paintCombobox(hWnd, hMemDC, *pComboboxData);

						::RestoreDC(hMemDC, savedState);

						::BitBlt(
							hdc,
							ps.rcPaint.left, ps.rcPaint.top,
							ps.rcPaint.right - ps.rcPaint.left,
							ps.rcPaint.bottom - ps.rcPaint.top,
							hMemDC,
							ps.rcPaint.left, ps.rcPaint.top,
							SRCCOPY
						);
					}
				}
				else
				{
					umbra::paintCombobox(hWnd, hdc, *pComboboxData);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_ENABLE:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				return retVal;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxCtrlSubclass(HWND hWnd)
	{
		const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
		umbra::setSubclass<ComboBoxData>(hWnd, ComboBoxSubclass, kComboBoxSubclassID, cbStyle);
	}

	void removeComboBoxCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<ComboBoxData>(hWnd, ComboBoxSubclass, kComboBoxSubclassID);
	}

	static void setComboBoxCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
		const bool isCbList = cbStyle == CBS_DROPDOWNLIST;
		const bool isCbSimple = cbStyle == CBS_SIMPLE;

		if (isCbList
			|| cbStyle == CBS_DROPDOWN
			|| isCbSimple)
		{
			COMBOBOXINFO cbi{};
			cbi.cbSize = sizeof(COMBOBOXINFO);
			if (::GetComboBoxInfo(hWnd, &cbi) == TRUE)
			{
				if (p._theme && cbi.hwndList != nullptr)
				{
					if (isCbSimple)
					{
						umbra::replaceClientEdgeWithBorderSafe(cbi.hwndList);
					}

					// dark scroll bar for list box of combo box
					::SetWindowTheme(cbi.hwndList, p._themeClassName, nullptr);
				}
			}

			if (!umbra::isThemePrefered() && p._subclass)
			{
				HWND hParent = ::GetParent(hWnd);
				if ((hParent == nullptr || GetWndClassName(hParent) != WC_COMBOBOXEX))
				{
					umbra::setComboBoxCtrlSubclass(hWnd);
				}
			}

			if (p._theme) // for light dropdown arrow in dark mode
			{
				umbra::setDarkThemeExperimental(hWnd, L"CFD");

				if (!isCbList)
				{
					::SendMessage(hWnd, CB_SETEDITSEL, 0, 0); // clear selection
				}
			}
		}
	}

	/**
	 * @brief Window subclass procedure for custom color for combo box ex' list box and edit control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setComboBoxExCtrlSubclass()
	 * @see umbra::removeComboBoxExCtrlSubclass()
	 */
	static LRESULT CALLBACK ComboboxExSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ComboboxExSubclass, uIdSubclass);
				umbra::unhookSysColor();
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, umbra::getDlgBackgroundBrush());
				return TRUE;
			}

			case WM_CTLCOLOREDIT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORLISTBOX:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorListbox(wParam, lParam);
			}

			case WM_COMMAND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				// ComboboxEx has only one child combo box, so only control-defined notification code is checked.
				// Hooking is done only when list box is about to show. And unhook when list box is closed.
				// This process is used to avoid visual glitches in other GUI.
				switch (HIWORD(wParam))
				{
					case CBN_DROPDOWN:
					{
						umbra::hookSysColor();
						break;
					}

					case CBN_CLOSEUP:
					{
						umbra::unhookSysColor();
						break;
					}

					default:
					{
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxExCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, ComboboxExSubclass, kComboBoxExSubclassID);
	}

	void removeComboBoxExCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, ComboboxExSubclass, kComboBoxExSubclassID);
		umbra::unhookSysColor();
	}

	static void setComboBoxExCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			umbra::setComboBoxExCtrlSubclass(hWnd);
		}
	}

	/**
	 * @brief Window subclass procedure for custom color for list view's gridlines and edit control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setListViewCtrlSubclass()
	 * @see umbra::removeListViewCtrlSubclass()
	 */
	static LRESULT CALLBACK ListViewSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ListViewSubclass, uIdSubclass);
				umbra::unhookSysColor();
				break;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const auto lvStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & LVS_TYPEMASK;
				const bool isReport = (lvStyle == LVS_REPORT);
				bool hasGridlines = false;
				if (isReport)
				{
					const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
					hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
				}

				if (hasGridlines)
				{
					umbra::hookSysColor();
					const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
					umbra::unhookSysColor();
					return retVal;
				}
				break;
			}

			// For edit control, which is created when renaming/editing items
			case WM_CTLCOLOREDIT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_NOTIFY:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW)
				{
					auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
					switch (lpnmcd->dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							if (umbra::isExperimentalActive())
							{
								return CDRF_NOTIFYITEMDRAW;
							}
							return CDRF_DODEFAULT;
						}

						case CDDS_ITEMPREPAINT:
						{
							::SetTextColor(lpnmcd->hdc, umbra::getDarkerTextColor());

							return CDRF_NEWFONT;
						}

						default:
						{
							return CDRF_DODEFAULT;
						}
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setListViewCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, ListViewSubclass, kListViewSubclassID);
	}

	void removeListViewCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, ListViewSubclass, kListViewSubclassID);
	}

	static void setListViewCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		HWND hHeader = ListView_GetHeader(hWnd);

		if (p._theme)
		{
			ListView_SetTextColor(hWnd, umbra::getViewTextColor());
			ListView_SetTextBkColor(hWnd, umbra::getViewBackgroundColor());
			ListView_SetBkColor(hWnd, umbra::getViewBackgroundColor());

			umbra::setDarkListView(hWnd);
			umbra::setDarkListViewCheckboxes(hWnd);
			umbra::setDarkTooltips(hWnd, umbra::ToolTipsType::listview);

			if (umbra::isThemePrefered())
			{
				umbra::setDarkThemeExperimental(hHeader, L"ItemsView");
			}
		}

		if (p._subclass)
		{
			if (!umbra::isThemePrefered())
			{
				umbra::setHeaderCtrlSubclass(hHeader);
			}

			const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
			ListView_SetExtendedListViewStyle(hWnd, lvExStyle | LVS_EX_DOUBLEBUFFER);
			umbra::setListViewCtrlSubclass(hWnd);
		}
	}

	struct HeaderData
	{
		ThemeData _themeData{ VSCLASS_HEADER };
		BufferData _bufferData;
		FontData _fontData{ nullptr };

		POINT _pt{ LONG_MIN, LONG_MIN };
		bool _isHot = false;
		bool _hasBtnStyle = true;
		bool _isPressed = false;

		HeaderData() = delete;

		explicit HeaderData(bool hasBtnStyle)
			: _hasBtnStyle(hasBtnStyle)
		{}
	};

	static void paintHeader(HWND hWnd, HDC hdc, HeaderData& headerData)
	{
		auto& themeData = headerData._themeData;
		const auto& hTheme = themeData.getHTheme();
		const bool hasTheme = themeData.ensureTheme(hWnd);
		auto& fontData = headerData._fontData;

		::SetBkMode(hdc, TRANSPARENT);
		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, umbra::getHeaderEdgePen()));

		RECT rcHeader{};
		::GetClientRect(hWnd, &rcHeader);
		::FillRect(hdc, &rcHeader, umbra::getHeaderBackgroundBrush());

		LOGFONT lf{};
		if (!fontData.hasFont()
			&& hasTheme
			&& SUCCEEDED(::GetThemeFont(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, TMT_FONT, &lf)))
		{
			fontData.setFont(::CreateFontIndirect(&lf));
		}

		HFONT hFont = (fontData.hasFont()) ? fontData.getFont() : reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		DTTOPTS dtto{};
		if (hasTheme)
		{
			dtto.dwSize = sizeof(DTTOPTS);
			dtto.dwFlags = DTT_TEXTCOLOR;
			dtto.crText = umbra::getHeaderTextColor();
		}
		else
		{
			::SetTextColor(hdc, umbra::getHeaderTextColor());
		}

		HWND hList = ::GetParent(hWnd);
		const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
		bool hasGridlines = false;
		if (lvStyle == LVS_REPORT)
		{
			const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
			hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
		}

		const int count = Header_GetItemCount(hWnd);
		RECT rcItem{};
		for (int i = 0; i < count; i++)
		{
			Header_GetItemRect(hWnd, i, &rcItem);
			const bool isOnItem = ::PtInRect(&rcItem, headerData._pt) == TRUE;

			if (headerData._hasBtnStyle && isOnItem)
			{
				RECT rcTmp{ rcItem };
				if (hasGridlines)
				{
					::OffsetRect(&rcTmp, 1, 0);
				}
				else if (umbra::isExperimentalActive())
				{
					::OffsetRect(&rcTmp, -1, 0);
				}
				::FillRect(hdc, &rcTmp, umbra::getHeaderHotBackgroundBrush());
			}

			std::wstring buffer(MAX_PATH, L'\0');
			HDITEM hdi{};
			hdi.mask = HDI_TEXT | HDI_FORMAT;
			hdi.pszText = buffer.data();
			hdi.cchTextMax = MAX_PATH - 1;

			Header_GetItem(hWnd, i, &hdi);

			if (hasTheme
				&& ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP
					|| (hdi.fmt & HDF_SORTDOWN) == HDF_SORTDOWN))
			{
				const int iStateID = ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP) ? HSAS_SORTEDUP : HSAS_SORTEDDOWN;
				RECT rcArrow{ rcItem };
				SIZE szArrow{};
				if (SUCCEEDED(::GetThemePartSize(hTheme, hdc, HP_HEADERSORTARROW, iStateID, nullptr, TS_DRAW, &szArrow)))
				{
					rcArrow.bottom = szArrow.cy;
				}

				::DrawThemeBackground(hTheme, hdc, HP_HEADERSORTARROW, iStateID, &rcArrow, nullptr);
			}

			LONG edgeX = rcItem.right;
			if (!hasGridlines)
			{
				--edgeX;
				if (umbra::isExperimentalActive())
				{
					--edgeX;
				}
			}

			const std::array<POINT, 2> edge{ {
				{ edgeX, rcItem.top },
				{ edgeX, rcItem.bottom }
			} };
			::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

			DWORD dtFlags = DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_HIDEPREFIX;
			if ((hdi.fmt & HDF_RIGHT) == HDF_RIGHT)
			{
				dtFlags |= DT_RIGHT;
			}
			else if ((hdi.fmt & HDF_CENTER) == HDF_CENTER)
			{
				dtFlags |= DT_CENTER;
			}

			static constexpr LONG lOffset = 6;
			static constexpr LONG rOffset = 8;

			rcItem.left += lOffset;
			rcItem.right -= rOffset;

			if (headerData._isPressed && isOnItem)
			{
				::OffsetRect(&rcItem, 1, 1);
			}

			if (hasTheme)
			{
				::DrawThemeTextEx(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, hdi.pszText, -1, dtFlags, &rcItem, &dtto);
			}
			else
			{
				::DrawText(hdc, hdi.pszText, -1, &rcItem, dtFlags);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Window subclass procedure for owner drawn header control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData HeaderData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setHeaderCtrlSubclass()
	 * @see umbra::removeHeaderCtrlSubclass()
	 */
	static LRESULT CALLBACK HeaderSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pHeaderData = reinterpret_cast<HeaderData*>(dwRefData);
		auto& themeData = pHeaderData->_themeData;
		auto& bufferData = pHeaderData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, HeaderSubclass, uIdSubclass);
				delete pHeaderData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					umbra::paintHeader(hWnd, hMemDC, *pHeaderData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_LBUTTONDOWN:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				pHeaderData->_isPressed = true;
				break;
			}

			case WM_LBUTTONUP:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				pHeaderData->_isPressed = false;
				break;
			}

			case WM_MOUSEMOVE:
			{
				if (!pHeaderData->_hasBtnStyle || pHeaderData->_isPressed)
				{
					break;
				}

				TRACKMOUSEEVENT tme{};

				if (!pHeaderData->_isHot)
				{
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_LEAVE;
					tme.hwndTrack = hWnd;

					::TrackMouseEvent(&tme);

					pHeaderData->_isHot = true;
				}

				pHeaderData->_pt.x = GET_X_LPARAM(lParam);
				pHeaderData->_pt.y = GET_Y_LPARAM(lParam);

				::InvalidateRect(hWnd, nullptr, FALSE);
				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

				pHeaderData->_isHot = false;
				pHeaderData->_pt.x = LONG_MIN;
				pHeaderData->_pt.y = LONG_MIN;

				::InvalidateRect(hWnd, nullptr, TRUE);

				return retVal;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setHeaderCtrlSubclass(HWND hWnd)
	{
		const bool hasBtnStyle = (::GetWindowLongPtr(hWnd, GWL_STYLE) & HDS_BUTTONS) == HDS_BUTTONS;
		umbra::setSubclass<HeaderData>(hWnd, HeaderSubclass, kHeaderSubclassID, hasBtnStyle);
	}

	void removeHeaderCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<HeaderData>(hWnd, HeaderSubclass, kHeaderSubclassID);
	}

	struct StatusBarData
	{
		ThemeData _themeData{ VSCLASS_STATUS };
		BufferData _bufferData;
		FontData _fontData;

		StatusBarData() = delete;

		explicit StatusBarData(const HFONT& hFont)
			: _fontData(hFont)
		{}
	};

	static void paintStatusBar(HWND hWnd, HDC hdc, StatusBarData& statusBarData)
	{
		const auto& hFont = statusBarData._fontData.getFont();

		struct {
			int horizontal = 0;
			int vertical = 0;
			int between = 0;
		} borders{};

		::SendMessage(hWnd, SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(&borders));

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasSizeGrip = (nStyle & SBARS_SIZEGRIP) == SBARS_SIZEGRIP;

		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, umbra::getEdgePen()));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		::SetBkMode(hdc, TRANSPARENT);
		::SetTextColor(hdc, umbra::getTextColor());

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		::FillRect(hdc, &rcClient, umbra::getBackgroundBrush());

		const auto nParts = static_cast<int>(::SendMessage(hWnd, SB_GETPARTS, 0, 0));
		std::wstring str;
		RECT rcPart{};
		RECT rcIntersect{};
		const int iLastDiv = nParts - (hasSizeGrip ? 1 : 0);
		const bool drawEdge = (nParts >= 2 || !hasSizeGrip);
		for (int i = 0; i < nParts; ++i)
		{
			::SendMessage(hWnd, SB_GETRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rcPart));
			if (::IntersectRect(&rcIntersect, &rcPart, &rcClient) == FALSE)
			{
				continue;
			}

			if (drawEdge && (i < iLastDiv))
			{
				const std::array<POINT, 2> edges{ {
					{ rcPart.right - borders.between, rcPart.top + 1 },
					{ rcPart.right - borders.between, rcPart.bottom - 3 }
				} };
				::Polyline(hdc, edges.data(), static_cast<int>(edges.size()));
			}

			rcPart.left += borders.between;
			rcPart.right -= borders.vertical;

			const LRESULT retValLen = ::SendMessage(hWnd, SB_GETTEXTLENGTH, static_cast<WPARAM>(i), 0);
			const DWORD cchText = LOWORD(retValLen);

			str.resize(static_cast<size_t>(cchText) + 1);
			const LRESULT retValText = ::SendMessage(hWnd, SB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(str.data()));

			if (cchText == 0 && (HIWORD(retValLen) & SBT_OWNERDRAW) != 0)
			{
				const auto id = static_cast<UINT>(::GetDlgCtrlID(hWnd));
				DRAWITEMSTRUCT dis{
					0
					, 0
					, static_cast<UINT>(i)
					, ODA_DRAWENTIRE
					, id
					, hWnd
					, hdc
					, rcPart
					, static_cast<ULONG_PTR>(retValText)
				};

				::SendMessage(::GetParent(hWnd), WM_DRAWITEM, id, reinterpret_cast<LPARAM>(&dis));
			}
			else
			{
				::DrawText(hdc, str.c_str(), -1, &rcPart, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
			}
		}

#if 0 // for horizontal edge
		POINT edgeHor[]{
			{rcClient.left, rcClient.top},
			{rcClient.right, rcClient.top}
		};
		Polyline(hdc, edgeHor, _countof(edgeHor));
#endif

		if (hasSizeGrip)
		{
			auto& themeData = statusBarData._themeData;
			const auto& hTheme = themeData.getHTheme();
			const bool hasTheme = themeData.ensureTheme(hWnd);
			if (hasTheme)
			{
				SIZE szGrip{};
				::GetThemePartSize(hTheme, hdc, SP_GRIPPER, 0, &rcClient, TS_DRAW, &szGrip);
				RECT rcGrip{ rcClient };
				rcGrip.left = rcGrip.right - szGrip.cx;
				rcGrip.top = rcGrip.bottom - szGrip.cy;
				::DrawThemeBackground(hTheme, hdc, SP_GRIPPER, 0, &rcGrip, nullptr);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Window subclass procedure for owner drawn status bar control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData StatusBarData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setStatusBarCtrlSubclass()
	 * @see umbra::removeStatusBarCtrlSubclass()
	 */
	static LRESULT CALLBACK StatusBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData)
	{
		auto* pStatusBarData = reinterpret_cast<StatusBarData*>(dwRefData);
		auto& themeData = pStatusBarData->_themeData;
		auto& bufferData = pStatusBarData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, StatusBarSubclass, uIdSubclass);
				delete pStatusBarData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					umbra::paintStatusBar(hWnd, hMemDC, *pStatusBarData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			case WM_THEMECHANGED:
			{
				themeData.closeTheme();

				LOGFONT lf{};
				NONCLIENTMETRICS ncm{};
				ncm.cbSize = sizeof(NONCLIENTMETRICS);
				if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0) != FALSE)
				{
					lf = ncm.lfStatusFont;
					pStatusBarData->_fontData.setFont(::CreateFontIndirect(&lf));
				}

				if (uMsg != WM_THEMECHANGED)
				{
					return 0;
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setStatusBarCtrlSubclass(HWND hWnd)
	{
		LOGFONT lf{};
		NONCLIENTMETRICS ncm{};
		ncm.cbSize = sizeof(NONCLIENTMETRICS);
		if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0) != FALSE)
		{
			lf = ncm.lfStatusFont;
		}
		umbra::setSubclass<StatusBarData>(hWnd, StatusBarSubclass, kStatusBarSubclassID, ::CreateFontIndirect(&lf));
	}

	void removeStatusBarCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<StatusBarData>(hWnd, StatusBarSubclass, kStatusBarSubclassID);
	}

	static void setStatusBarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			umbra::setStatusBarCtrlSubclass(hWnd);
		}
	}

	struct ProgressBarData
	{
		ThemeData _themeData{ VSCLASS_PROGRESS };
		BufferData _bufferData;

		int _iStateID = PBFS_PARTIAL;

		explicit ProgressBarData(HWND hWnd)
			: _iStateID(static_cast<int>(::SendMessage(hWnd, PBM_GETSTATE, 0, 0)))
		{}
	};

	static void getProgressBarRects(HWND hWnd, RECT* rcEmpty, RECT* rcFilled)
	{
		const auto pos = static_cast<int>(::SendMessage(hWnd, PBM_GETPOS, 0, 0));

		PBRANGE range{};
		::SendMessage(hWnd, PBM_GETRANGE, TRUE, reinterpret_cast<LPARAM>(&range));
		const int iMin = range.iLow;

		const int currPos = pos - iMin;
		if (currPos != 0)
		{
			const int totalWidth = rcEmpty->right - rcEmpty->left;
			rcFilled->left = rcEmpty->left;
			rcFilled->top = rcEmpty->top;
			rcFilled->bottom = rcEmpty->bottom;
			rcFilled->right = rcEmpty->left + static_cast<int>(static_cast<double>(currPos) / (range.iHigh - iMin) * totalWidth);

			rcEmpty->left = rcFilled->right; // to avoid painting under filled part
		}
	}

	static void paintProgressBar(HWND hWnd, HDC hdc, const ProgressBarData& progressBarData)
	{
		const auto& hTheme = progressBarData._themeData.getHTheme();

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		umbra::paintRoundFrameRect(hdc, rcClient, umbra::getEdgePen(), 0, 0);

		::InflateRect(&rcClient, -1, -1);
		rcClient.left = 1;

		RECT rcFill{};
		umbra::getProgressBarRects(hWnd, &rcClient, &rcFill);
		::DrawThemeBackground(hTheme, hdc, PP_FILL, progressBarData._iStateID, &rcFill, nullptr);
		::FillRect(hdc, &rcClient, umbra::getCtrlBackgroundBrush());
	}

	/**
	 * @brief Window subclass procedure for owner drawn progress bar control.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData ProgressBarData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setProgressBarCtrlSubclass()
	 * @see umbra::removeProgressBarCtrlSubclass()
	 */
	static LRESULT CALLBACK ProgressBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pProgressBarData = reinterpret_cast<ProgressBarData*>(dwRefData);
		auto& themeData = pProgressBarData->_themeData;
		auto& bufferData = pProgressBarData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ProgressBarSubclass, uIdSubclass);
				delete pProgressBarData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					umbra::paintProgressBar(hWnd, hMemDC, *pProgressBarData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case PBM_SETSTATE:
			{
				switch (wParam)
				{
					case PBST_NORMAL:
					{
						pProgressBarData->_iStateID = PBFS_NORMAL; // green
						break;
					}

					case PBST_ERROR:
					{
						pProgressBarData->_iStateID = PBFS_ERROR; // red
						break;
					}

					case PBST_PAUSED:
					{
						pProgressBarData->_iStateID = PBFS_PAUSED; // yellow
						break;
					}

					default:
					{
						pProgressBarData->_iStateID = PBFS_PARTIAL; // cyan
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setProgressBarCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, kProgressBarSubclassID, hWnd);
	}

	void removeProgressBarCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, kProgressBarSubclassID);
	}

	static void setProgressBarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		if (p._theme && (nStyle & PBS_MARQUEE) == PBS_MARQUEE)
		{
			umbra::setProgressBarClassicTheme(hWnd);
		}
		else if (p._subclass)
		{
			umbra::setProgressBarCtrlSubclass(hWnd);
		}
	}

	struct StaticTextData
	{
		bool _isEnabled = true;

		StaticTextData() = default;

		explicit StaticTextData(HWND hWnd)
			: _isEnabled(::IsWindowEnabled(hWnd) == TRUE)
		{}
	};

	/**
	 * @brief Window subclass procedure for better disabled state appearence for static control with text.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData StaticTextData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setStaticTextCtrlSubclass()
	 * @see umbra::removeStaticTextCtrlSubclass()
	 */
	static LRESULT CALLBACK StaticTextSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pStaticTextData = reinterpret_cast<StaticTextData*>(dwRefData);

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, StaticTextSubclass, uIdSubclass);
				delete pStaticTextData;
				break;
			}

			case WM_ENABLE:
			{
				pStaticTextData->_isEnabled = (wParam == TRUE);

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				if (!pStaticTextData->_isEnabled)
				{
					::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle & ~WS_DISABLED);
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::MapWindowPoints(hWnd, ::GetParent(hWnd), reinterpret_cast<LPPOINT>(&rcClient), 2);
				::RedrawWindow(::GetParent(hWnd), &rcClient, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

				if (!pStaticTextData->_isEnabled)
				{
					::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle | WS_DISABLED);
				}

				return 0;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setStaticTextCtrlSubclass(HWND hWnd)
	{
		umbra::setSubclass<StaticTextData>(hWnd, StaticTextSubclass, kStaticTextSubclassID, hWnd);
	}

	void removeStaticTextCtrlSubclass(HWND hWnd)
	{
		umbra::removeSubclass<StaticTextData>(hWnd, StaticTextSubclass, kStaticTextSubclassID);
	}

	static void setStaticTextCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			umbra::setStaticTextCtrlSubclass(hWnd);
		}
	}

	static void setTreeViewCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			TreeView_SetTextColor(hWnd, umbra::getViewTextColor());
			TreeView_SetBkColor(hWnd, umbra::getViewBackgroundColor());

			umbra::setTreeViewWindowTheme(hWnd, p._theme);
			umbra::setDarkTooltips(hWnd, umbra::ToolTipsType::treeview);
		}
	}

	static void setRebarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			umbra::setWindowEraseBgSubclass(hWnd);
		}
	}

	static void setToolbarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkLineAbovePanelToolbar(hWnd);
			umbra::setDarkTooltips(hWnd, umbra::ToolTipsType::toolbar);
		}
	}

	static void setScrollBarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkScrollBar(hWnd);
		}
	}

	static void enableSysLinkCtrlCtlColor(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::enableSysLinkCtrlCtlColor(hWnd);
		}
	}

	static void setRichEditCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkRichEdit(hWnd);
		}
	}

	static void setTrackbarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkTooltips(hWnd, ToolTipsType::trackbar);
			umbra::setWindowStyle(hWnd, umbra::isEnabled(), TBS_TRANSPARENTBKGND);
		}
	}

	// --- aclui CHECKLIST_ACLUI: owner-drawn Allow/Deny permissions list -------
	// Reached by the child-walk but it paints its own body, and both the body and
	// its label statics derive from COLOR_WINDOW / dialog faces that user32 resolves
	// from the internal sys-color table — the process GetSysColor hook can't reach
	// them, so they stay light. We darken the WHOLE field to the COLOR_WINDOW dark-
	// equivalent (ctrlBackground): the body via WM_ERASEBKGND, and the child statics
	// via WM_CTLCOLORSTATIC. Without the latter, aclui colours the statics dialog-
	// dark (0x202020) and they read as black patches on the grey body.
	static LRESULT CALLBACK AcluiCheckListSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, AcluiCheckListSubclass, uIdSubclass);
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, umbra::getCtrlBackgroundBrush());
				return TRUE;
			}

			case WM_CTLCOLORSTATIC:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				const bool isChildEnabled = ::IsWindowEnabled(reinterpret_cast<HWND>(lParam)) == TRUE;
				auto hdc = reinterpret_cast<HDC>(wParam);
				::SetTextColor(hdc, isChildEnabled ? umbra::getTextColor() : umbra::getDisabledTextColor());
				::SetBkColor(hdc, umbra::getCtrlBackgroundColor());
				return reinterpret_cast<LRESULT>(umbra::getCtrlBackgroundBrush());
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	// Best-effort dark for CHECKLIST_ACLUI: DarkMode_Explorer (dark scroll bar and
	// dark-aware bits) + the field subclass above (body and label statics → ctrlBackground).
	static void setAcluiCheckListSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			umbra::setDarkThemeExperimental(hWnd, L"DarkMode_Explorer");
		}
		if (p._subclass)
		{
			umbra::setSubclass(hWnd, AcluiCheckListSubclass, kAcluiCheckListSubclassID);
		}
	}

	static BOOL CALLBACK DarkEnumChildProc(HWND hWnd, LPARAM lParam)
	{
		const auto& p = *reinterpret_cast<DarkModeParams*>(lParam);
		const std::wstring className = GetWndClassName(hWnd);

		if (className == WC_BUTTON)
		{
			umbra::setBtnCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_STATIC)
		{
			umbra::setStaticTextCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_COMBOBOX)
		{
			umbra::setComboBoxCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_EDIT)
		{
			umbra::setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, false);
			return TRUE;
		}

		if (className == WC_LISTBOX)
		{
			umbra::setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, true);
			return TRUE;
		}

		if (className == WC_LISTVIEW)
		{
			umbra::setListViewCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_TREEVIEW)
		{
			umbra::setTreeViewCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == REBARCLASSNAMEW)
		{
			umbra::setRebarCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == TOOLBARCLASSNAME)
		{
			umbra::setToolbarCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == UPDOWN_CLASS)
		{
			umbra::setUpDownCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_TABCONTROL)
		{
			umbra::setTabCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == STATUSCLASSNAME)
		{
			umbra::setStatusBarCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_SCROLLBAR)
		{
			umbra::setScrollBarCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_COMBOBOXEX)
		{
			umbra::setComboBoxExCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == PROGRESS_CLASS)
		{
			umbra::setProgressBarCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_LINK)
		{
			umbra::enableSysLinkCtrlCtlColor(hWnd, p);
			return TRUE;
		}

		if (className == RICHEDIT_CLASS || className == MSFTEDIT_CLASS)
		{
			umbra::setRichEditCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == TRACKBAR_CLASS)
		{
			umbra::setTrackbarCtrlTheme(hWnd, p);
			return TRUE;
		}

		// aclui's owner-drawn Allow/Deny permissions list (the Security dialog).
		if (className == L"CHECKLIST_ACLUI")
		{
			umbra::setAcluiCheckListSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		// File-dialog / Explorer address & search band custom windows. These paint
		// their own COLOR_WINDOW (white) background — no uxtheme or GetSysColor hook
		// sees it — but they ARE real windows the auto-dark hook reaches on WM_CREATE.
		// setDarkWndNotifySafe gives them ctl-color + child theming but no erase fill,
		// so add a dark WM_ERASEBKGND fill (as for CHECKLIST_ACLUI). "Address Band Root"
		// is the sliver just left of the breadcrumb.
		if (className == L"_SearchEditBoxFakeWindow"
			|| className == L"Search Box"
			|| className == L"SearchEditBoxWrapperClass"
			|| className == L"Address Band Root")
		{
			if (p._subclass)
			{
				umbra::setWindowEraseBgSubclass(hWnd);
			}
			return TRUE;
		}

#if 0 // for debugging
		if (className == L"#32770") // dialog
		{
			return TRUE;
		}
#endif

		return TRUE;
	}

	void setChildCtrlsSubclassAndTheme(HWND hParent, bool subclass, bool theme)
	{
		DarkModeParams p{
			umbra::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr
			, subclass
			, theme
		};

		::EnumChildWindows(hParent, umbra::DarkEnumChildProc, reinterpret_cast<LPARAM>(&p));
	}

	// Applies the class-specific subclass/theme that DarkEnumChildProc gives a
	// child — but to hWnd ITSELF. Lets a per-window auto-theming hook (e.g. the
	// umbra-hook harness) class-theme each control as it is created, reusing the
	// same dispatch the tree walk uses rather than duplicating the big class switch.
	void setDarkChildCtrl(HWND hWnd)
	{
		if (hWnd == nullptr)
			return;

		DarkModeParams p{
			umbra::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr
			, true   // subclass
			, true   // theme
		};

		umbra::DarkEnumChildProc(hWnd, reinterpret_cast<LPARAM>(&p));
	}

	// The hook-free "theme this freshly-created window" decision a per-window
	// auto-theming hook applies on WM_CREATE: window-level canvas/ctl-color, the
	// class-specific child subclass, and a menu-bar subclass when the window owns a
	// menu. The interception that drives it (a WH_CALLWNDPROCRET hook + CreateThread
	// detour) is an application concern and lives outside the library.
	void applyDarkToNewWindow(HWND hWnd)
	{
		if (hWnd == nullptr)
			return;

		umbra::setDarkWndNotifySafe(hWnd);
		umbra::setDarkChildCtrl(hWnd);
		if (::GetMenu(hWnd) != nullptr)
			umbra::setWindowMenuBarSubclass(hWnd);

		// Flash prevention: a top-level window erases its client with the (light)
		// class brush on first show, before its content paints — the white "flashbang".
		// Installing a dark WM_ERASEBKGND subclass here, at WM_CREATE (the first chance
		// there is), makes that very first erase dark, so the compositor never has a
		// light frame to show. Top-level only — child controls keep their per-class
		// background shade.
		if ((::GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) == 0)
			umbra::setWindowEraseBgSubclass(hWnd);
	}

	// Early dark-mode prep for a WH_CALLWNDPROC hook (runs on WM_NCCREATE, before the
	// window proc and before the window opens its theme). Only allows dark mode — NO
	// SetWindowTheme, which breaks shell items-view selection rendering — so DUI/uxtheme
	// can resolve the dark theme variant. The full theming still runs from the
	// WH_CALLWNDPROCRET pass (applyDarkToNewWindow).
	void prepDarkModeForNewWindow(HWND hWnd)
	{
		if (hWnd == nullptr)
			return;

		umbra::allowDarkModeForWindow(hWnd, umbra::isExperimentalActive());
	}

	// Maps a Win32 system-color index to UMBRA's dark palette. The pure-data half
	// of the old process-wide GetSysColor/GetSysColorBrush hook: an application can
	// inline-hook those user32 exports and consult this for the colour to serve,
	// keeping the palette knowledge in the library and the interception (Detours)
	// in the app. Returns false for indices UMBRA does not override (the caller
	// then uses the real system color). Mirrors how UMBRA seeds its palette from
	// sys colors.
	bool darkSysColor(int nIndex, COLORREF& outColor) noexcept
	{
		switch (nIndex)
		{
		// --- backgrounds ---
		case COLOR_WINDOW:                outColor = umbra::getCtrlBackgroundColor(); return true;
		case COLOR_BTNFACE:               // == COLOR_3DFACE
		case COLOR_3DLIGHT:
		case COLOR_ACTIVECAPTION:
		case COLOR_INACTIVECAPTION:
		case COLOR_MENU:
		case COLOR_MENUBAR:
		case COLOR_SCROLLBAR:
		case COLOR_BACKGROUND:            // == COLOR_DESKTOP
		case COLOR_APPWORKSPACE:
		case COLOR_INFOBK:                outColor = umbra::getBackgroundColor(); return true;

		// --- text ---
		case COLOR_WINDOWTEXT:
		case COLOR_BTNTEXT:
		case COLOR_MENUTEXT:
		case COLOR_CAPTIONTEXT:
		case COLOR_INFOTEXT:
		case COLOR_HIGHLIGHTTEXT:         outColor = umbra::getTextColor(); return true;
		case COLOR_GRAYTEXT:
		case COLOR_INACTIVECAPTIONTEXT:   outColor = umbra::getDisabledTextColor(); return true;
		case COLOR_HOTLIGHT:              outColor = umbra::getLinkTextColor(); return true;

		// --- selection / highlight ---
		case COLOR_HIGHLIGHT:
		case COLOR_MENUHILIGHT:           outColor = umbra::getHotBackgroundColor(); return true;

		// --- edges / 3D ---
		case COLOR_WINDOWFRAME:
		case COLOR_3DDKSHADOW:
		case COLOR_3DSHADOW:              // == COLOR_BTNSHADOW
		case COLOR_ACTIVEBORDER:
		case COLOR_INACTIVEBORDER:        outColor = umbra::getEdgeColor(); return true;
		case COLOR_3DHIGHLIGHT:           outColor = umbra::getHotEdgeColor(); return true; // == BTNHIGHLIGHT/3DHILIGHT

		default:                          return false;
		}
	}

	// Counterpart to darkSysColor for uxtheme background drawing: decides whether to
	// replace a DrawThemeBackground[Ex] themed part with a flat dark fill, for DUI
	// bands uxtheme paints light (no dark variant). Targeted by class — only pure
	// background bands whose foreground text is already light, so a flat fill needs
	// no companion text override. Expanded from the DrawThemeBackground parts the
	// umbra-hook harness logs.
	bool darkThemeBackground(const wchar_t* classList, int partId,
	                         [[maybe_unused]] int stateId, COLORREF& outFill) noexcept
	{
		if (classList == nullptr)
			return false;

		const std::wstring_view cls(classList);
		const COLORREF dark = umbra::getBackgroundColor();

		// Explorer / file-dialog top strip (the address + search band). The harness's
		// DrawThemeBackground log shows it is a composite painted from several light
		// theme parts with no dark variant: the rebar band itself, the address box,
		// and the search box. Flat-fill each so the whole strip reads dark; their text
		// is drawn light, so no companion text override is needed. We deliberately
		// skip the glyph parts (SearchBox part 3) and the nav buttons (Navigation).
		//   Rebar: part 3 = RP_BAND, part 6 = RP_BACKGROUND.
		if (cls == L"Rebar" && (partId == 3 || partId == 6))         { outFill = dark; return true; }
		if (cls == L"AddressBand" && partId == 1)                    { outFill = dark; return true; }
		if (cls == L"SearchBoxComposited::SearchBox" && partId == 1) { outFill = dark; return true; }

		// Color-queried but never drawn in testing; harmless to keep as a known band.
		if (cls == L"Communications::Rebar")                         { outFill = dark; return true; }

		// Shell-created tab control / listview header (SysTabControl32 / SysHeader32):
		// real common controls, but umbra's custom-draw doesn't take here, so they fall
		// through to uxtheme light. Our own tabs/headers custom-draw and never reach
		// DrawThemeBackground, so darkening these classes catches only the shell ones.
		if (cls == L"Tab" && partId == 1)                            { outFill = dark; return true; }
		if (cls == L"Header" && (partId == 0 || partId == 1 || partId == 2))
			{ outFill = dark; return true; }

		return false;
	}

	// Counterpart to darkThemeBackground for uxtheme TEXT colour — a targeted patch for
	// theme "holes". When GetThemeColor FAILS (the theme defines no colour for that
	// part/state/prop), the caller falls back to a colour our GetSysColor export hook
	// can't reach: e.g. comctl's themed list-item text, which then draws dark-on-dark.
	// The breadcrumb ListviewPopup is the case in hand — ItemsView::ListView's LVP_LISTITEM
	// (part 1) base/normal state (0) TMT_TEXTCOLOR (3803) returns ELEMENT NOT FOUND, so its
	// unselected rows take that fallback while the SELECTED state (which the theme DOES
	// define) stays correct. Override ONLY the failing query, never a colour the theme
	// actually provides — so this fills the hole without disturbing working text. Not gated
	// by class: supplying the palette's view-text colour wherever a list item has no themed
	// text colour is correct in any mode (getViewTextColor tracks the light/dark palette).
	bool darkThemeColor(const wchar_t* /*classList*/, int partId, int stateId, int propId,
	                    HRESULT inHr, COLORREF& outColor) noexcept
	{
		if (SUCCEEDED(inHr))
			return false;

		// part 1 = LVP_LISTITEM, state 0 = base/normal, prop 3803 = TMT_TEXTCOLOR
		if (partId == 1 && stateId == 0 && propId == 3803)
		{
			outColor = umbra::getViewTextColor();
			return true;
		}

		return false;
	}

	void setChildCtrlsTheme(HWND hParent)
	{
#if defined(_DARKMODE_SUPPORT_OLDER_OS)
		umbra::setChildCtrlsSubclassAndTheme(hParent, false, true);
#else
		umbra::setChildCtrlsSubclassAndTheme(hParent, false, umbra::isAtLeastWindows10());
#endif
	}

	/**
	 * @brief Window subclass procedure for handling `WM_ERASEBKGND` message.
	 *
	 * Handles `WM_ERASEBKGND` to fill the window's client area with the custom color brush,
	 * preventing default light gray flicker or mismatched fill.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setWindowEraseBgSubclass()
	 * @see umbra::removeWindowEraseBgSubclass()
	 */
	static LRESULT CALLBACK WindowEraseBgSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowEraseBgSubclass, uIdSubclass);
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, umbra::getDlgBackgroundBrush());
				return TRUE;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies window subclassing to handle `WM_ERASEBKGND` message.
	 *
	 * @param hWnd Handle to the control to subclass.
	 *
	 * @see umbra::WindowEraseBgSubclass()
	 * @see umbra::removeWindowEraseBgSubclass()
	 */
	void setWindowEraseBgSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, WindowEraseBgSubclass, kWindowEraseBgSubclassID);
	}

	/**
	 * @brief Removes the subclass used for `WM_ERASEBKGND` message handling.
	 *
	 * Detaches the window's subclass proc used for `WM_ERASEBKGND` message handling.
	 *
	 * @param hWnd Handle to the previously subclassed window.
	 *
	 * @see umbra::WindowEraseBgSubclass()
	 * @see umbra::removeWindowEraseBgSubclass()
	 */
	void removeWindowEraseBgSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, WindowEraseBgSubclass, kWindowEraseBgSubclassID);
	}

	/**
	 * @brief Window subclass procedure for handling `WM_CTLCOLOR*` messages.
	 *
	 * Handles control drawing messages to apply foreground and background
	 * styling based on control type and class.
	 *
	 * Handles:
	 * - `WM_CTLCOLOREDIT`, `WM_CTLCOLORLISTBOX`, `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
	 * - `WM_PRINTCLIENT` for removing light border for push buttons in dark mode
	 *
	 * Cleans up subclass on `WM_NCDESTROY`
	 *
	 * Uses `umbra::onCtlColor*` utilities.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::onCtlColor()
	 * @see umbra::onCtlColorDlg()
	 * @see umbra::onCtlColorDlgStaticText()
	 * @see umbra::onCtlColorDlgLinkText()
	 * @see umbra::onCtlColorListbox()
	 */
	static LRESULT CALLBACK WindowCtlColorSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowCtlColorSubclass, uIdSubclass);
				break;
			}

			case WM_CTLCOLOREDIT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORLISTBOX:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorListbox(wParam, lParam);
			}

			case WM_CTLCOLORDLG:
			{

				if (!umbra::isEnabled())
				{
					break;
				}
				return umbra::onCtlColorDlg(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORSTATIC:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				auto hChild = reinterpret_cast<HWND>(lParam);
				const bool isChildEnabled = ::IsWindowEnabled(hChild) == TRUE;
				const std::wstring className = GetWndClassName(hChild);

				auto hdc = reinterpret_cast<HDC>(wParam);

				if (className == WC_EDIT)
				{
					if (isChildEnabled)
					{
						return umbra::onCtlColor(hdc);
					}
					return umbra::onCtlColorDlg(hdc);
				}

				if (className == WC_LINK)
				{
					return umbra::onCtlColorDlgLinkText(hdc, isChildEnabled);
				}

				DWORD_PTR dwRefDataStaticText = 0;
				if (::GetWindowSubclass(hChild, StaticTextSubclass, kStaticTextSubclassID, &dwRefDataStaticText) == TRUE)
				{
					const bool isTextEnabled = (reinterpret_cast<StaticTextData*>(dwRefDataStaticText))->_isEnabled;
					return umbra::onCtlColorDlgStaticText(hdc, isTextEnabled);
				}
				return umbra::onCtlColorDlg(hdc);
			}

			case WM_PRINTCLIENT:
			{
				if (!umbra::isEnabled())
				{
					break;
				}
				return TRUE;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies window subclassing to handle `WM_CTLCOLOR*` messages.
	 *
	 * Enable custom colors for edit, listbox, static, and dialog elements
	 * via @ref umbra::WindowCtlColorSubclass.
	 *
	 * @param hWnd Handle to the parent or composite control (dialog, rebar, toolbar, ...) to subclass.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::removeWindowCtlColorSubclass()
	 */
	void setWindowCtlColorSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, WindowCtlColorSubclass, kWindowCtlColorSubclassID);
	}

	/**
	 * @brief Removes the subclass used for `WM_CTLCOLOR*` messages handling.
	 *
	 * Detaches the window's subclass proc used for `WM_CTLCOLOR*` messages handling.
	 *
	 * @param hWnd Handle to the previously subclassed window.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::setWindowCtlColorSubclass()
	 */
	void removeWindowCtlColorSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, WindowCtlColorSubclass, kWindowCtlColorSubclassID);
	}

	/**
	 * @brief Applies custom drawing to a toolbar items (buttons) during `CDDS_ITEMPREPAINT`
	 *
	 * Handles color assignment and background painting for toolbar buttons during the
	 * `CDDS_ITEMPREPAINT` stage of `NMTBCUSTOMDRAW`. Applies appropriate brushes, pens,
	 * and background rendering depending on the button state:
	 * - **Hot**: Uses hot background and edge styling.
	 * - **Checked**: Uses control background and standard edge styling.
	 * - **Drop-down**: Calculates and paints iconic split-button drop arrow.
	 *
	 * Also configures transparency and color usage for text, hot-tracking, and background fills.
	 * Ensures hot/checked states are visually overridden by custom color highlights.
	 *
	 * @param lptbcd Reference to the toolbar's custom draw structure.
	 * @return Flags to control draw behavior (`TBCDRF_USECDCOLORS`, `TBCDRF_NOBACKGROUND`, `CDRF_NOTIFYPOSTPAINT`).
	 *
	 * @note This function clears `CDIS_HOT`/`CDIS_CHECKED` to allow manual visual overrides.
	 *
	 * @see umbra::postpaintToolbarItem()
	 * @see umbra::darkToolbarNotifyCustomDraw()
	 */
	[[nodiscard]] static LRESULT prepaintToolbarItem(LPNMTBCUSTOMDRAW& lptbcd)
	{
		// Set colors

		lptbcd->hbrMonoDither = umbra::getBackgroundBrush();
		lptbcd->hbrLines = umbra::getEdgeBrush();
		lptbcd->hpenLines = umbra::getEdgePen();
		lptbcd->clrText = umbra::getDarkerTextColor();
		lptbcd->clrTextHighlight = umbra::getTextColor();
		lptbcd->clrBtnFace = umbra::getBackgroundColor();
		lptbcd->clrBtnHighlight = umbra::getCtrlBackgroundColor();
		lptbcd->clrHighlightHotTrack = umbra::getHotBackgroundColor();
		lptbcd->nStringBkMode = TRANSPARENT;
		lptbcd->nHLStringBkMode = TRANSPARENT;

		// Get styles and rectangles

		const bool isHot = (lptbcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;
		const bool isChecked = (lptbcd->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED;

		RECT rcItem{ lptbcd->nmcd.rc };
		RECT rcDrop{};

		TBBUTTONINFOW tbi{};
		tbi.cbSize = sizeof(TBBUTTONINFOW);
		tbi.dwMask = TBIF_IMAGE | TBIF_STYLE;
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));

		const bool isIcon = tbi.iImage != I_IMAGENONE;
		const bool isDropDown = ((tbi.fsStyle & BTNS_DROPDOWN) == BTNS_DROPDOWN) && isIcon; // has 2 "buttons"
		if (isDropDown)
		{
			const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
			::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcDrop));

			rcItem.right = rcDrop.left;
		}

		static const int roundness = umbra::isAtLeastWindows11() ? kWin11CornerRoundness + 1 : 0;

		// Paint part

		if (isHot) // hot must have higher priority to overwrite checked state
		{
			if (!isIcon)
			{
				::FillRect(lptbcd->nmcd.hdc, &rcItem, umbra::getHotBackgroundBrush());
			}
			else
			{
				umbra::paintRoundRect(lptbcd->nmcd.hdc, rcItem, umbra::getHotEdgePen(), umbra::getHotBackgroundBrush(), roundness, roundness);
				if (isDropDown)
				{
					umbra::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, umbra::getHotEdgePen(), umbra::getHotBackgroundBrush(), roundness, roundness);
				}
			}

			lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED | CDIS_HOT); // clears states to use custom highlight
		}
		else if (isChecked)
		{
			if (!isIcon)
			{
				::FillRect(lptbcd->nmcd.hdc, &rcItem, umbra::getCtrlBackgroundBrush());
			}
			else
			{
				umbra::paintRoundRect(lptbcd->nmcd.hdc, rcItem, umbra::getEdgePen(), umbra::getCtrlBackgroundBrush(), roundness, roundness);
				if (isDropDown)
				{
					umbra::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, umbra::getEdgePen(), umbra::getCtrlBackgroundBrush(), roundness, roundness);
				}
			}

			lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED); // clears state to use custom highlight
		}

		LRESULT retVal = TBCDRF_USECDCOLORS;
		if ((lptbcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			retVal |= TBCDRF_NOBACKGROUND;
		}

		if (isDropDown)
		{
			retVal |= CDRF_NOTIFYPOSTPAINT;
		}

		return retVal;
	}

	/**
	 * @brief Applies custom drawing to a toolbar items (buttons) during `CDDS_ITEMPOSTPAINT.
	 *
	 * Paints arrow glyph with custom color over system black "⏷" for button with style `BTNS_DROPDOWN`.
	 * Triggered by `CDRF_NOTIFYPOSTPAINT` from @ref umbra::prepaintToolbarItem.
	 *
	 * Logic:
	 * - Retrieves the drop-down rectangle via `TB_GETITEMDROPDOWNRECT`.
	 * - Selects the toolbar font and draws a centered arrow glyph with custom text color.
	 *
	 * @param lptbcd Reference to `LPNMTBCUSTOMDRAW`.
	 * @return `CDRF_DODEFAULT` to let default text/icon rendering proceed normally.
	 *
	 * @note Only applies to iconic buttons.
	 *
	 * @see umbra::prepaintToolbarItem()
	 * @see umbra::darkToolbarNotifyCustomDraw()
	 */
	[[nodiscard]] static LRESULT postpaintToolbarItem(LPNMTBCUSTOMDRAW& lptbcd)
	{
		TBBUTTONINFOW tbi{};
		tbi.cbSize = sizeof(TBBUTTONINFOW);
		tbi.dwMask = TBIF_IMAGE;
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));
		const bool isIcon = tbi.iImage != I_IMAGENONE;
		if (!isIcon)
		{
			return CDRF_DODEFAULT;
		}

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(lptbcd->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(lptbcd->nmcd.hdc, hFont));

		RECT rcArrow{};
		const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcArrow));
		rcArrow.left += 1;
		rcArrow.bottom -= 3;

		::SetBkMode(lptbcd->nmcd.hdc, TRANSPARENT);
		::SetTextColor(lptbcd->nmcd.hdc, umbra::getTextColor());
		::DrawText(lptbcd->nmcd.hdc, L"⏷", -1, &rcArrow, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		::SelectObject(lptbcd->nmcd.hdc, holdFont);

		return CDRF_DODEFAULT;
	}

	/**
	 * @brief Handles custom draw notifications for a toolbar control.
	 *
	 * Processes `NMTBCUSTOMDRAW` messages to provide custom color painting
	 * at each stage of the custom draw cycle:
	 * - **CDDS_PREPAINT**: Fills the toolbar background and requests item-level drawing.
	 * - **CDDS_ITEMPREPAINT**: Applies custom item painting via @ref umbra::prepaintToolbarItem.
	 * - **CDDS_ITEMPOSTPAINT**: Paints dropdown arrows glyphs via @ref umbra::postpaintToolbarItem.
	 *
	 * @param hWnd Handle to the toolbar control.
	 * @param uMsg Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
	 * @param wParam Message parameter (forwarded to default subclass processing).
	 * @param lParam Pointer to `NMTBCUSTOMDRAW`.
	 * @return `LRESULT` containing draw flags or the result of default subclass processing.
	 *
	 * @see umbra::prepaintToolbarItem()
	 * @see umbra::postpaintToolbarItem()
	 */
	[[nodiscard]] static LRESULT darkToolbarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lptbcd = reinterpret_cast<LPNMTBCUSTOMDRAW>(lParam);

		switch (lptbcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				::FillRect(lptbcd->nmcd.hdc, &lptbcd->nmcd.rc, umbra::getDlgBackgroundBrush());
				return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
			}

			case CDDS_ITEMPREPAINT:
			{
				return umbra::prepaintToolbarItem(lptbcd);
			}

			case CDDS_ITEMPOSTPAINT:
			{
				return umbra::postpaintToolbarItem(lptbcd);
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies custom drawing to a list view item during `CDDS_ITEMPREPAINT`.
	 *
	 * Sets text/background colors and fills the item rectangle based on state and style.
	 * Handles list view custom colors and styles, and adapts to grid line configuration.
	 *
	 * Behavior:
	 * - **Selected**: Uses `umbra::getCtrlBackground*()` colors and text brush.
	 * - **Hot**: Uses `umbra::getHotBackground*()` colors with optional hover frame.
	 * - **Gridlines active**: Fills the entire row background, column by column.
	 *
	 * @param lplvcd Reference to `LPNMLVCUSTOMDRAW`.
	 * @param isReport Whether list view is in `LVS_REPORT` mode.
	 * @param hasGridLines Whether grid lines are enabled (`LVS_EX_GRIDLINES`).
	 *
	 * @see umbra::darkListViewNotifyCustomDraw()
	 */
	static void prepaintListViewItem(LPNMLVCUSTOMDRAW& lplvcd, bool isReport, bool hasGridLines)
	{
		const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
		const bool isSelected = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED;
		const bool isFocused = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_FOCUSED) == LVIS_FOCUSED;
		const bool isHot = (lplvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;

		HBRUSH hBrush = nullptr;

		if (isSelected)
		{
			lplvcd->clrText = umbra::getTextColor();
			lplvcd->clrTextBk = umbra::getCtrlBackgroundColor();
			hBrush = umbra::getCtrlBackgroundBrush();
		}
		else if (isHot)
		{
			lplvcd->clrText = umbra::getTextColor();
			lplvcd->clrTextBk = umbra::getHotBackgroundColor();
			hBrush = umbra::getHotBackgroundBrush();
		}

		if (hBrush != nullptr)
		{
			if (!isReport || hasGridLines)
			{
				::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, hBrush);
			}
			else
			{
				HWND hHeader = ListView_GetHeader(hList);
				const int nCol = Header_GetItemCount(hHeader);
				const LONG paddingLeft = umbra::isThemeDark() ? 1 : 0;
				const LONG paddingRight = umbra::isThemeDark() ? 2 : 1;

				LVITEMINDEX lvii{ static_cast<int>(lplvcd->nmcd.dwItemSpec), 0 };
				RECT rcSubitem{
					lplvcd->nmcd.rc.left
					, lplvcd->nmcd.rc.top
					, lplvcd->nmcd.rc.left + ListView_GetColumnWidth(hList, 0) - paddingRight
					, lplvcd->nmcd.rc.bottom
				};
				::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);

				for (int i = 1; i < nCol; ++i)
				{
					ListView_GetItemIndexRect(hList, &lvii, i, LVIR_BOUNDS, &rcSubitem);
					rcSubitem.left -= paddingLeft;
					rcSubitem.right -= paddingRight;
					::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);
				}
			}
		}
		else if (hasGridLines)
		{
			::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, umbra::getViewBackgroundBrush());
		}

		if (isFocused)
		{
#if 0 // for testing
			::DrawFocusRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc);
#endif
		}
		else if (!isSelected && isHot && !hasGridLines)
		{
			::FrameRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, umbra::getHotEdgeBrush());
		}
	}

	/**
	 * @brief Handles custom draw notifications for a list view control.
	 *
	 * Processes `NMLVCUSTOMDRAW` messages to provide custom color painting
	 * at each stage of the custom draw cycle:
	 * - **CDDS_PREPAINT**: Optionally fills the list view with grid lines
	 *                      with custom background color and requests item-level drawing.
	 * - **CDDS_ITEMPREPAINT**: Applies custom item painting via @ref umbra::prepaintListViewItem.
	 *
	 * @param hWnd Handle to the list view control.
	 * @param uMsg Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
	 * @param wParam Message parameter (forwarded to default subclass processing).
	 * @param lParam Pointer to `NMLVCUSTOMDRAW`.
	 * @return `LRESULT` containing draw flags or the result of default subclass processing.
	 *
	 * @see umbra::prepaintListViewItem()
	 */
	[[nodiscard]] static LRESULT darkListViewNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
		const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
		const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
		const bool isReport = (lvStyle == LVS_REPORT);
		bool hasGridlines = false;
		if (isReport)
		{
			const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
			hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
		}

		switch (lplvcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				if (isReport && hasGridlines)
				{
					::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, umbra::getViewBackgroundBrush());
				}

				return CDRF_NOTIFYITEMDRAW;
			}

			case CDDS_ITEMPREPAINT:
			{
				umbra::prepaintListViewItem(lplvcd, isReport, hasGridlines);
				return CDRF_NEWFONT;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies custom drawing to a tree view node during `CDDS_ITEMPREPAINT`.
	 *
	 * Colors the node background for selection/hot states, assigns text color,
	 * and requests optional post-paint framing.
	 *
	 * @param lptvcd Reference to `LPNMTVCUSTOMDRAW`.
	 * @return Bitmask with `CDRF_NEWFONT`, `CDRF_NOTIFYPOSTPAINT` if drawing was applied.
	 *
	 * @see umbra::postpaintTreeViewItem()
	 * @see umbra::darkTreeViewNotifyCustomDraw()
	 */
	[[nodiscard]] static LRESULT prepaintTreeViewItem(LPNMTVCUSTOMDRAW& lptvcd)
	{
		LRESULT retVal = CDRF_DODEFAULT;

		if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			lptvcd->clrText = umbra::getTextColor();
			lptvcd->clrTextBk = umbra::getCtrlBackgroundColor();
			::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, umbra::getCtrlBackgroundBrush());

			retVal |= CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
		}
		else if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
		{
			lptvcd->clrText = umbra::getTextColor();
			lptvcd->clrTextBk = umbra::getHotBackgroundColor();

			if (umbra::isAtLeastWindows10() || umbra::getTreeViewStyle() == TreeViewStyle::light)
			{
				::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, umbra::getHotBackgroundBrush());
				retVal |= CDRF_NOTIFYPOSTPAINT;
			}
			retVal |= CDRF_NEWFONT;
		}

		return retVal;
	}

	/**
	 * @brief Applies custom drawing to a tree view node during `CDDS_ITEMPOSTPAINT`.
	 *
	 * Paints a frame around a tree view node after painting based on state.
	 *
	 * @param lptvcd Reference to `LPNMTVCUSTOMDRAW`.
	 *
	 * @see umbra::prepaintTreeViewItem()
	 * @see umbra::darkTreeViewNotifyCustomDraw()
	 */
	static void postpaintTreeViewItem(LPNMTVCUSTOMDRAW& lptvcd)
	{
		RECT rcFrame{ lptvcd->nmcd.rc };
		::InflateRect(&rcFrame, 1, 0);

		if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
		{
			umbra::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, umbra::getHotEdgePen(), 0, 0);
		}
		else if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			umbra::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, umbra::getEdgePen(), 0, 0);
		}
	}

	/**
	 * @brief Handles custom draw notifications for a tree view control.
	 *
	 * Processes `NMTVCUSTOMDRAW` messages to provide custom color painting
	 * at each stage of the custom draw cycle:
	 * - **CDDS_PREPAINT**: Requests item-level drawing.
	 * - **CDDS_ITEMPREPAINT**: Applies custom item painting based on state via @ref umbra::prepaintTreeViewItem.
	 * - **CDDS_ITEMPOSTPAINT**: Paints frames based on state via @ref umbra::postpaintTreeViewItem.
	 *
	 * @param hWnd Handle to the tree view control.
	 * @param uMsg Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
	 * @param wParam Message parameter (forwarded to default subclass processing).
	 * @param lParam Pointer to `NMTVCUSTOMDRAW`.
	 * @return `LRESULT` containing draw flags or the result of default subclass processing.
	 *
	 * @see umbra::prepaintTreeViewItem()
	 * @see umbra::postpaintTreeViewItem()
	 */
	[[nodiscard]] static LRESULT darkTreeViewNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lptvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);

		switch (lptvcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				return CDRF_NOTIFYITEMDRAW;
			}

			case CDDS_ITEMPREPAINT:
			{
				const LRESULT retVal = umbra::prepaintTreeViewItem(lptvcd);
				if (retVal == CDRF_DODEFAULT)
				{
					break;
				}
				return retVal;
			}

			case CDDS_ITEMPOSTPAINT:
			{
				umbra::postpaintTreeViewItem(lptvcd);
				return CDRF_DODEFAULT;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies custom drawing to a trackbar items during `CDDS_ITEMPREPAINT`.
	 *
	 * Colors the trackbar thumb background for selection state,
	 * and colors the trackbar slider based on if tracbar is enabled.
	 * For trackbar with style `TBS_AUTOTICKS` default handling is used.
	 *
	 * @param lpnmcd Reference to `LPNMCUSTOMDRAW`.
	 * @return `CDRF_SKIPDEFAULT` if drawing was applied.
	 *
	 * @see umbra::darkTrackbarNotifyCustomDraw()
	 */
	[[nodiscard]] static LRESULT prepaintTrackbarItem(LPNMCUSTOMDRAW& lpnmcd)
	{
		LRESULT retVal = CDRF_DODEFAULT;

		switch (lpnmcd->dwItemSpec)
		{
			case TBCD_TICS:
			{
				break;
			}

			case TBCD_THUMB:
			{
				if ((lpnmcd->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
				{
					::FillRect(lpnmcd->hdc, &lpnmcd->rc, umbra::getCtrlBackgroundBrush());
					retVal = CDRF_SKIPDEFAULT;
				}
				break;
			}

			case TBCD_CHANNEL: // slider
			{
				if (::IsWindowEnabled(lpnmcd->hdr.hwndFrom) == FALSE)
				{
					::FillRect(lpnmcd->hdc, &lpnmcd->rc, umbra::getDlgBackgroundBrush());
					umbra::paintRoundFrameRect(lpnmcd->hdc, lpnmcd->rc, umbra::getEdgePen(), 0, 0);
				}
				else
				{
					::FillRect(lpnmcd->hdc, &lpnmcd->rc, umbra::getCtrlBackgroundBrush());
				}

				retVal = CDRF_SKIPDEFAULT;
				break;
			}

			default:
			{
				break;
			}
		}

		return retVal;
	}

	/**
	 * @brief Handles custom draw notifications for a trackbar control.
	 *
	 * Processes `NMCUSTOMDRAW` messages to provide custom color painting
	 * at each stage of the custom draw cycle:
	 * - **CDDS_PREPAINT**: Requests item-level drawing.
	 * - **CDDS_ITEMPREPAINT**: Applies custom item painting based on item type via @ref umbra::prepaintTrackbarItem.
	 *
	 * @param hWnd Handle to the trackbar control.
	 * @param uMsg Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
	 * @param wParam Message parameter (forwarded to default subclass processing).
	 * @param lParam Pointer to `NMCUSTOMDRAW`.
	 * @return `LRESULT` containing draw flags or the result of default subclass processing.
	 *
	 * @see umbra::prepaintTrackbarItem()
	 */
	[[nodiscard]] static LRESULT darkTrackbarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);

		switch (lpnmcd->dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				return CDRF_NOTIFYITEMDRAW;
			}

			case CDDS_ITEMPREPAINT:
			{
				const LRESULT retVal = umbra::prepaintTrackbarItem(lpnmcd);
				if (retVal == CDRF_DODEFAULT)
				{
					break;
				}
				return retVal;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies custom drawing to a rebar control during `CDDS_PREPAINT`.
	 *
	 * Paints chevrons and 'gripper' edges for all bands if applicable.
	 *
	 * @param lpnmcd Reference to `LPNMCUSTOMDRAW`.
	 * @return `CDRF_SKIPDEFAULT` if drawing was applied.
	 *
	 * @see umbra::darkRebarNotifyCustomDraw()
	 */
	[[nodiscard]] static LRESULT prepaintRebar(LPNMCUSTOMDRAW& lpnmcd)
	{
		::FillRect(lpnmcd->hdc, &lpnmcd->rc, umbra::getDlgBackgroundBrush());

		REBARBANDINFO rbBand{};
		rbBand.cbSize = sizeof(REBARBANDINFO);
		rbBand.fMask = RBBIM_STYLE | RBBIM_CHEVRONLOCATION | RBBIM_CHEVRONSTATE;

		const auto nBands = static_cast<UINT>(::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETBANDCOUNT, 0, 0));
		for (UINT i = 0; i < nBands; ++i)
		{
			::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETBANDINFO, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rbBand));

			// paints chevron
			if ((rbBand.fStyle & RBBS_USECHEVRON) == RBBS_USECHEVRON
				&& (rbBand.rcChevronLocation.right - rbBand.rcChevronLocation.left) > 0)
			{
				static const int roundness = umbra::isAtLeastWindows11() ? kWin11CornerRoundness + 1 : 0;

				const bool isHot = (rbBand.uChevronState & STATE_SYSTEM_HOTTRACKED) == STATE_SYSTEM_HOTTRACKED;
				const bool isPressed = (rbBand.uChevronState & STATE_SYSTEM_PRESSED) == STATE_SYSTEM_PRESSED;

				if (isHot)
				{
					umbra::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, umbra::getHotEdgePen(), umbra::getHotBackgroundBrush(), roundness, roundness);
				}
				else if (isPressed)
				{
					umbra::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, umbra::getEdgePen(), umbra::getCtrlBackgroundBrush(), roundness, roundness);
				}

				::SetTextColor(lpnmcd->hdc, isHot ? umbra::getTextColor() : umbra::getDarkerTextColor());
				::SetBkMode(lpnmcd->hdc, TRANSPARENT);

				static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOCLIP;
				::DrawText(lpnmcd->hdc, L"»", -1, &rbBand.rcChevronLocation, dtFlags);
			}

			// paints gripper edge
			if ((rbBand.fStyle & RBBS_GRIPPERALWAYS) == RBBS_GRIPPERALWAYS
				&& ((rbBand.fStyle & RBBS_FIXEDSIZE) != RBBS_FIXEDSIZE
					|| (rbBand.fStyle & RBBS_NOGRIPPER) != RBBS_NOGRIPPER))
			{
				auto holdPen = static_cast<HPEN>(::SelectObject(lpnmcd->hdc, umbra::getDarkerTextPen()));

				RECT rcBand{};
				::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rcBand));

				static constexpr LONG offset = 5;
				const std::array<POINT, 2> edges{ {
					{ rcBand.left, rcBand.top + offset},
					{ rcBand.left, rcBand.bottom - offset}
				} };
				::Polyline(lpnmcd->hdc, edges.data(), static_cast<int>(edges.size()));

				::SelectObject(lpnmcd->hdc, holdPen);
			}
		}
		return CDRF_SKIPDEFAULT;
	}

	/**
	 * @brief Handles custom draw notifications for a rebar control.
	 *
	 * Processes `NMCUSTOMDRAW` messages to provide custom color painting
	 * at each stage of the custom draw cycle:
	 * - **CDDS_PREPAINT**: Applies custom painting based on item type via @ref umbra::prepaintRebar.
	 *
	 * @param hWnd Handle to the rebar control.
	 * @param uMsg Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
	 * @param wParam Message parameter (forwarded to default subclass processing).
	 * @param lParam Pointer to `NMCUSTOMDRAW`.
	 * @return `LRESULT` containing draw flags or the result of default subclass processing.
	 *
	 * @see umbra::prepaintRebar()
	 */
	[[nodiscard]] static LRESULT darkRebarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
		if (lpnmcd->dwDrawStage == CDDS_PREPAINT)
		{
			return umbra::prepaintRebar(lpnmcd);
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Window subclass procedure for handling `WM_NOTIFY` message for custom draw for supported controls.
	 *
	 * Handles `WM_NOTIFY` for custom draw for supported controls:
	 * - toolbar, list view, tree view, trackbar, and rebar.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setWindowNotifyCustomDrawSubclass()
	 * @see umbra::removeWindowNotifyCustomDrawSubclass()
	 */
	static LRESULT CALLBACK WindowNotifySubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowNotifySubclass, uIdSubclass);
				break;
			}

			case WM_NOTIFY:
			{
				if (!umbra::isEnabled())
				{
					break;
				}

				auto* lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
				if (lpnmhdr->code == NM_CUSTOMDRAW)
				{
					const std::wstring className = GetWndClassName(lpnmhdr->hwndFrom);

					if (className == TOOLBARCLASSNAME)
					{
						return umbra::darkToolbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == WC_LISTVIEW)
					{
						return umbra::darkListViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == WC_TREEVIEW)
					{
						return umbra::darkTreeViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == TRACKBAR_CLASS)
					{
						return umbra::darkTrackbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == REBARCLASSNAME)
					{
						return umbra::darkRebarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies window subclassing for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	 *
	 * Installs @ref umbra::WindowNotifySubclass.
	 * Enables handling of `WM_NOTIFY` `NM_CUSTOMDRAW` notifications for custom drawing
	 * behavior for supported controls.
	 *
	 * @param hWnd Handle to the window with child which support `NM_CUSTOMDRAW`.
	 *
	 * @see umbra::WindowNotifySubclass()
	 * @see umbra::removeWindowNotifyCustomDrawSubclass()
	 */
	void setWindowNotifyCustomDrawSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, WindowNotifySubclass, kWindowNotifySubclassID);
	}

	/**
	 * @brief Removes the subclass used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	 *
	 * Detaches the window's subclass proc used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	 *
	 * @param hWnd Handle to the previously subclassed window.
	 *
	 * @see umbra::WindowNotifySubclass()
	 * @see umbra::setWindowNotifyCustomDrawSubclass()
	 */
	void removeWindowNotifyCustomDrawSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, WindowNotifySubclass, kWindowNotifySubclassID);
	}

	/**
	 * @brief Fills the menu bar background custom color.
	 *
	 * Uses `GetMenuBarInfo` and `GetWindowRect` to compute the menu bar rectangle
	 * in client-relative coordinates, then fills it with @ref umbra::getDlgBackgroundBrush.
	 *
	 * @param hWnd Handle to the window with a menu bar.
	 * @param hdc Target device context for painting.
	 *
	 * @note Offsets top slightly to account for non-client overlap.
	 */
	static void paintMenuBar(HWND hWnd, HDC hdc)
	{
		// get the menubar rect
		MENUBARINFO mbi{};
		mbi.cbSize = sizeof(MENUBARINFO);
		::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

		RECT rcWindow{};
		::GetWindowRect(hWnd, &rcWindow);

		// the rcBar is offset by the window rect
		RECT rcBar{ mbi.rcBar };
		::OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);

		rcBar.top -= 1;

		::FillRect(hdc, &rcBar, umbra::getDlgBackgroundBrush());
	}

	/**
	 * @brief Paints a single menu bar item with custom colors based on state.
	 *
	 * Measures and renders menu item text using `DrawThemeTextEx`, and
	 * fills background using appropriate brush based on `ODS_*` item state.
	 *
	 * @param UDMI Reference to `UAHDRAWMENUITEM` struct from `WM_UAHDRAWMENUITEM`.
	 * @param hTheme The themed handle to `VSCLASS_MENU` (via @ref ThemeData).
	 *
	 * @see umbra::WindowMenuBarSubclass()
	 */
	static void paintMenuBarItems(UAHDRAWMENUITEM& UDMI, const HTHEME& hTheme)
	{
		// get the menu item string
		std::wstring buffer(MAX_PATH, L'\0');
		MENUITEMINFO mii{};
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STRING;
		mii.dwTypeData = buffer.data();
		mii.cch = MAX_PATH - 1;

		::GetMenuItemInfo(UDMI.um.hmenu, static_cast<UINT>(UDMI.umi.iPosition), TRUE, &mii);

		// get the item state for drawing

		DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;

		int iTextStateID = MBI_NORMAL;
		int iBackgroundStateID = MBI_NORMAL;
		if ((UDMI.dis.itemState & ODS_SELECTED) == ODS_SELECTED)
		{
			// clicked
			iTextStateID = MBI_PUSHED;
			iBackgroundStateID = MBI_PUSHED;
		}
		else if ((UDMI.dis.itemState & ODS_HOTLIGHT) == ODS_HOTLIGHT)
		{
			// hot tracking
			iTextStateID = ((UDMI.dis.itemState & ODS_INACTIVE) == ODS_INACTIVE) ? MBI_DISABLEDHOT : MBI_HOT;
			iBackgroundStateID = MBI_HOT;
		}
		else if (((UDMI.dis.itemState & ODS_GRAYED) == ODS_GRAYED)
			|| ((UDMI.dis.itemState & ODS_DISABLED) == ODS_DISABLED)
			|| ((UDMI.dis.itemState & ODS_INACTIVE) == ODS_INACTIVE))
		{
			// disabled / grey text / inactive
			iTextStateID = MBI_DISABLED;
			iBackgroundStateID = MBI_DISABLED;
		}
		else if ((UDMI.dis.itemState & ODS_DEFAULT) == ODS_DEFAULT)
		{
			// normal display
			iTextStateID = MBI_NORMAL;
			iBackgroundStateID = MBI_NORMAL;
		}

		if ((UDMI.dis.itemState & ODS_NOACCEL) == ODS_NOACCEL)
		{
			dwFlags |= DT_HIDEPREFIX;
		}

		switch (iBackgroundStateID)
		{
			case MBI_NORMAL:
			case MBI_DISABLED:
			{
				::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, umbra::getDlgBackgroundBrush());
				break;
			}

			case MBI_HOT:
			case MBI_DISABLEDHOT:
			{
				::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, umbra::getHotBackgroundBrush());
				break;
			}

			case MBI_PUSHED:
			case MBI_DISABLEDPUSHED:
			{
				::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, umbra::getCtrlBackgroundBrush());
				break;
			}

			default:
			{
				::DrawThemeBackground(hTheme, UDMI.um.hdc, MENU_BARITEM, iBackgroundStateID, &UDMI.dis.rcItem, nullptr);
				break;
			}
		}

		DTTOPTS dttopts{};
		dttopts.dwSize = sizeof(DTTOPTS);
		dttopts.dwFlags = DTT_TEXTCOLOR;
		switch (iTextStateID)
		{
			case MBI_NORMAL:
			case MBI_HOT:
			case MBI_PUSHED:
			{
				dttopts.crText = umbra::getTextColor();
				break;
			}

			case MBI_DISABLED:
			case MBI_DISABLEDHOT:
			case MBI_DISABLEDPUSHED:
			{
				dttopts.crText = umbra::getDisabledTextColor();
				break;
			}

			default:
			{
				break;
			}
		}

		::DrawThemeTextEx(hTheme, UDMI.um.hdc, MENU_BARITEM, iTextStateID, buffer.c_str(), static_cast<int>(mii.cch), dwFlags, &UDMI.dis.rcItem, &dttopts);
	}

	/**
	 * @brief Over-paints the 1-pixel light line under a menu bar with custom color.
	 *
	 * Called post-paint to overwrite non-client leftovers that break custom color styling.
	 * Computes exact line position based on `MenuBarInfo`, and fills with custom color.
	 *
	 * @param hWnd Handle to the window with a menu bar.
	 *
	 * @see umbra::WindowMenuBarSubclass()
	 */
	static void drawUAHMenuNCBottomLine(HWND hWnd)
	{
		MENUBARINFO mbi{};
		mbi.cbSize = sizeof(MENUBARINFO);
		if (::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi) == FALSE)
		{
			return;
		}

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);
		::MapWindowPoints(hWnd, nullptr, reinterpret_cast<POINT*>(&rcClient), 2);

		RECT rcWindow{};
		::GetWindowRect(hWnd, &rcWindow);

		::OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

		// the rcBar is offset by the window rect
		RECT rcAnnoyingLine{ rcClient };
		rcAnnoyingLine.bottom = rcAnnoyingLine.top;
		rcAnnoyingLine.top--;


		HDC hdc = ::GetWindowDC(hWnd);
		::FillRect(hdc, &rcAnnoyingLine, umbra::getDlgBackgroundBrush());
		::ReleaseDC(hWnd, hdc);
	}

	/**
	 * @brief Window subclass procedure for custom color for themed menu bar.
	 *
	 * Applies custom colors for menu bar, but not for popup menus.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData ThemeData instance.
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setWindowMenuBarSubclass()
	 * @see umbra::removeWindowMenuBarSubclass()
	 */
	static LRESULT CALLBACK WindowMenuBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pMenuThemeData = reinterpret_cast<ThemeData*>(dwRefData);

		if (uMsg != WM_NCDESTROY && (!umbra::isEnabled() || !pMenuThemeData->ensureTheme(hWnd)))
		{
			return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowMenuBarSubclass, uIdSubclass);
				delete pMenuThemeData;
				break;
			}

			case WM_UAHDRAWMENU:
			{
				auto* pUDM = reinterpret_cast<UAHMENU*>(lParam);
				umbra::paintMenuBar(hWnd, pUDM->hdc);

				return 0;
			}

			case WM_UAHDRAWMENUITEM:
			{
				const auto& hTheme = pMenuThemeData->getHTheme();
				auto* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
				umbra::paintMenuBarItems(*pUDMI, hTheme);

				return 0;
			}

#if 0 // for debugging
			case WM_UAHMEASUREMENUITEM:
			{
				auto* pMMI = reinterpret_cast<UAHMEASUREMENUITEM*>(lParam);
				return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			}
#endif

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			case WM_THEMECHANGED:
			{
				pMenuThemeData->closeTheme();
				break;
			}

			case WM_NCACTIVATE:
			case WM_NCPAINT:
			{
				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				umbra::drawUAHMenuNCBottomLine(hWnd);
				return retVal;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies window subclassing for menu bar themed custom drawing.
	 *
	 * Installs @ref umbra::WindowMenuBarSubclass with an associated `ThemeData` instance
	 * for the `VSCLASS_MENU` visual style. Enables custom drawing
	 * behavior for menu bar.
	 *
	 * @param hWnd Handle to the window with a menu bar.
	 *
	 * @see umbra::WindowMenuBarSubclass()
	 * @see umbra::removeWindowMenuBarSubclass()
	 */
	void setWindowMenuBarSubclass(HWND hWnd)
	{
		umbra::setSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, kWindowMenuBarSubclassID, VSCLASS_MENU);
	}

	/**
	 * @brief Removes the subclass used for menu bar themed custom drawing.
	 *
	 * Detaches the window's subclass proc used for menu bar themed custom drawing.
	 *
	 * @param hWnd Handle to the previously subclassed window.
	 *
	 * @see umbra::WindowMenuBarSubclass()
	 * @see umbra::setWindowMenuBarSubclass()
	 */
	void removeWindowMenuBarSubclass(HWND hWnd)
	{
		umbra::removeSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, kWindowMenuBarSubclassID);
	}

	/**
	 * @brief Window subclass procedure for handling `WM_SETTINGCHANGE` message.
	 *
	 * Handles `WM_SETTINGCHANGE` to perform changes for dark mode based on system setting.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see umbra::setWindowSettingChangeSubclass()
	 * @see umbra::removeWindowSettingChangeSubclass()
	 */
	static LRESULT CALLBACK WindowSettingChangeSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowSettingChangeSubclass, uIdSubclass);
				break;
			}

			case WM_SETTINGCHANGE:
			{
				if (umbra::handleSettingChange(lParam))
				{
					umbra::setDarkTitleBarEx(hWnd, true);
					umbra::setChildCtrlsTheme(hWnd);
					::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	/**
	 * @brief Applies window subclassing to handle `WM_SETTINGCHANGE` message.
	 *
	 * Enable monitoring WM_SETTINGCHANGE message,
	 * allowing the app to respond to system-wide dark mode change.
	 *
	 * @param hWnd Handle to the main window.
	 *
	 * @see umbra::WindowSettingChangeSubclass()
	 * @see umbra::removeWindowSettingChangeSubclass()
	 */
	void setWindowSettingChangeSubclass(HWND hWnd)
	{
		umbra::setSubclass(hWnd, WindowSettingChangeSubclass, kWindowSettingChangeSubclassID);
	}

	/**
	 * @brief Removes the subclass used for `WM_SETTINGCHANGE` message handling.
	 *
	 * Detaches the window's subclass proc used for `WM_SETTINGCHANGE` messages handling.
	 *
	 * @param hWnd Handle to the previously subclassed window.
	 *
	 * @see umbra::WindowSettingChangeSubclass()
	 * @see umbra::setWindowSettingChangeSubclass()
	 */
	void removeWindowSettingChangeSubclass(HWND hWnd)
	{
		umbra::removeSubclass(hWnd, WindowSettingChangeSubclass, kWindowSettingChangeSubclassID);
	}

	/**
	 * @brief Configures the SysLink control to be affected by `WM_CTLCOLORSTATIC` message.
	 *
	 * Configures all items to either use default system link colors if in classic mode,
	 * or to be affected by `WM_CTLCOLORSTATIC` message from its parent.
	 *
	 * @param hWnd Handle to the SysLink control.
	 *
	 * @note Will affect all items, even if it's static (non-clickable).
	 */
	void enableSysLinkCtrlCtlColor(HWND hWnd)
	{
		LITEM lItem{};
		lItem.iLink = 0;
		lItem.mask = LIF_ITEMINDEX | LIF_STATE;
		lItem.state = umbra::isEnabled() ? LIS_DEFAULTCOLORS : 0;
		lItem.stateMask = LIS_DEFAULTCOLORS;
		while (::SendMessage(hWnd, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&lItem)))
			++lItem.iLink;
	}

	/**
	 * @brief Sets dark title bar and optional Windows 11 features.
	 *
	 * For Windows 10 (2004+) and newer, this function configures the dark title bar using
	 * `DWMWA_USE_IMMERSIVE_DARK_MODE`. On Windows 11, if `useWin11Features` is `true`, it
	 * additionally applies:
	 * - Rounded corners (`DWMWA_WINDOW_CORNER_PREFERENCE`)
	 * - Border color (`DWMWA_BORDER_COLOR`)
	 * - Mica backdrop (`DWMWA_SYSTEMBACKDROP_TYPE`) if allowed and compatible
	 * - Static text color for text and dialog background color for background
	 *   (`DWMWA_CAPTION_COLOR`, `DWMWA_TEXT_COLOR`),
	 *   only when frames are not extended to full window
	 *
	 * If `_DARKMODE_SUPPORT_OLDER_OS` is defined and running on pre-2004 builds,
	 * fallback behavior will enable dark title bars via undocumented APIs.
	 *
	 * @param hWnd Handle to the top-level window.
	 * @param useWin11Features `true` to enable Windows 11 specific features such as Mica and rounded corners.
	 *
	 * @note Requires Windows 10 version 2004 (build 19041) or later.
	 *
	 * @see DwmSetWindowAttribute
	 * @see DwmExtendFrameIntoClientArea
	 */
	void setDarkTitleBarEx(HWND hWnd, bool useWin11Features)
	{
		static const DWORD buildNumber = umbra::getWindowsBuildNumber();
		if (buildNumber >= WinVerHelper::WIN10_VER_2004)
		{
			const BOOL useDark = umbra::isExperimentalActive() ? TRUE : FALSE;
			::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

			if (useWin11Features && umbra::isAtLeastWindows11())
			{
				::DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &g_dmCfg._roundCorner, sizeof(g_dmCfg._roundCorner));
				::DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &g_dmCfg._borderColor, sizeof(g_dmCfg._borderColor));

				bool canColorizeTitleBar = true;

				if (buildNumber >= WinVerHelper::WIN11_VER_22H2)
				{
					if (g_dmCfg._micaExtend && g_dmCfg._mica != DWMSBT_AUTO && !umbra::isWindowsModeEnabled() && (g_dmCfg._dmType == DarkModeType::dark))
					{
						static constexpr MARGINS margins{ -1, 0, 0, 0 };
						::DwmExtendFrameIntoClientArea(hWnd, &margins);
					}

					::DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &g_dmCfg._mica, sizeof(g_dmCfg._mica));

					canColorizeTitleBar = !g_dmCfg._micaExtend;
				}

				canColorizeTitleBar = g_dmCfg._colorizeTitleBar && canColorizeTitleBar && umbra::isEnabled();
				const COLORREF clrDlg = canColorizeTitleBar ? umbra::getDlgBackgroundColor() : DWMWA_COLOR_DEFAULT;
				const COLORREF clrText = canColorizeTitleBar ? umbra::getTextColor() : DWMWA_COLOR_DEFAULT;
				::DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &clrDlg, sizeof(clrDlg));
				::DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &clrText, sizeof(clrText));
			}
		}
#if defined(_DARKMODE_SUPPORT_OLDER_OS)
		else
		{
			umbra::allowDarkModeForWindow(hWnd, umbra::isExperimentalActive());
			umbra::setTitleBarThemeColor(hWnd);
		}
#endif
	}

	/**
	 * @brief Sets dark mode title bar on supported Windows versions.
	 *
	 * Delegates to @ref setDarkTitleBarEx with `useWin11Features = false`.
	 *
	 * @param hWnd Handle to the top-level window.
	 *
	 * @see umbra::setDarkTitleBarEx()
	 */
	void setDarkTitleBar(HWND hWnd)
	{
		umbra::setDarkTitleBarEx(hWnd, false);
	}

	/**
	 * @brief Applies an experimental visual style to the specified window, if supported.
	 *
	 * When experimental features are supported and active,
	 * this function enables dark experimental visual style on the window.
	 *
	 * @param hWnd Handle to the target window or control.
	 * @param themeClassName Name of the theme class to apply (e.g. L"Explorer", "ItemsView").
	 *
	 * @note This function is a no-op if experimental theming is not supported on the current OS.
	 *
	 * @see umbra::isExperimentalSupported()
	 * @see umbra::isExperimentalActive()
	 * @see umbra::allowDarkModeForWindow()
	 */
	void setDarkThemeExperimental(HWND hWnd, const wchar_t* themeClassName)
	{
		if (umbra::isExperimentalSupported())
		{
			umbra::allowDarkModeForWindow(hWnd, umbra::isExperimentalActive());
			::SetWindowTheme(hWnd, themeClassName, nullptr);
		}
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style if experimental mode is active.
	 *
	 * Useful for controls like list views or tree views to use dark scroll bars
	 * and explorer style theme in supported environments.
	 *
	 * @param hWnd Handle to the control or window to theme.
	 */
	void setDarkExplorerTheme(HWND hWnd)
	{
		::SetWindowTheme(hWnd, umbra::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr, nullptr);
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style to scroll bars.
	 *
	 * Convenience wrapper that calls @ref umbra::setDarkExplorerTheme to apply dark scroll bar
	 * for compatible controls (e.g. list views, tree views).
	 *
	 * @param hWnd Handle to the control with scroll bars.
	 *
	 * @see umbra::setDarkExplorerTheme()
	 */
	void setDarkScrollBar(HWND hWnd)
	{
		umbra::setDarkExplorerTheme(hWnd);
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style to tooltip controls based on context.
	 *
	 * Selects the appropriate `GETTOOLTIPS` message depending on the control type
	 * (e.g. toolbar, list view, tree view, tab bar) to retrieve the tooltip handle.
	 * If `ToolTipsType::tooltip` is specified, applies theming directly to `hWnd`.
	 *
	 * Internally calls @ref umbra::setDarkExplorerTheme to set dark tooltip.
	 *
	 * @param hWnd Handle to the parent control or tooltip.
	 * @param type The tooltip context type (toolbar, list view, etc.).
	 *
	 * @see umbra::setDarkExplorerTheme()
	 * @see ToolTipsType
	 */
	void setDarkTooltips(HWND hWnd, ToolTipsType type)
	{
		UINT msg = 0;
		switch (type)
		{
			case umbra::ToolTipsType::toolbar:
			{
				msg = TB_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::listview:
			{
				msg = LVM_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::treeview:
			{
				msg = TVM_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::tabbar:
			{
				msg = TCM_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::trackbar:
			{
				msg = TBM_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::rebar:
			{
				msg = RB_GETTOOLTIPS;
				break;
			}

			case umbra::ToolTipsType::tooltip:
			{
				msg = 0;
				break;
			}
		}

		if (msg == 0)
		{
			umbra::setDarkExplorerTheme(hWnd);
		}
		else
		{
			auto hTips = reinterpret_cast<HWND>(::SendMessage(hWnd, msg, 0, 0));
			if (hTips != nullptr)
			{
				umbra::setDarkExplorerTheme(hTips);
			}
		}
	}

	/**
	 * @brief Sets the color of line above a toolbar control for non-classic mode.
	 *
	 * Sends `TB_SETCOLORSCHEME` to customize the line drawn above the toolbar.
	 * When non-classic mode is enabled, sets both `clrBtnHighlight` and `clrBtnShadow`
	 * to the dialog background color, otherwise uses system defaults.
	 *
	 * @param hWnd Handle to the toolbar control.
	 */
	void setDarkLineAbovePanelToolbar(HWND hWnd)
	{
		COLORSCHEME scheme{};
		scheme.dwSize = sizeof(COLORSCHEME);

		if (umbra::isEnabled())
		{
			scheme.clrBtnHighlight = umbra::getDlgBackgroundColor();
			scheme.clrBtnShadow = umbra::getDlgBackgroundColor();
		}
		else
		{
			scheme.clrBtnHighlight = CLR_DEFAULT;
			scheme.clrBtnShadow = CLR_DEFAULT;
		}

		::SendMessage(hWnd, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
	}

	/**
	 * @brief Applies an experimental Explorer visual style to a list view.
	 *
	 * Uses @ref umbra::setDarkThemeExperimental with the `"Explorer"` theme class to adapt
	 * list view visuals (e.g. scroll bars, selection color) for dark mode, if supported.
	 *
	 * @param hWnd Handle to the list view control.
	 *
	 * @see umbra::setDarkThemeExperimental()
	 */
	void setDarkListView(HWND hWnd)
	{
		umbra::setDarkThemeExperimental(hWnd, L"Explorer");
	}

	/**
	 * @brief Replaces default list view checkboxes with themed dark-mode versions on Windows 11.
	 *
	 * If the list view uses `LVS_EX_CHECKBOXES` and is running on Windows 11 or later,
	 * this function manually renders the unchecked and checked checkbox visuals using
	 * themed drawing APIs, then inserts the resulting icons into the state image list.
	 *
	 * Uses `"DarkMode_Explorer::Button"` as the theme class if experimental dark mode is active;
	 * otherwise falls back to `VSCLASS_BUTTON`.
	 *
	 * @param hWnd Handle to the list view control with extended checkbox style.
	 *
	 * @note Does nothing on pre-Windows 11 systems or if checkboxes are not enabled.
	 */
	void setDarkListViewCheckboxes(HWND hWnd)
	{
		if (!umbra::isAtLeastWindows11())
		{
			return;
		}

		const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
		if ((lvExStyle & LVS_EX_CHECKBOXES) != LVS_EX_CHECKBOXES)
		{
			return;
		}

		HDC hdc = ::GetDC(nullptr);

		const bool useDark = umbra::isExperimentalActive() && umbra::isThemeDark();
		HTHEME hTheme = ::OpenThemeData(nullptr, useDark ? L"DarkMode_Explorer::Button" : VSCLASS_BUTTON);

		SIZE szBox{};
		::GetThemePartSize(hTheme, hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_DRAW, &szBox);

		const RECT rcBox{ 0, 0, szBox.cx, szBox.cy };

		auto hImgList = ListView_GetImageList(hWnd, LVSIL_STATE);
		if (hImgList == nullptr)
		{
			::CloseThemeData(hTheme);
			::ReleaseDC(nullptr, hdc);
			return;
		}
		::ImageList_RemoveAll(hImgList);

		HDC hBoxDC = ::CreateCompatibleDC(hdc);
		HBITMAP hBoxBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);
		HBITMAP hMaskBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);

		auto holdBmp = static_cast<HBITMAP>(::SelectObject(hBoxDC, hBoxBmp));
		::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &rcBox, nullptr);

		ICONINFO ii{};
		ii.fIcon = TRUE;
		ii.hbmColor = hBoxBmp;
		ii.hbmMask = hMaskBmp;

		HICON hIcon = ::CreateIconIndirect(&ii);
		if (hIcon != nullptr)
		{
			::ImageList_AddIcon(hImgList, hIcon);
			::DestroyIcon(hIcon);
			hIcon = nullptr;
		}

		::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, CBS_CHECKEDNORMAL, &rcBox, nullptr);
		ii.hbmColor = hBoxBmp;

		hIcon = ::CreateIconIndirect(&ii);
		if (hIcon != nullptr)
		{
			::ImageList_AddIcon(hImgList, hIcon);
			::DestroyIcon(hIcon);
			hIcon = nullptr;
		}

		::SelectObject(hBoxDC, holdBmp);
		::DeleteObject(hMaskBmp);
		::DeleteObject(hBoxBmp);
		::DeleteDC(hBoxDC);
		::CloseThemeData(hTheme);
		::ReleaseDC(nullptr, hdc);
	}

	/**
	 * @brief Sets colors and edges for a RichEdit control.
	 *
	 * Determines if the control has `WS_BORDER` or `WS_EX_STATICEDGE`, and sets the background
	 * accordingly: uses control background color when edged, otherwise dialog background.
	 *
	 * In dark mode:
	 * - Sets background color via `EM_SETBKGNDCOLOR`
	 * - Updates default text color via `EM_SETCHARFORMAT`
	 * - Applies themed scroll bars using `DarkMode_Explorer::ScrollBar`
	 *
	 * When not in dark mode, restores default visual styles and coloring.
	 * Also conditionally swaps `WS_BORDER` and `WS_EX_STATICEDGE`.
	 *
	 * @param hWnd Handle to the RichEdit control.
	 *
	 * @see umbra::setWindowStyle()
	 * @see umbra::setWindowExStyle()
	 */
	void setDarkRichEdit(HWND hWnd)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasBorder = (nStyle & WS_BORDER) == WS_BORDER;

		const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		const bool hasStaticEdge = (nExStyle & WS_EX_STATICEDGE) == WS_EX_STATICEDGE;

		if (umbra::isEnabled())
		{
			const COLORREF clrBg = (hasStaticEdge || hasBorder ? umbra::getCtrlBackgroundColor() : umbra::getDlgBackgroundColor());
			::SendMessage(hWnd, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(clrBg));

			CHARFORMATW cf{};
			cf.cbSize = sizeof(CHARFORMATW);
			cf.dwMask = CFM_COLOR;
			cf.crTextColor = umbra::getTextColor();
			::SendMessage(hWnd, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

			::SetWindowTheme(hWnd, nullptr, L"DarkMode_Explorer::ScrollBar");
		}
		else
		{
			::SendMessage(hWnd, EM_SETBKGNDCOLOR, TRUE, 0);

			CHARFORMATW cf{};
			cf.cbSize = sizeof(CHARFORMATW);
			cf.dwMask = CFM_COLOR;
			cf.dwEffects = CFE_AUTOCOLOR;
			::SendMessage(hWnd, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

			::SetWindowTheme(hWnd, nullptr, nullptr);
		}

		umbra::setWindowStyle(hWnd, umbra::isEnabled() && hasStaticEdge, WS_BORDER);
		umbra::setWindowExStyle(hWnd, !umbra::isEnabled() && hasBorder, WS_EX_STATICEDGE);
	}

	/**
	 * @brief Applies visual styles; ctl color message and child controls subclassings to a window safely.
	 *
	 * Ensures the specified window is not `nullptr` and then:
	 * - Enables the dark title bar
	 * - Subclasses the window for control ctl coloring
	 * - Applies theming and subclassing to child controls
	 *
	 *
	 * @param hWnd Handle to the window. No action taken if `nullptr`.
	 * @param useWin11Features `true` to enable Windows 11 specific styling like Mica or rounded corners.
	 *
	 * @note Should not be used in combination with @ref umbra::setDarkWndNotifySafeEx
	 *       and @ref umbra::setDarkWndNotifySafe to avoid overlapping styling logic.
	 *
	 * @see umbra::setDarkWndNotifySafeEx()
	 * @see umbra::setDarkWndNotifySafe()
	 * @see umbra::setDarkTitleBarEx()
	 * @see umbra::setWindowCtlColorSubclass()
	 * @see umbra::setChildCtrlsSubclassAndTheme()
	 */
	void setDarkWndSafe(HWND hWnd, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		umbra::setDarkTitleBarEx(hWnd, useWin11Features);
		umbra::setWindowCtlColorSubclass(hWnd);
		umbra::setChildCtrlsSubclassAndTheme(hWnd);
	}

	/**
	 * @brief Applies visual styles; ctl color message, child controls, custom drawing, and setting change subclassings to a window safely.
	 *
	 * Ensures the specified window is not `nullptr` and then:
	 * - Enables the dark title bar
	 * - Subclasses the window for control coloring
	 * - Applies theming and subclassing to child controls
	 * - Enables custom draw-based theming via notification subclassing
	 * - Subclasses the window to handle dark mode change if window mode is enabled.
	 *
	 * @param hWnd Handle to the window. No action taken if `nullptr`.
	 * @param setSettingChangeSubclass `true` to set setting change subclass if applicable.
	 * @param useWin11Features `true` to enable Windows 11 specific styling like Mica or rounded corners.
	 *
	 * @note `setSettingChangeSubclass = true` should be used only on main window.
	 *       For other secondary windows and controls use @ref umbra::setDarkWndNotifySafe.
	 *       Should not be used in combination with @ref umbra::setDarkWndSafe
	 *       and @ref umbra::setDarkWndNotifySafe to avoid overlapping styling logic.
	 *
	 * @see umbra::setDarkWndNotifySafe()
	 * @see umbra::setDarkWndSafe()
	 * @see umbra::setDarkTitleBarEx()
	 * @see umbra::setWindowCtlColorSubclass()
	 * @see umbra::setWindowNotifyCustomDrawSubclass()
	 * @see umbra::setChildCtrlsSubclassAndTheme()
	 * @see umbra::isWindowsModeEnabled()
	 * @see umbra::setWindowSettingChangeSubclass()
	 */
	void setDarkWndNotifySafeEx(HWND hWnd, bool setSettingChangeSubclass, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		umbra::setDarkTitleBarEx(hWnd, useWin11Features);
		umbra::setWindowCtlColorSubclass(hWnd);
		umbra::setWindowNotifyCustomDrawSubclass(hWnd);
		umbra::setChildCtrlsSubclassAndTheme(hWnd);
		if (setSettingChangeSubclass && umbra::isWindowsModeEnabled())
		{
			umbra::setWindowSettingChangeSubclass(hWnd);
		}
	}

	/**
	 * @brief Applies visual styles; ctl color message, child controls, and custom drawing subclassings to a window safely.
	 *
	 * Calls @ref umbra::setDarkWndNotifySafeEx with `setSettingChangeSubclass = false`, streamlining
	 * dark mode setup for secondary or transient windows that don't need to track system dark mode changes.
	 *
	 * @param hWnd Handle to the target window.
	 * @param useWin11Features Enable Windows 11-specific visual effects (e.g., Mica, rounded corners).
	 *
	 * @note Should not be used in combination with @ref umbra::setDarkWndSafe
	 *       and @ref umbra::setDarkWndNotifySafeEx to avoid overlapping styling logic.
	 *
	 * @see umbra::setDarkWndNotifySafeEx()
	 * @see umbra::setDarkWndSafe()
	 */
	void setDarkWndNotifySafe(HWND hWnd, bool useWin11Features)
	{
		umbra::setDarkWndNotifySafeEx(hWnd, false, useWin11Features);
	}

	/**
	 * @brief Enables or disables theme-based dialog background textures in classic mode.
	 *
	 * Applies `ETDT_ENABLETAB` only when `theme` is `true` and the current mode is classic.
	 * This replaces the default classic gray background with a lighter themed texture.
	 * Otherwise disables themed dialog textures with `ETDT_DISABLE`.
	 *
	 * @param hWnd Handle to the target dialog window.
	 * @param theme `true` to enable themed tab textures in classic mode.
	 *
	 * @see EnableThemeDialogTexture
	 */
	void enableThemeDialogTexture(HWND hWnd, bool theme)
	{
		::EnableThemeDialogTexture(hWnd, theme && (g_dmCfg._dmType == DarkModeType::classic) ? ETDT_ENABLETAB : ETDT_DISABLE);
	}

	/**
	 * @brief Enables or disables visual styles for a window.
	 *
	 * Applies `SetWindowTheme(hWnd, L"", L"")` when `doDisable` is `true`, effectively removing
	 * the current theme. Restores default theming when `doDisable` is `false`.
	 *
	 * @param hWnd Handle to the window.
	 * @param doDisable `true` to strip visual styles, `false` to re-enable them.
	 *
	 * @see SetWindowTheme
	 */
	void disableVisualStyle(HWND hWnd, bool doDisable)
	{
		if (doDisable)
		{
			::SetWindowTheme(hWnd, L"", L"");
		}
		else
		{
			::SetWindowTheme(hWnd, nullptr, nullptr);
		}
	}

	/**
	 * @brief Calculates perceptual lightness of a COLORREF color.
	 *
	 * Converts the RGB color to linear space and calculates perceived lightness.
	 *
	 * @param clr COLORREF in 0xBBGGRR format.
	 * @return Lightness value as a double.
	 *
	 * @note Based on: https://stackoverflow.com/a/56678483
	 */
	double calculatePerceivedLightness(COLORREF clr) noexcept
	{
		auto linearValue = [](double colorChannel) -> double {
			colorChannel /= 255.0;

			static constexpr double treshhold = 0.04045;
			static constexpr double lowScalingFactor = 12.92;
			static constexpr double gammaOffset = 0.055;
			static constexpr double gammaScalingFactor = 1.055;
			static constexpr double gammaExp = 2.4;

			if (colorChannel <= treshhold)
			{
				return colorChannel / lowScalingFactor;
			}
			return std::pow(((colorChannel + gammaOffset) / gammaScalingFactor), gammaExp);
		};

		const double r = linearValue(static_cast<double>(GetRValue(clr)));
		const double g = linearValue(static_cast<double>(GetGValue(clr)));
		const double b = linearValue(static_cast<double>(GetBValue(clr)));

		static constexpr double rWeight = 0.2126;
		static constexpr double gWeight = 0.7152;
		static constexpr double bWeight = 0.0722;

		const double luminance = (rWeight * r) + (gWeight * g) + (bWeight * b);

		static constexpr double cieEpsilon = 216.0 / 24389.0;
		static constexpr double cieKappa = 24389.0 / 27.0;
		static constexpr double oneThird = 1.0 / 3.0;
		static constexpr double scalingFactor = 116.0;
		static constexpr double offset = 16.0;

		// calculate lightness

		if (luminance <= cieEpsilon)
		{
			return (luminance * cieKappa);
		}
		return ((std::pow(luminance, oneThird) * scalingFactor) - offset);
	}

	/**
	 * @brief Retrieves the current TreeView style configuration.
	 *
	 * @return Reference to the current `TreeViewStyle`.
	 */
	const TreeViewStyle& getTreeViewStyle() noexcept
	{
		return g_dmCfg._tvStyle;
	}

	/// Set TreeView style
	static void setTreeViewStyle(TreeViewStyle tvStyle) noexcept
	{
		g_dmCfg._tvStyle = tvStyle;
	}

	/**
	 * @brief Determines appropriate TreeView style based on background perceived lightness.
	 *
	 * Checks the perceived lightness of the current view background and
	 * selects a corresponding style: dark, light, or classic. Style selection
	 * is based on how far the lightness deviates from the middle gray threshold range
	 * around the midpoint value (50.0).
	 *
	 * @see umbra::calculatePerceivedLightness()
	 */
	void calculateTreeViewStyle()
	{
		static constexpr double middle = 50.0;
		const COLORREF bgColor = umbra::getViewBackgroundColor();

		if (g_dmCfg._tvBackground != bgColor || g_dmCfg._lightness == middle)
		{
			g_dmCfg._lightness = umbra::calculatePerceivedLightness(bgColor);
			g_dmCfg._tvBackground = bgColor;
		}

		if (g_dmCfg._lightness < (middle - kMiddleGrayRange))
		{
			umbra::setTreeViewStyle(TreeViewStyle::dark);
		}
		else if (g_dmCfg._lightness > (middle + kMiddleGrayRange))
		{
			umbra::setTreeViewStyle(TreeViewStyle::light);
		}
		else
		{
			umbra::setTreeViewStyle(TreeViewStyle::classic);
		}
	}

	/**
	 * @brief Applies the appropriate window theme style to the specified TreeView.
	 *
	 * Updates the TreeView's visual behavior and theme based on the currently selected
	 * style @ref umbra::getTreeViewStyle. It conditionally adjusts the `TVS_TRACKSELECT`
	 * style flag and applies a matching visual theme using `SetWindowTheme()`.
	 *
	 * If `force` is `true`, the style is applied regardless of previous state.
	 * Otherwise, the update occurs only if the style has changed since the last update.
	 *
	 * - `light`: Enables `TVS_TRACKSELECT`, applies "Explorer" theme.
	 * - `dark`: If supported, enables `TVS_TRACKSELECT`, applies "DarkMode_Explorer" theme.
	 * - `classic`: Disables `TVS_TRACKSELECT`, clears the theme.
	 *
	 * @param hWnd Handle to the TreeView control.
	 * @param force Whether to forcibly reapply the style even if unchanged.
	 *
	 * @see TreeViewStyle
	 * @see umbra::getTreeViewStyle()
	 * @see umbra::getPrevTreeViewStyle()
	 */
	void setTreeViewWindowTheme(HWND hWnd, bool force)
	{
		if (force || umbra::getPrevTreeViewStyle() != umbra::getTreeViewStyle())
		{
			auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			const bool hasHotStyle = (nStyle & TVS_TRACKSELECT) == TVS_TRACKSELECT;
			bool change = false;
			std::wstring strSubAppName;

			switch (umbra::getTreeViewStyle())
			{
				case TreeViewStyle::light:
				{
					if (!hasHotStyle)
					{
						nStyle |= TVS_TRACKSELECT;
						change = true;
					}
					strSubAppName = L"Explorer";
					break;
				}

				case TreeViewStyle::dark:
				{
					if (umbra::isExperimentalSupported())
					{
						if (!hasHotStyle)
						{
							nStyle |= TVS_TRACKSELECT;
							change = true;
						}
						strSubAppName = L"DarkMode_Explorer";
						break;
					}
					[[fallthrough]];
				}

				case TreeViewStyle::classic:
				{
					if (hasHotStyle)
					{
						nStyle &= ~TVS_TRACKSELECT;
						change = true;
					}
					strSubAppName = L"";
					break;
				}
			}

			if (change)
			{
				::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle);
			}

			::SetWindowTheme(hWnd, strSubAppName.empty() ? nullptr : strSubAppName.c_str(), nullptr);
		}
	}

	/**
	 * @brief Retrieves the previous TreeView style configuration.
	 *
	 * @return Reference to the previous `TreeViewStyle`.
	 */
	const TreeViewStyle& getPrevTreeViewStyle() noexcept
	{
		return g_dmCfg._tvStylePrev;
	}

	/**
	 * @brief Stores the current TreeView style as the previous style for later comparison.
	 */
	void setPrevTreeViewStyle() noexcept
	{
		g_dmCfg._tvStylePrev = umbra::getTreeViewStyle();
	}

	/**
	 * @brief Checks whether the current theme is dark.
	 *
	 * Internally it use TreeView style to determine if dark theme is used.
	 *
	 * @return `true` if the active style is `TreeViewStyle::dark`, otherwise `false`.
	 *
	 * @see umbra::getTreeViewStyle()
	 */
	bool isThemeDark() noexcept
	{
		return umbra::getTreeViewStyle() == TreeViewStyle::dark;
	}

	/**
	 * @brief Checks whether the color is dark.
	 *
	 * @param clr Color to check.
	 *
	 * @return `true` if the perceived lightness of the color
	 *         is less than (50.0 - kMiddleGrayRange), otherwise `false`.
	 *
	 * @see umbra::calculatePerceivedLightness()
	 */
	bool isColorDark(COLORREF clr) noexcept
	{
		static constexpr double middle = 50.0;
		return umbra::calculatePerceivedLightness(clr) < (middle - kMiddleGrayRange);
	}

	/**
	 * @brief Forces a window to redraw its non-client frame.
	 *
	 * Triggers a non-client area update by using `SWP_FRAMECHANGED` without changing
	 * size, position, or Z-order.
	 *
	 * @param hWnd Handle to the target window.
	 */
	void redrawWindowFrame(HWND hWnd)
	{
		::SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	/**
	 * @brief Sets or clears a specific window style or extended style.
	 *
	 * Checks if the specified `dwFlag` is already set and toggles it if needed.
	 * Only valid for `GWL_STYLE` or `GWL_EXSTYLE`.
	 *
	 * @param hWnd Handle to the window.
	 * @param setFlag `true` to set the flag, `false` to clear it.
	 * @param dwFlag Style bitmask to apply.
	 * @param gwlIdx Either `GWL_STYLE` or `GWL_EXSTYLE`.
	 * @return `TRUE` if modified, `FALSE` if unchanged, `-1` if invalid index.
	 */
	static int setWindowLongPtrStyle(HWND hWnd, bool setFlag, LONG_PTR dwFlag, int gwlIdx)
	{
		if ((gwlIdx != GWL_STYLE) && (gwlIdx != GWL_EXSTYLE))
		{
			return -1;
		}

		auto nStyle = ::GetWindowLongPtr(hWnd, gwlIdx);
		const bool hasFlag = (nStyle & dwFlag) == dwFlag;

		if (setFlag != hasFlag)
		{
			nStyle ^= dwFlag;
			::SetWindowLongPtr(hWnd, gwlIdx, nStyle);
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * @brief Sets a window's standard style flags and redraws window if needed.
	 *
	 * Wraps @ref umbra::setWindowLongPtrStyle with `GWL_STYLE`
	 * and calls @ref umbra::redrawWindowFrame if a change occurs.
	 *
	 * @param hWnd Handle to the target window.
	 * @param setStyle `true` to set the flag, `false` to remove it.
	 * @param styleFlag Style bit to modify.
	 */
	void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag)
	{
		if (umbra::setWindowLongPtrStyle(hWnd, setStyle, styleFlag, GWL_STYLE) == TRUE)
		{
			umbra::redrawWindowFrame(hWnd);
		}
	}

	/**
	 * @brief Sets a window's extended style flags and redraws window if needed.
	 *
	 * Wraps @ref umbra::setWindowLongPtrStyle with `GWL_EXSTYLE`
	 * and calls @ref umbra::redrawWindowFrame if a change occurs.
	 *
	 * @param hWnd Handle to the target window.
	 * @param setExStyle `true` to set the flag, `false` to remove it.
	 * @param exStyleFlag Extended style bit to modify.
	 */
	void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag)
	{
		if (umbra::setWindowLongPtrStyle(hWnd, setExStyle, exStyleFlag, GWL_EXSTYLE) == TRUE)
		{
			umbra::redrawWindowFrame(hWnd);
		}
	}

	/**
	 * @brief Replaces an extended edge (e.g. client edge) with a standard window border.
	 *
	 * The given `exStyleFlag` must be a valid edge-related extended window style:
	 * - `WS_EX_CLIENTEDGE`
	 * - `WS_EX_DLGMODALFRAME`
	 * - `WS_EX_STATICEDGE`
	 * - `WS_EX_WINDOWEDGE`
	 * ...or any combination of these.
	 *
	 * If `replace` is `true`, the specified extended edge style(s) are removed and
	 * `WS_BORDER` is applied. If `false`, the edge style(s) are restored and `WS_BORDER` is cleared.
	 *
	 * @param hWnd Handle to the target window.
	 * @param replace `true` to apply standard border; `false` to restore extended edge(s).
	 * @param exStyleFlag One or more valid edge-related extended styles.
	 *
	 * @see umbra::setWindowExStyle()
	 * @see umbra::setWindowStyle()
	 */
	void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag)
	{
		umbra::setWindowExStyle(hWnd, !replace, exStyleFlag);
		umbra::setWindowStyle(hWnd, replace, WS_BORDER);
	}

	/**
	 * @brief Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER` based on dark mode state.
	 *
	 * If dark mode is enabled, removes `WS_EX_CLIENTEDGE` and applies `WS_BORDER`.
	 * Otherwise restores the extended edge style.
	 *
	 * @param hWnd Handle to the target window. No action is taken if `hWnd` is `nullptr`.
	 *
	 * @see umbra::replaceExEdgeWithBorder()
	 */
	void replaceClientEdgeWithBorderSafe(HWND hWnd)
	{
		if (hWnd != nullptr)
		{
			umbra::replaceExEdgeWithBorder(hWnd, umbra::isEnabled(), WS_EX_CLIENTEDGE);
		}
	}

	/**
	 * @brief Applies classic-themed styling to a progress bar in non-classic mode.
	 *
	 * When dark mode is enabled, applies `WS_DLGFRAME`, removes visual styles
	 * to allow to set custom background and fill colors using:
	 * - Background: `umbra::getBackgroundColor()`
	 * - Fill: Hardcoded green `0x06B025` via `PBM_SETBARCOLOR`
	 *
	 * Typically used for marquee style progress bar.
	 *
	 * @param hWnd Handle to the progress bar control.
	 *
	 * @see umbra::setWindowStyle()
	 * @see umbra::disableVisualStyle()
	 */
	void setProgressBarClassicTheme(HWND hWnd)
	{
		umbra::setWindowStyle(hWnd, umbra::isEnabled(), WS_DLGFRAME);
		umbra::disableVisualStyle(hWnd, umbra::isEnabled());
		if (umbra::isEnabled())
		{
			::SendMessage(hWnd, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(umbra::getCtrlBackgroundColor()));
			static constexpr COLORREF greenLight = HEXRGB(0x06B025);
			static constexpr COLORREF greenDark = HEXRGB(0x0F7B0F);
			::SendMessage(hWnd, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(umbra::isExperimentalActive() ? greenDark : greenLight));
		}
	}

	/**
	 * @brief Handles text and background colorizing for read-only controls.
	 *
	 * Sets the text color and background color on the provided HDC.
	 * Returns the corresponding background brush for painting.
	 * Typically used for read-only controls (e.g. edit control and combo box' list box).
	 * Typically used in response to `WM_CTLCOLORSTATIC` or in `WM_CTLCOLORLISTBOX`
	 * via @ref umbra::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context (HDC) receiving the drawing instructions.
	 * @return Background brush to use for painting, or `FALSE` (0) if classic mode is enabled
	 *         and `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::onCtlColorListbox()
	 */
	LRESULT onCtlColor(HDC hdc)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			return FALSE;
		}
#endif
		::SetTextColor(hdc, umbra::getTextColor());
		::SetBkColor(hdc, umbra::getBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for interactive controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLOREDIT` and `WM_CTLCOLORLISTBOX`
	 * via @ref umbra::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::onCtlColorListbox()
	 */
	LRESULT onCtlColorCtrl(HDC hdc)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, umbra::getTextColor());
		::SetBkColor(hdc, umbra::getCtrlBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getCtrlBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for window and disabled non-text controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
	 * and `WM_CTLCOLORLISTBOX` via @ref umbra::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::onCtlColorListbox()
	 */
	LRESULT onCtlColorDlg(HDC hdc)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, umbra::getTextColor());
		::SetBkColor(hdc, umbra::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for error state (for specific usage).
	 *
	 * Sets the text and background colors on the provided HDC.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorError(HDC hdc)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, umbra::getTextColor());
		::SetBkColor(hdc, umbra::getErrorBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getErrorBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for static text controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Colors depend on if control is enabled.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORSTATIC`.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
			return FALSE;
		}
#endif
		::SetTextColor(hdc, isTextEnabled ? umbra::getTextColor() : umbra::getDisabledTextColor());
		::SetBkColor(hdc, umbra::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for syslink controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Colors depend on if control is enabled.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORSTATIC`.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled)
	{
#if defined(_DARKMODE_DLG_PROC_CTLCOLOR_RETURNS)
		if (!umbra::_isEnabled())
		{
			::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_HOTLIGHT : COLOR_GRAYTEXT));
			return FALSE;
		}
#endif
		::SetTextColor(hdc, isTextEnabled ? umbra::getLinkTextColor() : umbra::getDisabledTextColor());
		::SetBkColor(hdc, umbra::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(umbra::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for list box controls.
	 *
	 * Inspects the list box style flags to detect if it's part of a combo box (via `LBS_COMBOBOX`)
	 * and whether experimental feature is active. Based on the context, delegates to:
	 * - @ref umbra::onCtlColorCtrl for standard enabled listboxes
	 * - @ref umbra::onCtlColorDlg for disabled ones or when dark mode is disabled
	 * - @ref umbra::onCtlColor for combo box' listbox
	 *
	 * @param wParam WPARAM from `WM_CTLCOLORLISTBOX`, representing the HDC.
	 * @param lParam LPARAM from `WM_CTLCOLORLISTBOX`, representing the HWND of the listbox.
	 * @return The brush handle as LRESULT for background painting, or `FALSE` if not themed.
	 *
	 * @see umbra::WindowCtlColorSubclass()
	 * @see umbra::onCtlColor()
	 * @see umbra::onCtlColorCtrl()
	 * @see umbra::onCtlColorDlg()
	 */
	LRESULT onCtlColorListbox(WPARAM wParam, LPARAM lParam)
	{
		auto hdc = reinterpret_cast<HDC>(wParam);
		auto hWnd = reinterpret_cast<HWND>(lParam);

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isComboBox = (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;
		if ((!isComboBox || !umbra::isExperimentalActive()))
		{
			if (::IsWindowEnabled(hWnd) == TRUE)
			{
				return umbra::onCtlColorCtrl(hdc);
			}
			return umbra::onCtlColorDlg(hdc);
		}
		return umbra::onCtlColor(hdc);
	}

	/**
	 * @brief Hook procedure for customizing common dialogs with dark mode.
	 */
	UINT_PTR CALLBACK HookDlgProc(HWND hWnd, UINT uMsg, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam)
	{
		if (uMsg == WM_INITDIALOG)
		{
			umbra::setDarkWndSafe(hWnd);
			return TRUE;
		}
		return FALSE;
	}
} // namespace umbra

#endif // !defined(_DARKMODE_NOT_USED)
