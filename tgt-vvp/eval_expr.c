/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#if !defined(WINNT)
#ident "$Id: eval_expr.c,v 1.23 2001/05/02 01:57:25 steve Exp $"
#endif

# include  "vvp_priv.h"
# include  <assert.h>

struct vector_info draw_eval_expr_wid(ivl_expr_t exp, unsigned wid);

static unsigned char allocation_map[0x10000/8];

static inline int peek_bit(unsigned addr)
{
      unsigned bit = addr % 8;
      addr /= 8;
      return 1 & (allocation_map[addr] >> bit);
}

static inline void set_bit(unsigned addr)
{
      unsigned bit = addr % 8;
      addr /= 8;
      allocation_map[addr] |= (1 << bit);
}

static inline void clr_bit(unsigned addr)
{
      unsigned bit = addr % 8;
      addr /= 8;
      allocation_map[addr] &= ~(1 << bit);
}

/*
 * This clears a vector that was previously allocated by
 * allocate_vector. That is, it unmarks all the bits of the map that
 * represent this vector.
 *
 * If the vector is based in one of 4 constant bit values, then there
 * are no bits to clear. If the vector is based in the 4-8 result
 * area, then someone is broken.
 */
void clr_vector(struct vector_info vec)
{
      unsigned idx;
      if (vec.base < 4)
	    return;
      assert(vec.base >= 8);
      for (idx = 0 ;  idx < vec.wid ;  idx += 1)
	    clr_bit(vec.base + idx);
}

static unsigned short allocate_vector(unsigned short wid)
{
      unsigned short base = 8;

      unsigned short idx = 0;
      while (idx < wid) {
	    assert((base + idx) < 0x10000);
	    if (peek_bit(base+idx)) {
		  base = base + idx + 1;
		  idx = 0;

	    } else {
		  idx += 1;
	    }
      }

      for (idx = 0 ;  idx < wid ;  idx += 1)
	    set_bit(base+idx);

      return base;
}


static struct vector_info draw_binary_expr_eq(ivl_expr_t exp)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      unsigned wid = ivl_expr_width(le);
      if (ivl_expr_width(re) > wid)
	    wid = ivl_expr_width(re);

      lv = draw_eval_expr_wid(le, wid);
      rv = draw_eval_expr_wid(re, wid);

      switch (ivl_expr_opcode(exp)) {
	  case 'E': /* === */
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 6;
	    lv.wid = 1;
	    break;

	  case 'e': /* == */
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 4;
	    lv.wid = 1;
	    break;

	  case 'N': /* !== */
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    fprintf(vvp_out, "    %%inv 6, 1;\n");

	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 6;
	    lv.wid = 1;
	    break;

	  case 'n': /* != */
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    fprintf(vvp_out, "    %%inv 4, 1;\n");

	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 4;
	    lv.wid = 1;
	    break;

	  default:
	    assert(0);
      }

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      { unsigned short base = allocate_vector(1);
        fprintf(vvp_out, "    %%mov %u, %u, 1;\n", base, lv.base);
	lv.base = base;
	lv.wid = 1;
      }

      return lv;
}

static struct vector_info draw_binary_expr_land(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

	/* XXXX For now, assume the operands are a single bit. */
      assert(ivl_expr_width(le) == 1);
      assert(ivl_expr_width(re) == 1);

      lv = draw_eval_expr_wid(le, wid);
      rv = draw_eval_expr_wid(re, wid);

      if (lv.base < 4) {
	    if (rv.base < 4) {
		  unsigned lb = lv.base;
		  unsigned rb = rv.base;

		  if ((lb == 0) || (rb == 0)) {
			lv.base = 0;

		  } else if ((lb == 1) && (rb == 1)) {
			lv.base = 1;
		  } else {
			lv.base = 2;
		  }

	    } else {
		  fprintf(vvp_out, "    %%and %u, %u, 1;\n", rv.base, lv.base);
		  lv = rv;
	    }

      } else {
	    fprintf(vvp_out, "    %%and %u, %u, 1;\n", lv.base, rv.base);
	    clr_vector(rv);
      }

      assert(wid == 1);

      return lv;
}

