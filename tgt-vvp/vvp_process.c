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
#ident "$Id: vvp_process.c,v 1.31 2001/05/03 04:55:28 steve Exp $"
#endif

# include  "vvp_priv.h"
# include  <string.h>
# include  <assert.h>

static int show_statement(ivl_statement_t net, ivl_scope_t sscope);

static unsigned local_count = 0;
static unsigned thread_count = 0;

/*
 * This file includes the code needed to generate VVP code for
 * processes. Scopes are already declared, we generate here the
 * executable code for the processes.
 */

unsigned bitchar_to_idx(char bit)
{
      switch (bit) {
	  case '0':
	    return 0;
	  case '1':
	    return 1;
	  case 'x':
	    return 2;
	  case 'z':
	    return 3;
	  default:
	    assert(0);
	    return 0;
      }
}

/*
 * These functions handle the blocking assignment. Use the %set
 * instruction to perform the actual assignment, and calculate any
 * lvalues and rvalues that need calculating.
 *
 * The set_to_nexus function takes a particular nexus and generates
 * the %set statements to assign the value.
 *
 * The show_stmt_assign function looks at the assign statement, scans
 * the l-values, and matches bits of the r-value with the correct
 * nexus.
 */

static void set_to_nexus(ivl_nexus_t nex, unsigned bit)
{
      unsigned idx;

      for (idx = 0 ;  idx < ivl_nexus_ptrs(nex) ;  idx += 1) {
	    ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);
	    unsigned pin = ivl_nexus_ptr_pin(ptr);
	    ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

	    if (sig == 0)
		  continue;

	    fprintf(vvp_out, "    %%set V_%s[%u], %u;\n",
		    ivl_signal_name(sig), pin, bit);
      }
}

static void assign_to_nexus(ivl_nexus_t nex, unsigned bit, unsigned delay)
{
      unsigned idx;

      for (idx = 0 ;  idx < ivl_nexus_ptrs(nex) ;  idx += 1) {
	    ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);
	    unsigned pin = ivl_nexus_ptr_pin(ptr);
	    ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

	    if (sig == 0)
		  continue;

	    fprintf(vvp_out, "    %%assign V_%s[%u], %u, %u;\n",
		    ivl_signal_name(sig), pin, delay, bit);
      }
}

static int show_stmt_assign(ivl_statement_t net)
{
      ivl_lval_t lval;
      ivl_expr_t rval = ivl_stmt_rval(net);


	/* Handle the special case that the r-value is a constant. We
	   can generate the %set statement directly, without any worry
	   about generating code to evaluate the r-value expressions. */

      if (ivl_expr_type(rval) == IVL_EX_NUMBER) {
	    unsigned idx;
	    const char*bits = ivl_expr_bits(rval);

	      /* XXXX Only single l-value supported for now */
	    assert(ivl_stmt_lvals(net) == 1);

	    lval = ivl_stmt_lval(net, 0);
	      /* XXXX No mux support yet. */
	    assert(ivl_lval_mux(lval) == 0);

	    for (idx = 0 ;  idx < ivl_lval_pins(lval) ;  idx += 1)
		  set_to_nexus(ivl_lval_pin(lval, idx),
			       bitchar_to_idx(bits[idx]));

	    return 0;
      }

      { struct vector_info res = draw_eval_expr(rval);
        unsigned wid = res.wid;
	unsigned idx;

	  /* XXXX Only single l-value supported for now */
	assert(ivl_stmt_lvals(net) == 1);

	lval = ivl_stmt_lval(net, 0);
	  /* XXXX No mux support yet. */
	assert(ivl_lval_mux(lval) == 0);

	if (ivl_lval_pins(lval) < wid)
	      wid = ivl_lval_pins(lval);

	for (idx = 0 ;  idx < wid ;  idx += 1) {
	      unsigned bidx = res.base < 4? res.base : (res.base+idx);
	      set_to_nexus(ivl_lval_pin(lval, idx), bidx);
	}

	for (idx = wid ;  idx < ivl_lval_pins(lval) ;  idx += 1)
	      set_to_nexus(ivl_lval_pin(lval, idx), 0);

	clr_vector(res);
      }

      return 0;
}

