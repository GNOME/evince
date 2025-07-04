/* sysprof-version.h
 *
 * Copyright 2016-2020 Christian Hergert <chergert@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Subject to the terms and conditions of this license, each copyright holder
 * and contributor hereby grants to those receiving rights under this license
 * a perpetual, worldwide, non-exclusive, no-charge, royalty-free,
 * irrevocable (except for failure to satisfy the conditions of this license)
 * patent license to make, have made, use, offer to sell, sell, import, and
 * otherwise transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable by such
 * copyright holder or contributor that are necessarily infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright holders
 *     and non-copyrightable additions of contributors, in source or binary
 *     form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of authorship to
 *     which such Contribution(s) was added by such copyright holder or
 *     contributor, if, at the time the Contribution is added, such addition
 *     causes such combination to be necessarily infringed. The patent license
 *     shall not apply to any other combinations which include the
 *     Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any copyright
 * holder or contributor is granted under this license, whether expressly, by
 * implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#pragma once

/**
 * SECTION:sysprof-version
 * @short_description: sysprof version checking
 *
 * sysprof provides macros to check the version of the library
 * at compile-time
 */

/**
 * SYSPROF_MAJOR_VERSION:
 *
 * sysprof major version component (e.g. 1 if %SYSPROF_VERSION is 1.2.3)
 */
#define SYSPROF_MAJOR_VERSION (49)

/**
 * SYSPROF_MINOR_VERSION:
 *
 * sysprof minor version component (e.g. 2 if %SYSPROF_VERSION is 1.2.3)
 */
#define SYSPROF_MINOR_VERSION (0)

/**
 * SYSPROF_MICRO_VERSION:
 *
 * sysprof micro version component (e.g. 3 if %SYSPROF_VERSION is 1.2.3)
 */
#define SYSPROF_MICRO_VERSION (0)

/**
 * SYSPROF_VERSION
 *
 * sysprof version.
 */
#define SYSPROF_VERSION (49.alpha)

/**
 * SYSPROF_VERSION_S:
 *
 * sysprof version, encoded as a string, useful for printing and
 * concatenation.
 */
#define SYSPROF_VERSION_S "49.alpha"

#define SYSPROF_ENCODE_VERSION(major,minor,micro) \
        ((major) << 24 | (minor) << 16 | (micro) << 8)

/**
 * SYSPROF_VERSION_HEX:
 *
 * sysprof version, encoded as an hexadecimal number, useful for
 * integer comparisons.
 */
#define SYSPROF_VERSION_HEX \
        (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION, SYSPROF_MINOR_VERSION, SYSPROF_MICRO_VERSION))

/**
 * SYSPROF_CHECK_VERSION:
 * @major: required major version
 * @minor: required minor version
 * @micro: required micro version
 *
 * Compile-time version checking. Evaluates to %TRUE if the version
 * of sysprof is greater than the required one.
 */
#define SYSPROF_CHECK_VERSION(major,minor,micro)   \
        (SYSPROF_MAJOR_VERSION > (major) || \
         (SYSPROF_MAJOR_VERSION == (major) && SYSPROF_MINOR_VERSION > (minor)) || \
         (SYSPROF_MAJOR_VERSION == (major) && SYSPROF_MINOR_VERSION == (minor) && \
          SYSPROF_MICRO_VERSION >= (micro)))