static struct vector_info draw_binary_expr_lor(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

	/* XXXX For now, assume the operands are a single bit. */
      assert(ivl_expr_width(le) == 1);
      assert(ivl_expr_width(re) == 1);

      lv = draw_eval_expr_wid(le, wid);
      rv = draw_eval_expr_wid(re, wid);

      if (lv.base < 4) {
	    if (rv.base < 4) {
		  unsigned lb = lv.base;
		  unsigned rb = rv.base;

		  if ((lb == 0) && (rb == 0)) {
			lv.base = 0;

		  } else if ((lb == 1) || (rb == 1)) {
			lv.base = 1;
		  } else {
			lv.base = 2;
		  }

	    } else {
		  fprintf(vvp_out, "    %%or %u, %u, 1;\n", rv.base, lv.base);
		  lv = rv;
	    }

      } else {
	    fprintf(vvp_out, "    %%or %u, %u, 1;\n", lv.base, rv.base);
	    clr_vector(rv);
      }

      assert(wid == 1);

      return lv;
}

static struct vector_info draw_binary_expr_le(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      char s_flag = (ivl_expr_signed(le) && ivl_expr_signed(re)) ? 's' : 'u';

      unsigned owid = ivl_expr_width(le);
      if (ivl_expr_width(re) > owid)
	    owid = ivl_expr_width(re);

      lv = draw_eval_expr_wid(le, owid);
      rv = draw_eval_expr_wid(re, owid);

      switch (ivl_expr_opcode(exp)) {
	  case 'G':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    rv.base, lv.base, lv.wid);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case 'L':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    lv.base, rv.base, lv.wid);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case '<':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    lv.base, rv.base, lv.wid);
	    break;

	  case '>':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    rv.base, lv.base, lv.wid);
	    break;

	  default:
	    assert(0);
      }

      clr_vector(lv);
      clr_vector(rv);

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      { unsigned short base = allocate_vector(1);
        fprintf(vvp_out, "    %%mov %u, 5, 1;\n", base);
	lv.base = base;
	lv.wid = 1;
      }

      assert(wid == 1);

      return lv;
}

static struct vector_info draw_binary_expr_logic(ivl_expr_t exp,
						 unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      lv = draw_eval_expr_wid(le, wid);
      rv = draw_eval_expr_wid(re, wid);

	/* The result goes into the left operand, and that is returned
	   as the result. The instructions do not allow the lv value
	   to be a constant bit, so we either switch the operands, or
	   copy the vector into a new area. */
      if (lv.base < 4) {
	    if (rv.base > 4) {
		  struct vector_info tmp = lv;
		  lv = rv;
		  rv = tmp;

	    } else {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(lv.wid);
		  tmp.wid = lv.wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, lv.base, tmp.wid);
		  lv = tmp;
	    }
      }

      switch (ivl_expr_opcode(exp)) {

	  case '&':
	    fprintf(vvp_out, "   %%and %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case '|':
	    fprintf(vvp_out, "   %%or %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case '^':
	    fprintf(vvp_out, "   %%xor %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case 'X': /* exclusive nor (~^) */
	    fprintf(vvp_out, "    %%xnor %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  default:
	    assert(0);
      }

      clr_vector(rv);
      return lv;
}

static struct vector_info draw_binary_expr_ls(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      unsigned shift = 0;

      struct vector_info lv;
      struct vector_info rs;

	/* XXXX support only constant right expressions. */
      switch (ivl_expr_type(re)) {
	  case IVL_EX_NUMBER: {
		unsigned idx, nbits = ivl_expr_width(re);
		const char*bits = ivl_expr_bits(re);

		for (idx = 0 ;  idx < nbits ;  idx += 1) switch (bits[idx]) {

		    case '0':
		      break;
		    case '1':
		      assert(idx < (8*sizeof shift));
		      shift |= 1 << idx;
		      break;
		    default:
		      assert(0);
		}
		break;
	  }
	    
	  case IVL_EX_ULONG:
	    shift = ivl_expr_uvalue(re);
	    break;
	  default:
	    assert(0);
	    break;
      }

      if (shift >= wid) {
	    rs.base = 0;
	    rs.wid  = wid;
	    return rs;
      }

      lv = draw_eval_expr_wid(le, wid);

      assert(lv.wid >= (wid - shift));
      rs.base = allocate_vector(wid);
      rs.wid = wid;

      fprintf(vvp_out, "    %%mov %u, %u, %u;\n", rs.base+shift,
	      lv.base, wid-shift);
      fprintf(vvp_out, "    %%mov %u, 0, %u;\n", rs.base, shift);
      clr_vector(lv);

      return rs;
}

static struct vector_info draw_binary_expr_rs(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      unsigned shift = 0;

      struct vector_info lv;
      struct vector_info rs;

	/* XXXX support only constant right expressions. */
      switch (ivl_expr_type(re)) {
	  case IVL_EX_NUMBER: {
		unsigned idx, nbits = ivl_expr_width(re);
		const char*bits = ivl_expr_bits(re);

		for (idx = 0 ;  idx < nbits ;  idx += 1) switch (bits[idx]) {

		    case '0':
		      break;
		    case '1':
		      assert(idx < (8*sizeof shift));
		      shift |= 1 << idx;
		      break;
		    default:
		      assert(0);
		}
		break;
	  }
	    
	  case IVL_EX_ULONG:
	    shift = ivl_expr_uvalue(re);
	    break;
	  default:
	    assert(0);
	    break;
      }

      if (shift >= wid) {
	    rs.base = 0;
	    rs.wid  = wid;
	    return rs;
      }

      lv = draw_eval_expr_wid(le, wid);


      switch (lv.base) {
	  case 0:
	    return lv;
	  case 1:
	  case 2:
	  case 3:
	    rs.base = allocate_vector(wid);
	    rs.wid = wid;
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", rs.base,
		    lv.base, wid-shift);
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    rs.base+wid-shift, shift);
	    return rs;

	  default:
	    assert(lv.wid == wid);
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", lv.base,
		    lv.base+shift, wid-shift);
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    lv.base+wid-shift, shift);
	    return lv;
      }
}