static int show_stmt_assign_nb(ivl_statement_t net)
{
      ivl_lval_t lval;
      ivl_expr_t rval = ivl_stmt_rval(net);
      ivl_expr_t del  = ivl_stmt_delay_expr(net);

      unsigned long delay = 0;
      if (del != 0) {
	      /* XXXX Only support constant values. */
	    assert(ivl_expr_type(del) == IVL_EX_ULONG);
	    delay = ivl_expr_uvalue(del);
      }


	/* Handle the special case that the r-value is a constant. We
	   can generate the %set statement directly, without any worry
	   about generating code to evaluate the r-value expressions. */

      if (ivl_expr_type(rval) == IVL_EX_NUMBER) {
	    unsigned idx;
	    const char*bits = ivl_expr_bits(rval);

	      /* XXXX Only single l-value supported for now */
	    assert(ivl_stmt_lvals(net) == 1);

	    lval = ivl_stmt_lval(net, 0);
	      /* XXXX No mux support yet. */
	    assert(ivl_lval_mux(lval) == 0);

	    for (idx = 0 ;  idx < ivl_lval_pins(lval) ;  idx += 1)
		  assign_to_nexus(ivl_lval_pin(lval, idx),
				  bitchar_to_idx(bits[idx]), delay);

	    return 0;
      }

      { struct vector_info res = draw_eval_expr(rval);
        unsigned wid = res.wid;
	unsigned idx;

	  /* XXXX Only single l-value supported for now */
	assert(ivl_stmt_lvals(net) == 1);

	lval = ivl_stmt_lval(net, 0);
	  /* XXXX No mux support yet. */
	assert(ivl_lval_mux(lval) == 0);

	if (ivl_lval_pins(lval) < wid)
	      wid = ivl_lval_pins(lval);

	for (idx = 0 ;  idx < wid ;  idx += 1)
	      assign_to_nexus(ivl_lval_pin(lval, idx), res.base+idx, delay);

	for (idx = wid ;  idx < ivl_lval_pins(lval) ;  idx += 1)
	      assign_to_nexus(ivl_lval_pin(lval, idx), 0, delay);

	clr_vector(res);
      }

      return 0;
}

static int show_stmt_block(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      unsigned idx;
      unsigned cnt = ivl_stmt_block_count(net);

      for (idx = 0 ;  idx < cnt ;  idx += 1) {
	    rc += show_statement(ivl_stmt_block_stmt(net, idx), sscope);
      }

      return rc;
}

static int show_stmt_case(ivl_statement_t net, ivl_scope_t sscope)
{
      ivl_expr_t exp = ivl_stmt_cond_expr(net);
      struct vector_info cond = draw_eval_expr(exp);
      unsigned count = ivl_stmt_case_count(net);

      unsigned local_base = local_count;

      unsigned idx, default_case;

      local_count += count + 1;

	/* First draw the branch table.  All the non-default cases
	   generate a branch out of here, to the code that implements
	   the case. The default will fall through all the tests. */
      default_case = count;

      for (idx = 0 ;  idx < count ;  idx += 1) {
	    ivl_expr_t cex = ivl_stmt_case_expr(net, idx);
	    struct vector_info cvec;

	    if (cex == 0) {
		  default_case = idx;
		  continue;
	    }

	    cvec = draw_eval_expr_wid(cex, cond.wid);
	    assert(cvec.wid == cond.wid);

	    switch (ivl_statement_type(net)) {

		case IVL_ST_CASE:
		  fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n",
			  cond.base, cvec.base, cond.wid);
		  fprintf(vvp_out, "    %%jmp/1 T_%d.%d, 6;\n",
			  thread_count, local_base+idx);
		  break;

		case IVL_ST_CASEX:
		  fprintf(vvp_out, "    %%cmp/x %u, %u, %u;\n",
			  cond.base, cvec.base, cond.wid);
		  fprintf(vvp_out, "    %%jmp/1 T_%d.%d, 4;\n",
			  thread_count, local_base+idx);
		  break;

		case IVL_ST_CASEZ:
		  fprintf(vvp_out, "    %%cmp/z %u, %u, %u;\n",
			  cond.base, cvec.base, cond.wid);
		  fprintf(vvp_out, "    %%jmp/1 T_%d.%d, 4;\n",
			  thread_count, local_base+idx);
		  break;

		default:
		  assert(0);
	    }
	    
	      /* Done with the case expression */
	    clr_vector(cvec);
      }

	/* Done with the condition expression */
      clr_vector(cond);

	/* Emit code for the default case. */
      if (default_case < count) {
	    ivl_statement_t cst = ivl_stmt_case_stmt(net, default_case);
	    show_statement(cst, sscope);
      }

	/* Jump to the out of the case. */
      fprintf(vvp_out, "    %%jmp T_%d.%d;\n", thread_count,
	      local_base+count);

      for (idx = 0 ;  idx < count ;  idx += 1) {
	    ivl_statement_t cst = ivl_stmt_case_stmt(net, idx);

	    if (idx == default_case)
		  continue;

	    fprintf(vvp_out, "T_%d.%d\n", thread_count, local_base+idx);
	    show_statement(cst, sscope);

	    fprintf(vvp_out, "    %%jmp T_%d.%d;\n", thread_count,
		    local_base+count);

      }

	/* The out of the case. */
      fprintf(vvp_out, "T_%d.%d\n",  thread_count, local_base+count);

      return 0;
}

