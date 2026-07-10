/*
 * Copyright 2026 CodingMarkus
 *
 * SPDX-License-Identifier: ISC
 */

#ifndef WEBMINCER_JS_MANGLER_H
#define WEBMINCER_JS_MANGLER_H

#include "minifier.h"

struct Minification mangle_js_identifiers(const char *js, bool force_module);

#endif