static struct vector_info draw_binary_expr_plus(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      assert(ivl_expr_width(le) == wid);
      assert(ivl_expr_width(re) == wid);

      lv = draw_eval_expr_wid(le, wid);
      rv = draw_eval_expr_wid(re, wid);

      if (ivl_expr_opcode(exp) == '-')
	    fprintf(vvp_out, "    %%sub %u, %u, %u;\n", lv.base, rv.base, wid);
      else
	    fprintf(vvp_out, "    %%add %u, %u, %u;\n", lv.base, rv.base, wid);

      clr_vector(rv);

      return lv;
}

static struct vector_info draw_binary_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info rv;

      switch (ivl_expr_opcode(exp)) {
	  case 'a': /* && (logical and) */
	    rv = draw_binary_expr_land(exp, wid);
	    break;

	  case 'E': /* === */
	  case 'e': /* == */
	  case 'N': /* !== */
	  case 'n': /* != */
	    assert(wid == 1);
	    rv = draw_binary_expr_eq(exp);
	    break;

	  case '<':
	  case '>':
	  case 'L': /* <= */
	  case 'G': /* >= */
	    rv = draw_binary_expr_le(exp, wid);
	    break;

	  case '+':
	  case '-':
	    rv = draw_binary_expr_plus(exp, wid);
	    break;

	  case 'l': /* << */
	    rv = draw_binary_expr_ls(exp, wid);
	    break;

	  case 'o': /* || (logical or) */
	    rv = draw_binary_expr_lor(exp, wid);
	    break;

	  case 'r': /* >> */
	    rv = draw_binary_expr_rs(exp, wid);
	    break;

	  case '&':
	  case '|':
	  case '^':
	  case 'X':
	    rv = draw_binary_expr_logic(exp, wid);
	    break;

	  default:
	    fprintf(stderr, "vvp.tgt error: unsupported binary (%c)\n",
		    ivl_expr_opcode(exp));
	    assert(0);
      }

      return rv;
}

static struct vector_info draw_concat_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned idx, off;
      struct vector_info res;

      assert(wid >= ivl_expr_width(exp));

      res.base = allocate_vector(wid);
      res.wid = wid;

      idx = ivl_expr_parms(exp);
      off = 0;
      while (idx > 0) {
	    ivl_expr_t arg = ivl_expr_parm(exp, idx-1);
	    unsigned awid = ivl_expr_width(arg);

	    struct vector_info avec = draw_eval_expr_wid(arg, awid);

	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", res.base+off,
		    avec.base, avec.wid);
	    clr_vector(avec);

	    idx -= 1;
	    off += awid;
      }

      if (off < wid) {
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+off, wid-off);
      }

      return res;
}

/*
 * A number in an expression is made up by copying constant bits into
 * the allocated vector.
 */
