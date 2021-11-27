// SPDX-FileCopyrightText: 2021 Daniel Vrátil <dvratil@kde.org>
//
// SPDX-License-Identifier: MIT

#pragma once


// vv workaround for QtCreator's clang code model warnings vv

// Bypass GCC / MSVC coroutine guards when using clang code model
#if !defined(__APPLE__) && !defined(__ANDROID__) && (defined(__GNUC__) || defined(_MSC_VER))
#if defined(__GNUC__) && defined(__clang__) && !defined(__cpp_impl_coroutine)
#define __cpp_impl_coroutine true
#elif defined(_MSC_VER) && defined(__clang__) && !defined(__cpp_lib_coroutine)
#define __cpp_lib_coroutine true
#endif
// Clang requires coroutine types in std::experimental
#include <coroutine>
#if defined(__clang__)
namespace std::experimental {
using std::coroutine_traits;
using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;
}
#endif
#endif

// ^^ workaround for QtCreator's clang code model warnings ^^

#include "task.h"
#include "qcorocore.h"
#if defined(QT_DBUS_LIB)
#  include "qcorodbus.h"
#endif
#include "qcoronetwork.h"
