/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* This file was automatically generated --- DO NOT EDIT */
/* Generated on Fri Jan 27 16:11:30 EST 2017 */

#include "codelet-dft.h"

#ifdef HAVE_FMA

/* Generated by: ../../../genfft/gen_twiddle_c.native -fma -reorder-insns -schedule-for-pipeline -simd -compact -variables 4 -pipeline-latency 8 -n 8 -name t2bv_8 -include t2b.h -sign 1 */

/*
 * This function contains 33 FP additions, 24 FP multiplications,
 * (or, 23 additions, 14 multiplications, 10 fused multiply/add),
 * 36 stack variables, 1 constants, and 16 memory accesses
 */
#include "t2b.h"

static void t2bv_8(R *ri, R *ii, const R *W, stride rs, INT mb, INT me, INT ms)
{
     DVK(KP707106781, +0.707106781186547524400844362104849039284835938);
     {
	  INT m;
	  R *x;
	  x = ii;
	  for (m = mb, W = W + (mb * ((TWVL / VL) * 14)); m < me; m = m + VL, x = x + (VL * ms), W = W + (TWVL * 14), MAKE_VOLATILE_STRIDE(8, rs)) {
	       V T1, T2, Th, Tj, T5, T7, Ta, Tc;
	       T1 = LD(&(x[0]), ms, &(x[0]));
	       T2 = LD(&(x[WS(rs, 4)]), ms, &(x[0]));
	       Th = LD(&(x[WS(rs, 2)]), ms, &(x[0]));
	       Tj = LD(&(x[WS(rs, 6)]), ms, &(x[0]));
	       T5 = LD(&(x[WS(rs, 1)]), ms, &(x[WS(rs, 1)]));
	       T7 = LD(&(x[WS(rs, 5)]), ms, &(x[WS(rs, 1)]));
	       Ta = LD(&(x[WS(rs, 7)]), ms, &(x[WS(rs, 1)]));
	       Tc = LD(&(x[WS(rs, 3)]), ms, &(x[WS(rs, 1)]));
	       {
		    V T3, Ti, Tk, T6, T8, Tb, Td;
		    T3 = BYTW(&(W[TWVL * 6]), T2);
		    Ti = BYTW(&(W[TWVL * 2]), Th);
		    Tk = BYTW(&(W[TWVL * 10]), Tj);
		    T6 = BYTW(&(W[0]), T5);
		    T8 = BYTW(&(W[TWVL * 8]), T7);
		    Tb = BYTW(&(W[TWVL * 12]), Ta);
		    Td = BYTW(&(W[TWVL * 4]), Tc);
		    {
			 V Tq, T4, Tr, Tl, Tt, T9, Tu, Te, Tw, Ts;
			 Tq = VADD(T1, T3);
			 T4 = VSUB(T1, T3);
			 Tr = VADD(Ti, Tk);
			 Tl = VSUB(Ti, Tk);
			 Tt = VADD(T6, T8);
			 T9 = VSUB(T6, T8);
			 Tu = VADD(Tb, Td);
			 Te = VSUB(Tb, Td);
			 Tw = VADD(Tq, Tr);
			 Ts = VSUB(Tq, Tr);
			 {
			      V Tx, Tv, Tm, Tf;
			      Tx = VADD(Tt, Tu);
			      Tv = VSUB(Tt, Tu);
			      Tm = VSUB(T9, Te);
			      Tf = VADD(T9, Te);
			      {
				   V Tp, Tn, To, Tg;
				   ST(&(x[0]), VADD(Tw, Tx), ms, &(x[0]));
				   ST(&(x[WS(rs, 4)]), VSUB(Tw, Tx), ms, &(x[0]));
				   ST(&(x[WS(rs, 2)]), VFMAI(Tv, Ts), ms, &(x[0]));
				   ST(&(x[WS(rs, 6)]), VFNMSI(Tv, Ts), ms, &(x[0]));
				   Tp = VFMA(LDK(KP707106781), Tm, Tl);
				   Tn = VFNMS(LDK(KP707106781), Tm, Tl);
				   To = VFMA(LDK(KP707106781), Tf, T4);
				   Tg = VFNMS(LDK(KP707106781), Tf, T4);
				   ST(&(x[WS(rs, 1)]), VFMAI(Tp, To), ms, &(x[WS(rs, 1)]));
				   ST(&(x[WS(rs, 7)]), VFNMSI(Tp, To), ms, &(x[WS(rs, 1)]));
				   ST(&(x[WS(rs, 5)]), VFMAI(Tn, Tg), ms, &(x[WS(rs, 1)]));
				   ST(&(x[WS(rs, 3)]), VFNMSI(Tn, Tg), ms, &(x[WS(rs, 1)]));
			      }
			 }
		    }
	       }
	  }
     }
     VLEAVE();
}

static const tw_instr twinstr[] = {
     VTW(0, 1),
     VTW(0, 2),
     VTW(0, 3),
     VTW(0, 4),
     VTW(0, 5),
     VTW(0, 6),
     VTW(0, 7),
     {TW_NEXT, VL, 0}
};

static const ct_desc desc = { 8, XSIMD_STRING("t2bv_8"), twinstr, &GENUS, {23, 14, 10, 0}, 0, 0, 0 };

