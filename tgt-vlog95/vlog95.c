/*
 * Copyright (C) 2010-2011 Cary R. (cygcary@yahoo.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * This is the vlog95 target module. It generates a 1364-1995 compliant
 * netlist from the input netlist. The generated netlist is expected to
 * be simulation equivalent to the original.
 */

# include "version_base.h"
# include "version_tag.h"
# include "config.h"
# include "vlog95_priv.h"
# include <stdlib.h>
# include <string.h>

static const char*version_string =
"Icarus Verilog VLOG95 Code Generator " VERSION " (" VERSION_TAG ")\n\n"
"Copyright (C) 2010-2011 Cary R. (cygcary@yahoo.com)\n\n"
"  This program is free software; you can redistribute it and/or modify\n"
"  it under the terms of the GNU General Public License as published by\n"
"  the Free Software Foundation; either version 2 of the License, or\n"
"  (at your option) any later version.\n\n"
"  This program is distributed in the hope that it will be useful,\n"
"  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"  GNU General Public License for more details.\n\n"
"  You should have received a copy of the GNU General Public License along\n"
"  with this program; if not, write to the Free Software Foundation, Inc.,\n"
"  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n"
;

FILE*vlog_out;
int vlog_errors = 0;
int sim_precision = 0;
unsigned indent = 0;
unsigned indent_incr = 2;

unsigned emit_file_line = 0;

ivl_design_t design = 0;

int target_design(ivl_design_t des)
{
      ivl_scope_t *roots;
      unsigned nroots, idx;
      const char*path = ivl_design_flag(des, "-o");
	/* Set the indent spacing with the -pspacing flag passed to iverilog
	 * (e.g. -pspacing=4). The default is 2 spaces. */
      const char*spacing = ivl_design_flag(des, "spacing");
	/* Use -pfileline to determine if file and line information is
	 * printed for most lines. (e.g. -pfileline=1). The default is no
	 * file/line information will be printed for individual lines. */
      const char*fileline = ivl_design_flag(des, "fileline");
      assert(path);

	/* Check for and use a provided indent spacing. */
      if (strcmp(spacing, "") != 0) {
	    char *eptr;
	    long sp_incr = strtol(spacing, &eptr, 0);
	      /* Nothing usable in the spacing string. */
	    if (spacing == eptr) {
		  fprintf(stderr, "vlog95 error: Unable to extract spacing "
		                  "increment from string: %s\n", spacing);
		  return 1;
	    }
	      /* Extra stuff at the end. */
	    if (*eptr != 0) {
		  fprintf(stderr, "vlog95 error: Extra characters '%s' "
		                  "included at end of spacing string: %s\n",
		                  eptr, spacing);
		  return 1;
	    }
	      /* The increment must be greater than zero. */
	    if (sp_incr < 1) {
		  fprintf(stderr, "vlog95 error: Spacing increment (%ld) must "
		                  "be greater than zero.\n", sp_incr);
		  return 1;
	    }
	      /* An increment of more than sixteen is too much. */
	    if (sp_incr > 16) {
		  fprintf(stderr, "vlog95 error: Spacing increment (%ld) must "
		                  "be sixteen or less.\n", sp_incr);
		  return 1;
	    }
	    indent_incr = sp_incr;
      }

	/* Check to see if file/line information should be printed. */
      if (strcmp(fileline, "") != 0) {
	    char *eptr;
	    long fl_value = strtol(fileline, &eptr, 0);
	      /* Nothing usable in the file/line string. */
	    if (fileline == eptr) {
		  fprintf(stderr, "vlog95 error: Unable to extract file/line "
		                  "information from string: %s\n", fileline);
		  return 1;
	    }
	      /* Extra stuff at the end. */
	    if (*eptr != 0) {
		  fprintf(stderr, "vlog95 error: Extra characters '%s' "
		                  "included at end of file/line string: %s\n",
		                  eptr, fileline);
		  return 1;
	    }
	      /* The file/line flag must be positive. */
	    if (fl_value < 0) {
		  fprintf(stderr, "vlog95 error: File/line flag (%ld) must "
		                  "be positive.\n", fl_value);
		  return 1;
	    }
	    emit_file_line = fl_value > 0;
      }

      design = des;

#ifdef HAVE_FOPEN64
      vlog_out = fopen64(path, "w");
#else
      vlog_out = fopen(path, "w");
#endif
      if (vlog_out == 0) {
	    perror(path);
	    return -1;
      }

      fprintf(vlog_out, "/*\n");
      fprintf(vlog_out, " * 1364-1995 Verilog generated by Icarus Verilog "
                        "VLOG95 Code Generator,\n");
      fprintf(vlog_out, " * Version: " VERSION " (" VERSION_TAG ")\n");
      fprintf(vlog_out, " * Converted using %s delays.\n",
                        ivl_design_delay_sel(des));
      fprintf(vlog_out, " */\n");

      sim_precision = ivl_design_time_precision(des);

	/* Get all the root modules and then convert each one. */
      ivl_design_roots(des, &roots, &nroots);
      for (idx = 0; idx < nroots; idx += 1) emit_scope(roots[idx], 0);

      free_emitted_scope_list();

	/* Emit any UDP definitions that the design used. */
      emit_udp_list();

	/* Emit any UDPs that are Icarus generated (D-FF). */
      emit_icarus_generated_udps();

      fclose(vlog_out);

      return vlog_errors;
}


const char* target_query(const char*key)
{
      if (strcmp(key,"version") == 0) return version_string;

      return 0;
}