static int show_stmt_condit(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      unsigned lab_false, lab_out;
      ivl_expr_t exp = ivl_stmt_cond_expr(net);
      struct vector_info cond = draw_eval_expr(exp);

      assert(cond.wid == 1);

      lab_false = local_count++;
      lab_out = local_count++;

      fprintf(vvp_out, "    %%jmp/0xz  T_%d.%d, %u;\n",
	      thread_count, lab_false, cond.base);

	/* Done with the condition expression. */
      clr_vector(cond);

      rc += show_statement(ivl_stmt_cond_true(net), sscope);

      if (ivl_stmt_cond_false(net)) {
	    fprintf(vvp_out, "    %%jmp T_%d.%d;\n", thread_count, lab_out);
	    fprintf(vvp_out, "T_%d.%u ;\n", thread_count, lab_false);

	    rc += show_statement(ivl_stmt_cond_false(net), sscope);

	    fprintf(vvp_out, "T_%d.%u ;\n", thread_count, lab_out);

      } else {
	    fprintf(vvp_out, "T_%d.%u ;\n", thread_count, lab_false);
      }

      return rc;
}

/*
 * The delay statement is easy. Simply write a ``%delay <n>''
 * instruction to delay the thread, then draw the included statement.
 * The delay statement comes from verilog code like this:
 *
 *        ...
 *        #<delay> <stmt>;
 */
static int show_stmt_delay(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      unsigned long delay = ivl_stmt_delay_val(net);
      ivl_statement_t stmt = ivl_stmt_sub_stmt(net);

      fprintf(vvp_out, "    %%delay %lu;\n", delay);
      rc += show_statement(stmt, sscope);

      return rc;
}

static int show_stmt_disable(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;

      ivl_scope_t target = ivl_stmt_call(net);
      fprintf(vvp_out, "    %%disable S_%s;\n", ivl_scope_name(target));

      return rc;
}

static int show_stmt_forever(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      ivl_statement_t stmt = ivl_stmt_sub_stmt(net);
      unsigned lab_top = local_count++;

      fprintf(vvp_out, "T_%u.%u ;\n", thread_count, lab_top);
      rc += show_statement(stmt, sscope);
      fprintf(vvp_out, "    %%jmp T_%u.%u;\n", thread_count, lab_top);

      return rc;
}

static int show_stmt_fork(ivl_statement_t net, ivl_scope_t sscope)
{
      unsigned idx;
      int rc = 0;
      static int transient_id = 0;
      unsigned cnt = ivl_stmt_block_count(net);

      int out = transient_id++;

	/* Draw a fork statement for all but one of the threads of the
	   fork/join. Send the threads off to a bit of code where they
	   are implemented. */
      for (idx = 0 ;  idx < cnt-1 ;  idx += 1) {
	    fprintf(vvp_out, "    %%fork t_%u, S_%s;\n",
		    transient_id+idx, ivl_scope_name(sscope));
      }

	/* Draw code to execute the remaining thread in the current
	   thread, then generate enough joins to merge back together. */
      rc += show_statement(ivl_stmt_block_stmt(net, cnt-1), sscope);

      for (idx = 0 ;  idx < cnt-1 ;  idx += 1) {
	    fprintf(vvp_out, "    %%join;\n");
      }
      fprintf(vvp_out, "    %%jmp t_%u;\n", out);

      for (idx = 0 ;  idx < cnt-1 ;  idx += 1) {
	    fprintf(vvp_out, "t_%u\n", transient_id+idx);
	    rc += show_statement(ivl_stmt_block_stmt(net, idx), sscope);
	    fprintf(vvp_out, "    %%end;\n");
      }

	/* This is the label for the out. Use this to branch around
	   the implementations of all the child threads. */
      fprintf(vvp_out, "t_%u\n", out);

      return rc;
}

