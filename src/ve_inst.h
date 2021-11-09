#ifndef VE_INST_INCLUDE
#define VE_INST_INCLUDE

/**
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * VE instructions as inline assembly.
 * 
 * Copyright (c) 2020 Erich Focht
 */

#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __ve__
static inline void ve_inst_fenceSF(void)
{
    asm volatile ("fencem 1":::"memory");
}

static inline void ve_inst_fenceLF(void)
{
    asm volatile ("fencem 2":::"memory");
}

static inline void ve_inst_fenceLSF(void)
{
    asm volatile ("fencem 3":::);
}

static inline void ve_inst_fenceC1(void)
{
    asm volatile ("fencec 1":::);
}

static inline void ve_inst_fenceC2(void)
{
    asm volatile ("fencec 2":::);
}

static inline void ve_inst_fenceC4(void)
{
    asm volatile ("fencec 4":::);
}

static inline void ve_inst_fenceC7(void)
{
    asm volatile ("fencec 7":::);
}

static inline uint64_t ve_inst_lhm(void *vehva)
{
    uint64_t volatile ret;
    asm volatile ("lhm.l %0,0(%1)":"=r"(ret):"r"(vehva));
    return ret;
}

static inline uint32_t ve_inst_lhm32(void *vehva)
{
    uint32_t volatile ret;
    asm volatile ("lhm.w %0,0(%1)":"=r"(ret):"r"(vehva));
    return ret;
}

static inline void ve_inst_shm(void *vehva, uint64_t value)
{
    asm volatile ("shm.l %0,0(%1)"::"r"(value),"r"(vehva));
}

static inline void ve_inst_shm32(void *vehva, uint32_t value)
{
    asm volatile ("shm.w %0,0(%1)"::"r"(value),"r"(vehva));
}

#endif
#ifdef __cplusplus
}
#endif

#endif // VE_INST_INCLUDE