static struct vector_info draw_number_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned idx;
      unsigned nwid;
      struct vector_info res;
      const char*bits = ivl_expr_bits(exp);

      res.wid  = wid;

      nwid = wid;
      if (ivl_expr_width(exp) < nwid)
	    nwid = ivl_expr_width(exp);

	/* If all the bits of the number have the same value, then we
	   can use a constant bit. There is no need to allocate wr
	   bits, and there is no need to generate any code. */

      for (idx = 1 ;  idx < nwid ;  idx += 1) {
	    if (bits[idx] != bits[0])
		  break;
      }

      if (idx >= res.wid) {
	    switch (bits[0]) {
		case '0':
		  res.base = 0;
		  break;
		case '1':
		  res.base = 1;
		  break;
		case 'x':
		  res.base = 2;
		  break;
		case 'z':
		  res.base = 3;
		  break;
	    }
	    return res;
      }

	/* The number value needs to be represented as an allocated
	   vector. Allocate the vector and use %mov instructions to
	   load the constant bit values. */
      res.base = allocate_vector(wid);

      idx = 0;
      while (idx < nwid) {
	    unsigned cnt;
	    char src = '?';
	    switch (bits[idx]) {
		case '0':
		  src = '0';
		  break;
		case '1':
		  src = '1';
		  break;
		case 'x':
		  src = '2';
		  break;
		case 'z':
		  src = '3';
		  break;
	    }

	    for (cnt = 1 ;  idx+cnt < wid ;  cnt += 1)
		  if (bits[idx+cnt] != bits[idx])
			break;

	    fprintf(vvp_out, "    %%mov %u, %c, %u;\n",
		    res.base+idx, src, cnt);

	    idx += cnt;
      }

	/* Pad the number up to the expression width. */
      if (idx < wid)
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n", res.base+idx, wid-idx);

      return res;
}

static struct vector_info draw_signal_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned idx;
      unsigned swid = ivl_expr_width(exp);
      const char*name = ivl_expr_name(exp);
      struct vector_info res;

      if (swid > wid)
	    swid = wid;

      res.base = allocate_vector(wid);
      res.wid  = wid;

      for (idx = 0 ;  idx < swid ;  idx += 1)
	    fprintf(vvp_out, "    %%load  %u, V_%s[%u];\n",
		    res.base+idx, name, idx);

	/* Pad the signal value with zeros. */
      if (swid < wid)
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+swid, wid-swid);
      return res;
}

/*
 * A call to a user defined function generates a result that is the
 * result of this expression.
 *
 * The result of the function is placed by the function execution into
 * a signal within the scope of the function that also has a basename
 * the same as the function. The ivl_target API handled the result
 * mapping already, and we get the name of the result signal as
 * parameter 0 of the function definition.
 */

static struct vector_info draw_ufunc_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned idx;
      unsigned swid = ivl_expr_width(exp);
      ivl_scope_t def = ivl_expr_def(exp);
      const char*name = ivl_scope_port(def, 0);
      struct vector_info res;

	/* evaluate the expressions and send the results to the
	   function ports. */

      assert(ivl_expr_parms(exp) == (ivl_scope_ports(def)-1));
      for (idx = 0 ;  idx < ivl_expr_parms(exp) ;  idx += 1) {
	    const char*port = ivl_scope_port(def, idx+1);
	    unsigned pin, bit;

	    res = draw_eval_expr(ivl_expr_parm(exp, idx));
	    bit = res.base;
	    for (pin = 0 ;  pin < res.wid ;  pin += 1) {
		  fprintf(vvp_out, "    %%set V_%s[%u], %u;\n",
			  port, pin, bit);
		  if (bit >= 4)
			bit += 1;
	    }

	    clr_vector(res);
      }


	/* Call the function */
      fprintf(vvp_out, "    %%fork TD_%s, S_%s;\n", ivl_scope_name(def),
	      ivl_scope_name(def));
      fprintf(vvp_out, "    %%join;\n");

	/* The return value is in a signal that has the name of the
	   expression. Load that into the thread and return the
	   vector result. */

      res.base = allocate_vector(wid);
      res.wid  = wid;

      for (idx = 0 ;  idx < swid ;  idx += 1)
	    fprintf(vvp_out, "    %%load  %u, V_%s[%u];\n",
		    res.base+idx, name, idx);

	/* Pad the signal value with zeros. */
      if (swid < wid)
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+swid, wid-swid);

      return res;
}