/*
 * noop statements are implemented by doing nothing.
 */
static int show_stmt_noop(ivl_statement_t net)
{
      return 0;
}

static int show_stmt_repeat(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      unsigned lab_top = local_count++, lab_out = local_count++;
      ivl_expr_t exp = ivl_stmt_cond_expr(net);
      struct vector_info cnt = draw_eval_expr(exp);

	/* Test that 0 < expr */
      fprintf(vvp_out, "T_%u.%u %%cmp/u 0, %u, %u;\n", thread_count,
	      lab_top, cnt.base, cnt.wid);
      fprintf(vvp_out, "    %%jmp/0xz T_%u.%u, 5;\n", thread_count, lab_out);
	/* This adds -1 (all ones in 2's complement) to the count. */
      fprintf(vvp_out, "    %%add %u, 1, %u;\n", cnt.base, cnt.wid);

      rc += show_statement(ivl_stmt_sub_stmt(net), sscope);

      fprintf(vvp_out, "    %%jmp T_%u.%u;\n", thread_count, lab_top);
      fprintf(vvp_out, "T_%u.%u ;\n", thread_count, lab_out);

      clr_vector(cnt);

      return rc;
}

static int show_stmt_trigger(ivl_statement_t net)
{
      ivl_event_t ev = ivl_stmt_event(net);
      assert(ev);
      fprintf(vvp_out, "    %%set E_%s, 0;\n", ivl_event_name(ev));
      return 0;
}

static int show_stmt_utask(ivl_statement_t net)
{
      ivl_scope_t task = ivl_stmt_call(net);

      fprintf(vvp_out, "    %%fork TD_%s, S_%s;\n",
	      ivl_scope_name(task), ivl_scope_name(task));
      fprintf(vvp_out, "    %%join;\n");
      return 0;
}

static int show_stmt_wait(ivl_statement_t net, ivl_scope_t sscope)
{
      ivl_event_t ev = ivl_stmt_event(net);
      fprintf(vvp_out, "    %%wait E_%s;\n", ivl_event_name(ev));

      return show_statement(ivl_stmt_sub_stmt(net), sscope);
}

static int show_stmt_while(ivl_statement_t net, ivl_scope_t sscope)
{
      int rc = 0;
      struct vector_info cvec;

      unsigned top_label = local_count++;
      unsigned out_label = local_count++;

      fprintf(vvp_out, "T_%d.%d\n", thread_count, top_label);

	/* Draw the evaluation of the condition expression, and test
	   the result. If the expression evaluates to false, then
	   branch to the out label. */
      cvec = draw_eval_expr(ivl_stmt_cond_expr(net));
      fprintf(vvp_out, "    %%jmp/0xz T_%d.%d, %u;\n",
	      thread_count, out_label, cvec.base);
      clr_vector(cvec);

	/* Draw the body of the loop. */
      rc += show_statement(ivl_stmt_sub_stmt(net), sscope);

	/* This is the bottom of the loop. branch to the top where the
	   test is repeased, and also draw the out label. */
      fprintf(vvp_out, "    %%jmp T_%d.%d;\n", thread_count, top_label);
      fprintf(vvp_out, "T_%d.%d\n", thread_count, out_label);
      return rc;
}

static int show_system_task_call(ivl_statement_t net)
{
      unsigned idx;
      unsigned parm_count = ivl_stmt_parm_count(net);

      if (parm_count == 0) {
	    fprintf(vvp_out, "    %%vpi_call \"%s\";\n", ivl_stmt_name(net));
	    return 0;
      }

      fprintf(vvp_out, "    %%vpi_call \"%s\"", ivl_stmt_name(net));
      for (idx = 0 ;  idx < parm_count ;  idx += 1) {
	    ivl_expr_t expr = ivl_stmt_parm(net, idx);

	    switch (ivl_expr_type(expr)) {

		case IVL_EX_NUMBER: {
		      unsigned bit, wid = ivl_expr_width(expr);
		      const char*bits = ivl_expr_bits(expr);

		      fprintf(vvp_out, ", %u'b", wid);
		      for (bit = wid ;  bit > 0 ;  bit -= 1)
			    fputc(bits[bit-1], vvp_out);
		      break;
		}

		case IVL_EX_SIGNAL:
		  fprintf(vvp_out, ", V_%s", ivl_expr_name(expr));
		  break;

		case IVL_EX_STRING:
		  fprintf(vvp_out, ", \"%s\"", ivl_expr_string(expr));
		  break;

		case IVL_EX_SCOPE:
		  fprintf(vvp_out, ", S_%s",
			  ivl_scope_name(ivl_expr_scope(expr)));
		  break;

		case IVL_EX_SFUNC:
		  if (strcmp("$time", ivl_expr_name(expr)) == 0)
			fprintf(vvp_out, ", $time");
		  else
			fprintf(vvp_out, ", ?");
		  break;

		default:
		  fprintf(vvp_out, ", ?");
		  break;
	    }
      }

      fprintf(vvp_out, ";\n");
      return 0;
}