void XSIMD(codelet_t2bv_8) (planner *p) {
     X(kdft_dit_register) (p, t2bv_8, &desc);
}
#else				/* HAVE_FMA */

/* Generated by: ../../../genfft/gen_twiddle_c.native -simd -compact -variables 4 -pipeline-latency 8 -n 8 -name t2bv_8 -include t2b.h -sign 1 */

/*
 * This function contains 33 FP additions, 16 FP multiplications,
 * (or, 33 additions, 16 multiplications, 0 fused multiply/add),
 * 24 stack variables, 1 constants, and 16 memory accesses
 */
#include "t2b.h"

static void t2bv_8(R *ri, R *ii, const R *W, stride rs, INT mb, INT me, INT ms)
{
     DVK(KP707106781, +0.707106781186547524400844362104849039284835938);
     {
	  INT m;
	  R *x;
	  x = ii;
	  for (m = mb, W = W + (mb * ((TWVL / VL) * 14)); m < me; m = m + VL, x = x + (VL * ms), W = W + (TWVL * 14), MAKE_VOLATILE_STRIDE(8, rs)) {
	       V Tl, Tq, Tg, Tr, T5, Tt, Ta, Tu, Ti, Tk, Tj;
	       Ti = LD(&(x[0]), ms, &(x[0]));
	       Tj = LD(&(x[WS(rs, 4)]), ms, &(x[0]));
	       Tk = BYTW(&(W[TWVL * 6]), Tj);
	       Tl = VSUB(Ti, Tk);
	       Tq = VADD(Ti, Tk);
	       {
		    V Td, Tf, Tc, Te;
		    Tc = LD(&(x[WS(rs, 2)]), ms, &(x[0]));
		    Td = BYTW(&(W[TWVL * 2]), Tc);
		    Te = LD(&(x[WS(rs, 6)]), ms, &(x[0]));
		    Tf = BYTW(&(W[TWVL * 10]), Te);
		    Tg = VSUB(Td, Tf);
		    Tr = VADD(Td, Tf);
	       }
	       {
		    V T2, T4, T1, T3;
		    T1 = LD(&(x[WS(rs, 1)]), ms, &(x[WS(rs, 1)]));
		    T2 = BYTW(&(W[0]), T1);
		    T3 = LD(&(x[WS(rs, 5)]), ms, &(x[WS(rs, 1)]));
		    T4 = BYTW(&(W[TWVL * 8]), T3);
		    T5 = VSUB(T2, T4);
		    Tt = VADD(T2, T4);
	       }
	       {
		    V T7, T9, T6, T8;
		    T6 = LD(&(x[WS(rs, 7)]), ms, &(x[WS(rs, 1)]));
		    T7 = BYTW(&(W[TWVL * 12]), T6);
		    T8 = LD(&(x[WS(rs, 3)]), ms, &(x[WS(rs, 1)]));
		    T9 = BYTW(&(W[TWVL * 4]), T8);
		    Ta = VSUB(T7, T9);
		    Tu = VADD(T7, T9);
	       }
	       {
		    V Ts, Tv, Tw, Tx;
		    Ts = VSUB(Tq, Tr);
		    Tv = VBYI(VSUB(Tt, Tu));
		    ST(&(x[WS(rs, 6)]), VSUB(Ts, Tv), ms, &(x[0]));
		    ST(&(x[WS(rs, 2)]), VADD(Ts, Tv), ms, &(x[0]));
		    Tw = VADD(Tq, Tr);
		    Tx = VADD(Tt, Tu);
		    ST(&(x[WS(rs, 4)]), VSUB(Tw, Tx), ms, &(x[0]));
		    ST(&(x[0]), VADD(Tw, Tx), ms, &(x[0]));
		    {
			 V Th, To, Tn, Tp, Tb, Tm;
			 Tb = VMUL(LDK(KP707106781), VSUB(T5, Ta));
			 Th = VBYI(VSUB(Tb, Tg));
			 To = VBYI(VADD(Tg, Tb));
			 Tm = VMUL(LDK(KP707106781), VADD(T5, Ta));
			 Tn = VSUB(Tl, Tm);
			 Tp = VADD(Tl, Tm);
			 ST(&(x[WS(rs, 3)]), VADD(Th, Tn), ms, &(x[WS(rs, 1)]));
			 ST(&(x[WS(rs, 7)]), VSUB(Tp, To), ms, &(x[WS(rs, 1)]));
			 ST(&(x[WS(rs, 5)]), VSUB(Tn, Th), ms, &(x[WS(rs, 1)]));
			 ST(&(x[WS(rs, 1)]), VADD(To, Tp), ms, &(x[WS(rs, 1)]));
		    }
	       }
	  }
     }
     VLEAVE();
}

static const tw_instr twinstr[] = {
     VTW(0, 1),
     VTW(0, 2),
     VTW(0, 3),
     VTW(0, 4),
     VTW(0, 5),
     VTW(0, 6),
     VTW(0, 7),
     {TW_NEXT, VL, 0}
};

static const ct_desc desc = { 8, XSIMD_STRING("t2bv_8"), twinstr, &GENUS, {33, 16, 0, 0}, 0, 0, 0 };

void XSIMD(codelet_t2bv_8) (planner *p) {
     X(kdft_dit_register) (p, t2bv_8, &desc);
}
#endif				/* HAVE_FMA */
