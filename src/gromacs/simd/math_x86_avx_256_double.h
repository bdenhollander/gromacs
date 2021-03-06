/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2012,2013,2014, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
#ifndef GMX_SIMD_MATH_AVX_256_DOUBLE_H
#define GMX_SIMD_MATH_AVX_256_DOUBLE_H

#include "simd_math.h"

/* Temporary:
 * Alias some old SSE definitions to new SIMD definitions so we don't need
 * to modify _all_ group kernels - they will anyway be replaced with a new
 * generic SIMD version soon.
 */

#define gmx_mm256_invsqrt_pd   gmx_simd_invsqrt_d
#define gmx_mm256_inv_pd       gmx_simd_inv_d
#define gmx_mm256_log_pd       gmx_simd_log_d
#define gmx_mm256_pmecorrF_pd  gmx_simd_pmecorrF_d
#define gmx_mm256_pmecorrV_pd  gmx_simd_pmecorrV_d
#define gmx_mm256_sincos_pd    gmx_simd_sincos_d

#endif