/*
 * This function draws a statement as vvp assembly. It basically
 * switches on the statement type and draws code based on the type and
 * further specifics.
 */
static int show_statement(ivl_statement_t net, ivl_scope_t sscope)
{
      const ivl_statement_type_t code = ivl_statement_type(net);
      int rc = 0;

      switch (code) {

	  case IVL_ST_ASSIGN:
	    rc += show_stmt_assign(net);
	    break;

	  case IVL_ST_ASSIGN_NB:
	    rc += show_stmt_assign_nb(net);
	    break;

	  case IVL_ST_BLOCK:
	    rc += show_stmt_block(net, sscope);
	    break;

	  case IVL_ST_CASE:
	  case IVL_ST_CASEX:
	  case IVL_ST_CASEZ:
	    rc += show_stmt_case(net, sscope);
	    break;

	  case IVL_ST_CONDIT:
	    rc += show_stmt_condit(net, sscope);
	    break;

	  case IVL_ST_DELAY:
	    rc += show_stmt_delay(net, sscope);
	    break;

	  case IVL_ST_DISABLE:
	    rc += show_stmt_disable(net, sscope);
	    break;

	  case IVL_ST_FOREVER:
	    rc += show_stmt_forever(net, sscope);
	    break;

	  case IVL_ST_FORK:
	    rc += show_stmt_fork(net, sscope);
	    break;

	  case IVL_ST_NOOP:
	    rc += show_stmt_noop(net);
	    break;

	  case IVL_ST_REPEAT:
	    rc += show_stmt_repeat(net, sscope);
	    break;

	  case IVL_ST_STASK:
	    rc += show_system_task_call(net);
	    break;

	  case IVL_ST_TRIGGER:
	    rc += show_stmt_trigger(net);
	    break;

	  case IVL_ST_UTASK:
	    rc += show_stmt_utask(net);
	    break;

	  case IVL_ST_WAIT:
	    rc += show_stmt_wait(net, sscope);
	    break;

	  case IVL_ST_WHILE:
	    rc += show_stmt_while(net, sscope);
	    break;

	  default:
	    fprintf(stderr, "vvp.tgt: Unable to draw statement type %u\n",
		    code);
	    rc += 1;
	    break;
      }

      return rc;
}


/*
 * The process as a whole is surrounded by this code. We generate a
 * start label that the .thread statement can use, and we generate
 * code to terminate the thread.
 */

int draw_process(ivl_process_t net, void*x)
{
      int rc = 0;
      ivl_scope_t scope = ivl_process_scope(net);
      ivl_statement_t stmt = ivl_process_stmt(net);

      local_count = 0;
      fprintf(vvp_out, "    .scope S_%s;\n", ivl_scope_name(scope));

	/* Generate the entry label. Just give the thread a number so
	   that we ar certain the label is unique. */
      fprintf(vvp_out, "T_%d ;\n", thread_count);

	/* Draw the contents of the thread. */
      rc += show_statement(stmt, scope);


	/* Terminate the thread with either an %end instruction (initial
	   statements) or a %jmp back to the beginning of the thread. */

      switch (ivl_process_type(net)) {

	  case IVL_PR_INITIAL:
	    fprintf(vvp_out, "    %%end;\n");
	    break;

	  case IVL_PR_ALWAYS:
	    fprintf(vvp_out, "    %%jmp T_%d;\n", thread_count);
	    break;
      }

	/* Now write out the .thread directive that tells vvp where
	   the thread starts. */
      fprintf(vvp_out, "    .thread T_%d;\n", thread_count);


      thread_count += 1;
      return rc;
}