static struct vector_info draw_unary_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res;
      ivl_expr_t sub = ivl_expr_oper1(exp);

      switch (ivl_expr_opcode(exp)) {
	  case '~':
	    res = draw_eval_expr_wid(sub, wid);
	    fprintf(vvp_out, "    %%inv %u, %u;\n", res.base, res.wid);
	    break;

	  case '!':
	    res = draw_eval_expr(sub);
	    if (res.wid > 1) {
		    /* a ! on a vector is implemented with a reduction
		       nor. Generate the result into the first bit of
		       the input vector and free the rest of the
		       vector. */
		  struct vector_info tmp;
		  assert(res.base >= 4);
		  tmp.base = res.base+1;
		  tmp.wid = res.wid - 1;
		  fprintf(vvp_out, "    %%nor/r %u, %u, %u;\n",
			  res.base, res.base, res.wid);
		  clr_vector(tmp);
		  res.wid = 1;
	    } else {
		  fprintf(vvp_out, "    %%inv %u, 1;\n", res.base);
	    }
	    break;

	  default:
	    fprintf(stderr, "vvp error: unhandled unary: %c\n",
		    ivl_expr_opcode(exp));
	    assert(0);
      }

      return res;
}

struct vector_info draw_eval_expr_wid(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res;

      switch (ivl_expr_type(exp)) {
	  default:
	    fprintf(stderr, "vvp error: unhandled expr type: %u\n",
		    ivl_expr_type(exp));
	  case IVL_EX_NONE:
	    assert(0);
	    res.base = 0;
	    res.wid = 0;
	    break;

	  case IVL_EX_BINARY:
	    res = draw_binary_expr(exp, wid);
	    break;

	  case IVL_EX_CONCAT:
	    res = draw_concat_expr(exp, wid);
	    break;

	  case IVL_EX_NUMBER:
	    res = draw_number_expr(exp, wid);
	    break;

	  case IVL_EX_SIGNAL:
	    res = draw_signal_expr(exp, wid);
	    break;

	  case IVL_EX_UFUNC:
	    res = draw_ufunc_expr(exp, wid);
	    break;

	  case IVL_EX_UNARY:
	    res = draw_unary_expr(exp, wid);
	    break;
      }

      return res;
}

struct vector_info draw_eval_expr(ivl_expr_t exp)
{
      return draw_eval_expr_wid(exp, ivl_expr_width(exp));
}

/*
 * $Log: eval_expr.c,v $
 * Revision 1.23  2001/05/02 01:57:25  steve
 *  Support behavioral subtraction.
 *
 * Revision 1.22  2001/05/01 02:07:34  steve
 *  Comparisons cant leave their results in the opcode
 *  result area or their values will be clobbered by other
 *  parts of a complex expression.
 *
 * Revision 1.21  2001/04/30 05:11:18  steve
 *  OR is %or. Get this right.
 *
 * Revision 1.20  2001/04/29 20:47:39  steve
 *  Evalulate logical or (||)
 *
 * Revision 1.19  2001/04/21 03:26:23  steve
 *  Right shift by constant.
 *
 * Revision 1.18  2001/04/21 01:49:17  steve
 *  Left shift by a constant amount.
 *
 * Revision 1.17  2001/04/18 05:12:03  steve
 *  Use the new %fork syntax.
 *
 * Revision 1.16  2001/04/15 16:37:48  steve
 *  add XOR support.
 *
 * Revision 1.15  2001/04/15 04:07:56  steve
 *  Add support for behavioral xnor.
 *
 * Revision 1.14  2001/04/06 02:28:03  steve
 *  Generate vvp code for functions with ports.
 *
 * Revision 1.13  2001/04/05 01:12:28  steve
 *  Get signed compares working correctly in vvp.
 *
 * Revision 1.12  2001/04/02 03:47:49  steve
 *  Evaluate binary & and | operators.
 *
 * Revision 1.11  2001/04/01 22:26:21  steve
 *  Unary ! is a reduction operator.
 *
 * Revision 1.10  2001/04/01 21:47:29  steve
 *  Implement the unary ! operator.
 *
 * Revision 1.9  2001/04/01 07:22:42  steve
 *  Generate code for < and <=.
 *
 * Revision 1.8  2001/04/01 06:49:32  steve
 *  Evaluate the logical AND operator.
 *
 * Revision 1.7  2001/03/31 17:36:39  steve
 *  Generate vvp code for case statements.
 *
 * Revision 1.6  2001/03/31 02:00:44  steve
 *  Generate code for + and concat expressions.
 *
 * Revision 1.5  2001/03/29 05:16:25  steve
 *  Handle truncation/padding of numbers.
 *
 * Revision 1.4  2001/03/29 02:52:39  steve
 *  Add unary ~ operator to tgt-vvp.
 *
 * Revision 1.3  2001/03/27 06:43:27  steve
 *  Evaluate === and !==
 *
 * Revision 1.2  2001/03/23 01:10:24  steve
 *  Assure that operands are the correct width.
 *
 * Revision 1.1  2001/03/22 05:06:21  steve
 *  Geneate code for conditional statements.
 *
 */