int draw_task_definition(ivl_scope_t scope)
{
      int rc = 0;
      ivl_statement_t def = ivl_scope_def(scope);

      fprintf(vvp_out, "TD_%s ;\n", ivl_scope_name(scope));

      assert(def);
      rc += show_statement(def, scope);

      fprintf(vvp_out, "    %%end;\n");

      thread_count += 1;
      return rc;
}

int draw_func_definition(ivl_scope_t scope)
{
      int rc = 0;
      ivl_statement_t def = ivl_scope_def(scope);

      fprintf(vvp_out, "TD_%s ;\n", ivl_scope_name(scope));

      assert(def);
      rc += show_statement(def, scope);

      fprintf(vvp_out, "    %%end;\n");

      thread_count += 1;
      return rc;
}

/*
 * $Log: vvp_process.c,v $
 * Revision 1.31  2001/05/03 04:55:28  steve
 *  Generate null statements for conditional labels.
 *
 * Revision 1.30  2001/04/21 03:26:23  steve
 *  Right shift by constant.
 *
 * Revision 1.29  2001/04/21 00:55:46  steve
 *  Generate code for disable.
 *
 * Revision 1.28  2001/04/18 05:12:03  steve
 *  Use the new %fork syntax.
 *
 * Revision 1.27  2001/04/15 02:58:11  steve
 *  vvp support for <= with internal delay.
 *
 * Revision 1.26  2001/04/06 02:28:03  steve
 *  Generate vvp code for functions with ports.
 *
 * Revision 1.25  2001/04/05 03:20:58  steve
 *  Generate vvp code for the repeat statement.
 *
 * Revision 1.24  2001/04/04 04:50:35  steve
 *  Support forever loops in the tgt-vvp target.
 *
 * Revision 1.23  2001/04/04 04:28:41  steve
 *  Fix broken look scanning down bits of number.
 *
 * Revision 1.22  2001/04/04 04:14:09  steve
 *  emit vpi parameters values as vectors.
 *
 * Revision 1.21  2001/04/03 04:50:37  steve
 *  Support non-blocking assignments.
 *
 * Revision 1.20  2001/04/02 04:09:20  steve
 *  thread bit allocation leak in assign.
 *
 * Revision 1.19  2001/04/02 02:28:13  steve
 *  Generate code for task calls.
 *
 * Revision 1.18  2001/04/02 00:27:53  steve
 *  Scopes and numbers as vpi_call parameters.
 *
 * Revision 1.17  2001/04/01 06:49:04  steve
 *  Generate code for while statements.
 *
 * Revision 1.16  2001/04/01 04:34:59  steve
 *  Generate code for casex and casez
 *
 * Revision 1.15  2001/03/31 19:08:22  steve
 *  Handle $time as system task parameter.
 *
 * Revision 1.14  2001/03/31 19:02:13  steve
 *  Clear results of condition expressions.
 *
 * Revision 1.13  2001/03/31 17:36:39  steve
 *  Generate vvp code for case statements.
 *
 * Revision 1.12  2001/03/30 05:49:53  steve
 *  Generate code for fork/join statements.
 *
 * Revision 1.11  2001/03/29 03:47:38  steve
 *  Behavioral trigger statements.
 *
 * Revision 1.10  2001/03/28 06:07:40  steve
 *  Add the ivl_event_t to ivl_target, and use that to generate
 *  .event statements in vvp way ahead of the thread that uses it.
 *
 * Revision 1.9  2001/03/27 06:27:41  steve
 *  Generate code for simple @ statements.
 *
 * Revision 1.8  2001/03/27 03:31:07  steve
 *  Support error code from target_t::end_design method.
 *
 * Revision 1.7  2001/03/25 03:53:24  steve
 *  Skip true clause if condition ix 0, x or z
 *
 * Revision 1.6  2001/03/25 03:24:10  steve
 *  Draw signal inputs to system tasks.
 *
 * Revision 1.5  2001/03/23 01:54:32  steve
 *  assignments with non-trival r-values.
 *
 * Revision 1.4  2001/03/22 05:06:21  steve
 *  Geneate code for conditional statements.
 *
 * Revision 1.3  2001/03/21 01:49:43  steve
 *  Scan the scopes of a design, and draw behavioral
 *  blocking  assignments of constants to vectors.
 *
 * Revision 1.2  2001/03/20 01:44:14  steve
 *  Put processes in the proper scope.
 *
 * Revision 1.1  2001/03/19 01:20:46  steve
 *  Add the tgt-vvp code generator target.
 *
 */

