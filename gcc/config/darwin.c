/* Functions for generic Darwin as target machine for GNU C compiler.
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "tree.h"
#include "expr.h"
#include "reload.h"
#include "function.h"
#include "ggc.h"
#include "langhooks.h"
#include "tm_p.h"
#include "errors.h"
#include "hashtab.h"

/* Nonzero if the user passes the -mone-byte-bool switch, which forces
   sizeof(bool) to be 1. */
const char *darwin_one_byte_bool = 0;

int
name_needs_quotes (const char *name)
{
  int c;
  while ((c = *name++) != '\0')
    if (! ISIDNUM (c) && c != '.' && c != '$')
      return 1;
  return 0;
}

/*
 * flag_pic = 1 ... generate only indirections
 * flag_pic = 2 ... generate indirections and pure code
 */

static int
machopic_symbol_defined_p (rtx sym_ref)
{
  return ((SYMBOL_REF_FLAGS (sym_ref) & MACHO_SYMBOL_FLAG_DEFINED)
	  /* Local symbols must always be defined.  */
	  || SYMBOL_REF_LOCAL_P (sym_ref));
}

/* This module assumes that (const (symbol_ref "foo")) is a legal pic
   reference, which will not be changed.  */

enum machopic_addr_class
machopic_classify_symbol (rtx sym_ref)
{
  int flags;
  bool function_p;

  flags = SYMBOL_REF_FLAGS (sym_ref);
  function_p = SYMBOL_REF_FUNCTION_P (sym_ref);
  if (machopic_symbol_defined_p (sym_ref))
    return (function_p 
	    ? MACHOPIC_DEFINED_FUNCTION : MACHOPIC_DEFINED_DATA);
  else
    return (function_p 
	    ? MACHOPIC_UNDEFINED_FUNCTION : MACHOPIC_UNDEFINED_DATA);
}

static int
machopic_data_defined_p (rtx sym_ref)
{
  switch (machopic_classify_symbol (sym_ref))
    {
    case MACHOPIC_DEFINED_DATA:
      return 1;
    default:
      return 0;
    }
}

void
machopic_define_symbol (rtx mem)
{
  rtx sym_ref;
  if (GET_CODE (mem) != MEM)
    abort ();
  sym_ref = XEXP (mem, 0);
  SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_DEFINED;
}

static GTY(()) char * function_base;

const char *
machopic_function_base_name (void)
{
  /* if dynamic-no-pic is on, we should not get here */
  if (MACHO_DYNAMIC_NO_PIC_P)
    abort ();

  if (function_base == NULL)
    function_base =
      (char *) ggc_alloc_string ("<pic base>", sizeof ("<pic base>"));

  current_function_uses_pic_offset_table = 1;

  return function_base;
}

/* Return a SYMBOL_REF for the PIC function base.  */

rtx
machopic_function_base_sym (void)
{
  rtx sym_ref;

  sym_ref = gen_rtx_SYMBOL_REF (Pmode, machopic_function_base_name ());
  SYMBOL_REF_FLAGS (sym_ref) 
    |= (MACHO_SYMBOL_FLAG_VARIABLE | MACHO_SYMBOL_FLAG_DEFINED);
  return sym_ref;
}

static GTY(()) const char * function_base_func_name;
static GTY(()) int current_pic_label_num;

void
machopic_output_function_base_name (FILE *file)
{
  const char *current_name;

  /* If dynamic-no-pic is on, we should not get here.  */
  if (MACHO_DYNAMIC_NO_PIC_P)
    abort ();
  current_name =
    IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (current_function_decl));
  if (function_base_func_name != current_name)
    {
      ++current_pic_label_num;
      function_base_func_name = current_name;
    }
  fprintf (file, "\"L%011d$pb\"", current_pic_label_num);
}

/* The suffix attached to non-lazy pointer symbols.  */
#define NON_LAZY_POINTER_SUFFIX "$non_lazy_ptr"
/* The suffix attached to stub symbols.  */
#define STUB_SUFFIX "$stub"

typedef struct machopic_indirection GTY (())
{
  /* The SYMBOL_REF for the entity referenced.  */
  rtx symbol;
  /* The IDENTIFIER_NODE giving the name of the stub or non-lazy
     pointer.  */
  tree ptr_name;
  /* True iff this entry is for a stub (as opposed to a non-lazy
     pointer).  */
  bool stub_p;
  /* True iff this stub or pointer pointer has been referenced.  */
  bool used;
} machopic_indirection;

/* A table mapping stub names and non-lazy pointer names to
   SYMBOL_REFs for the stubbed-to and pointed-to entities.  */

static GTY ((param_is (struct machopic_indirection))) htab_t 
  machopic_indirections;

/* Return a hash value for a SLOT in the indirections hash table.  */

static hashval_t
machopic_indirection_hash (const void *slot)
{
  const machopic_indirection *p = (const machopic_indirection *) slot;
  return IDENTIFIER_HASH_VALUE (p->ptr_name);
}

/* Returns true if the KEY is the same as that associated with
   SLOT.  */

static int
machopic_indirection_eq (const void *slot, const void *key)
{
  return ((const machopic_indirection *) slot)->ptr_name == (tree) key;
}

/* Return the name of the non-lazy pointer (if STUB_P is false) or
   stub (if STUB_B is true) corresponding to the given name.  */

const char *
machopic_indirection_name (rtx sym_ref, bool stub_p)
{
  char *buffer;
  const char *name = XSTR (sym_ref, 0);
  int namelen = strlen (name);
  tree ptr_name;
  machopic_indirection *p;
  
  /* Construct the name of the non-lazy pointer or stub.  */
  if (stub_p)
    {
      int needs_quotes = name_needs_quotes (name);
      buffer = alloca (strlen ("&L")
		       + namelen
		       + strlen (STUB_SUFFIX)
		       + 2 /* possible quotes */
		       + 1 /* '\0' */);

      if (needs_quotes)
	{
	  if (name[0] == '*')
	    sprintf (buffer, "&\"L%s" STUB_SUFFIX "\"", name + 1);
	  else
	    sprintf (buffer, "&\"L%s%s" STUB_SUFFIX "\"", user_label_prefix, 
		     name);
	}
      else if (name[0] == '*')
	sprintf (buffer, "&L%s" STUB_SUFFIX, name + 1);
      else
	sprintf (buffer, "&L%s%s" STUB_SUFFIX, user_label_prefix, name);
    }
  else
    {
      buffer = alloca (strlen ("&L")
		       + strlen (user_label_prefix)
		       + namelen
		       + strlen (NON_LAZY_POINTER_SUFFIX)
		       + 1 /* '\0' */);
      if (name[0] == '*')
	sprintf (buffer, "&L%s" NON_LAZY_POINTER_SUFFIX, name + 1);
      else
	sprintf (buffer, "&L%s%s" NON_LAZY_POINTER_SUFFIX, 
		 user_label_prefix, name);
    }

  /* See if we already have it.  */
  ptr_name = maybe_get_identifier (buffer);
  /* If not, create a mapping from the non-lazy pointer to the
     SYMBOL_REF.  */
  if (!ptr_name)
    {
      void **slot;
      ptr_name = get_identifier (buffer);
      p = (machopic_indirection *) ggc_alloc (sizeof (machopic_indirection));
      p->symbol = sym_ref;
      p->ptr_name = ptr_name;
      p->stub_p = stub_p;
      p->used = 0;
      if (!machopic_indirections)
	machopic_indirections 
	  = htab_create_ggc (37, 
			     machopic_indirection_hash,
			     machopic_indirection_eq,
			     /*htab_del=*/NULL);
      slot = htab_find_slot_with_hash (machopic_indirections, ptr_name,
				       IDENTIFIER_HASH_VALUE (ptr_name),
				       INSERT);
      *((machopic_indirection **) slot) = p;
    }
  
  return IDENTIFIER_POINTER (ptr_name);
}

/* Return the name of the stub for the mcount function.  */

const char*
machopic_mcount_stub_name (void)
{
  return "&L*mcount$stub";
}

/* If NAME is the name of a stub or a non-lazy pointer , mark the stub
   or non-lazy pointer as used -- and mark the object to which the
   pointer/stub refers as used as well, since the pointer/stub will
   emit a reference to it.  */

void
machopic_validate_stub_or_non_lazy_ptr (const char *name)
{
  tree ident = get_identifier (name);

  machopic_indirection *p;
  
  p = ((machopic_indirection *) 
       (htab_find_with_hash (machopic_indirections, ident,
			     IDENTIFIER_HASH_VALUE (ident))));
  if (p)
    {
      p->used = 1;
      mark_referenced (ident);
      mark_referenced (get_identifier (XSTR (p->symbol, 0)));
    }
}

/* Transform ORIG, which may be any data source, to the corresponding
   source using indirections.  */

rtx
machopic_indirect_data_reference (rtx orig, rtx reg)
{
  rtx ptr_ref = orig;

  if (! MACHOPIC_INDIRECT)
    return orig;

  if (GET_CODE (orig) == SYMBOL_REF)
    {
      int defined = machopic_data_defined_p (orig);

      if (defined && MACHO_DYNAMIC_NO_PIC_P)
	{
#if defined (TARGET_TOC)
	  emit_insn (gen_macho_high (reg, orig));
	  emit_insn (gen_macho_low (reg, reg, orig));
#else
	   /* some other cpu -- writeme!  */
	   abort ();
#endif
	   return reg;
	}
      else if (defined)
	{
#if defined (TARGET_TOC) || defined (HAVE_lo_sum)
	  rtx pic_base = machopic_function_base_sym ();
	  rtx offset = gen_rtx_CONST (Pmode,
				      gen_rtx_MINUS (Pmode, orig, pic_base));
#endif

#if defined (TARGET_TOC) /* i.e., PowerPC */
	  rtx hi_sum_reg = (no_new_pseudos ? reg : gen_reg_rtx (Pmode));

	  if (reg == NULL)
	    abort ();

	  emit_insn (gen_rtx_SET (Pmode, hi_sum_reg,
			      gen_rtx_PLUS (Pmode, pic_offset_table_rtx,
				       gen_rtx_HIGH (Pmode, offset))));
	  emit_insn (gen_rtx_SET (Pmode, reg,
				  gen_rtx_LO_SUM (Pmode, hi_sum_reg, offset)));

	  orig = reg;
#else
#if defined (HAVE_lo_sum)
	  if (reg == 0) abort ();

	  emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_HIGH (Pmode, offset)));
	  emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_LO_SUM (Pmode, reg, offset)));
	  emit_insn (gen_rtx_USE (VOIDmode, pic_offset_table_rtx));

	  orig = gen_rtx_PLUS (Pmode, pic_offset_table_rtx, reg);
#endif
#endif
	  return orig;
	}

      ptr_ref = (gen_rtx_SYMBOL_REF 
		 (Pmode, 
		  machopic_indirection_name (orig, /*stub_p=*/false)));

     SYMBOL_REF_DECL (ptr_ref) = SYMBOL_REF_DECL (orig);

      ptr_ref = gen_rtx_MEM (Pmode, ptr_ref);
      MEM_READONLY_P (ptr_ref) = 1;

      return ptr_ref;
    }
  else if (GET_CODE (orig) == CONST)
    {
      rtx base, result;

      /* legitimize both operands of the PLUS */
      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  base = machopic_indirect_data_reference (XEXP (XEXP (orig, 0), 0),
						   reg);
	  orig = machopic_indirect_data_reference (XEXP (XEXP (orig, 0), 1),
						   (base == reg ? 0 : reg));
	}
      else
	return orig;

      if (MACHOPIC_PURE && GET_CODE (orig) == CONST_INT)
	result = plus_constant (base, INTVAL (orig));
      else
	result = gen_rtx_PLUS (Pmode, base, orig);

      if (MACHOPIC_JUST_INDIRECT && GET_CODE (base) == MEM)
	{
	  if (reg)
	    {
	      emit_move_insn (reg, result);
	      result = reg;
	    }
	  else
	    {
	      result = force_reg (GET_MODE (result), result);
	    }
	}

      return result;

    }
  else if (GET_CODE (orig) == MEM)
    XEXP (ptr_ref, 0) = machopic_indirect_data_reference (XEXP (orig, 0), reg);
  /* When the target is i386, this code prevents crashes due to the
     compiler's ignorance on how to move the PIC base register to
     other registers.  (The reload phase sometimes introduces such
     insns.)  */
  else if (GET_CODE (orig) == PLUS
	   && GET_CODE (XEXP (orig, 0)) == REG
	   && REGNO (XEXP (orig, 0)) == PIC_OFFSET_TABLE_REGNUM
#ifdef I386
	   /* Prevent the same register from being erroneously used
	      as both the base and index registers.  */
	   && GET_CODE (XEXP (orig, 1)) == CONST
#endif
	   && reg)
    {
      emit_move_insn (reg, XEXP (orig, 0));
      XEXP (ptr_ref, 0) = reg;
    }
  return ptr_ref;
}

/* Transform TARGET (a MEM), which is a function call target, to the
   corresponding symbol_stub if necessary.  Return a new MEM.  */

rtx
machopic_indirect_call_target (rtx target)
{
  if (GET_CODE (target) != MEM)
    return target;

  if (MACHOPIC_INDIRECT 
      && GET_CODE (XEXP (target, 0)) == SYMBOL_REF
      && !(SYMBOL_REF_FLAGS (XEXP (target, 0))
	   & MACHO_SYMBOL_FLAG_DEFINED))
    {
      rtx sym_ref = XEXP (target, 0);
      const char *stub_name = machopic_indirection_name (sym_ref, 
							 /*stub_p=*/true);
      enum machine_mode mode = GET_MODE (sym_ref);
      tree decl = SYMBOL_REF_DECL (sym_ref);
      
      XEXP (target, 0) = gen_rtx_SYMBOL_REF (mode, stub_name);
      SYMBOL_REF_DECL (XEXP (target, 0)) = decl;
      MEM_READONLY_P (target) = 1;
    }

  return target;
}

rtx
machopic_legitimize_pic_address (rtx orig, enum machine_mode mode, rtx reg)
{
  rtx pic_ref = orig;

  if (! MACHOPIC_INDIRECT)
    return orig;

  /* First handle a simple SYMBOL_REF or LABEL_REF */
  if (GET_CODE (orig) == LABEL_REF
      || (GET_CODE (orig) == SYMBOL_REF
	  ))
    {
      /* addr(foo) = &func+(foo-func) */
      rtx pic_base;

      orig = machopic_indirect_data_reference (orig, reg);

      if (GET_CODE (orig) == PLUS
	  && GET_CODE (XEXP (orig, 0)) == REG)
	{
	  if (reg == 0)
	    return force_reg (mode, orig);

	  emit_move_insn (reg, orig);
	  return reg;
	}

      /* if dynamic-no-pic then use 0 as the pic base  */
      if (MACHO_DYNAMIC_NO_PIC_P)
	pic_base = CONST0_RTX (Pmode);
      else
	pic_base = machopic_function_base_sym ();

      if (GET_CODE (orig) == MEM)
	{
	  if (reg == 0)
	    {
	      if (reload_in_progress)
		abort ();
	      else
		reg = gen_reg_rtx (Pmode);
	    }

#ifdef HAVE_lo_sum
	  if (MACHO_DYNAMIC_NO_PIC_P
	      && (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
		  || GET_CODE (XEXP (orig, 0)) == LABEL_REF))
	    {
#if defined (TARGET_TOC)	/* ppc  */
	      rtx temp_reg = (no_new_pseudos) ? reg : gen_reg_rtx (Pmode);
	      rtx asym = XEXP (orig, 0);
	      rtx mem;

	      emit_insn (gen_macho_high (temp_reg, asym));
	      mem = gen_rtx_MEM (GET_MODE (orig),
				 gen_rtx_LO_SUM (Pmode, temp_reg, asym));
	      MEM_READONLY_P (mem) = 1;
	      emit_insn (gen_rtx_SET (VOIDmode, reg, mem));
#else
	      /* Some other CPU -- WriteMe! but right now there are no other platform that can use dynamic-no-pic  */
	      abort ();
#endif
	      pic_ref = reg;
	    }
	  else
	  if (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
	      || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	    {
	      rtx offset = gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode,
							 XEXP (orig, 0),
							 pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
	      /* Generating a new reg may expose opportunities for
		 common subexpression elimination.  */
              rtx hi_sum_reg = no_new_pseudos ? reg : gen_reg_rtx (Pmode);
	      rtx mem;
	      rtx insn;
	      rtx sum;
	      
	      sum = gen_rtx_HIGH (Pmode, offset);
	      if (! MACHO_DYNAMIC_NO_PIC_P)
		sum = gen_rtx_PLUS (Pmode, pic_offset_table_rtx, sum);

	      emit_insn (gen_rtx_SET (Pmode, hi_sum_reg, sum));

	      mem = gen_rtx_MEM (GET_MODE (orig),
				 gen_rtx_LO_SUM (Pmode, 
						 hi_sum_reg, offset));
	      MEM_READONLY_P (mem) = 1;
	      insn = emit_insn (gen_rtx_SET (VOIDmode, reg, mem));
	      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_EQUAL, pic_ref, 
						    REG_NOTES (insn));

	      pic_ref = reg;
#else
	      emit_insn (gen_rtx_USE (VOIDmode,
				      gen_rtx_REG (Pmode, 
						   PIC_OFFSET_TABLE_REGNUM)));

	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_HIGH (Pmode,
						    gen_rtx_CONST (Pmode, 
								   offset))));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_LO_SUM (Pmode, reg,
					   gen_rtx_CONST (Pmode, offset))));
	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic_offset_table_rtx, reg);
#endif
	    }
	  else
#endif  /* HAVE_lo_sum */
	    {
	      rtx pic = pic_offset_table_rtx;
	      if (GET_CODE (pic) != REG)
		{
		  emit_move_insn (reg, pic);
		  pic = reg;
		}
#if 0
	      emit_insn (gen_rtx_USE (VOIDmode,
				      gen_rtx_REG (Pmode, 
						   PIC_OFFSET_TABLE_REGNUM)));
#endif

	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic,
				      gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode,
							 XEXP (orig, 0),
							 pic_base)));
	    }

#if !defined (TARGET_TOC)
	  emit_move_insn (reg, pic_ref);
	  pic_ref = gen_rtx_MEM (GET_MODE (orig), reg);
#endif
	  MEM_READONLY_P (pic_ref) = 1;
	}
      else
	{

#ifdef HAVE_lo_sum
	  if (GET_CODE (orig) == SYMBOL_REF
	      || GET_CODE (orig) == LABEL_REF)
	    {
	      rtx offset = gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode, 
							 orig, pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
              rtx hi_sum_reg;

	      if (reg == 0)
		{
		  if (reload_in_progress)
		    abort ();
		  else
		    reg = gen_reg_rtx (Pmode);
		}

	      hi_sum_reg = reg;

	      emit_insn (gen_rtx_SET (Pmode, hi_sum_reg,
				      (MACHO_DYNAMIC_NO_PIC_P)
				      ? gen_rtx_HIGH (Pmode, offset)
				      : gen_rtx_PLUS (Pmode,
						      pic_offset_table_rtx,
						      gen_rtx_HIGH (Pmode, 
								    offset))));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_LO_SUM (Pmode,
						      hi_sum_reg, offset)));
	      pic_ref = reg;
#else
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_HIGH (Pmode, offset)));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_LO_SUM (Pmode, reg, offset)));
	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic_offset_table_rtx, reg);
#endif
	    }
	  else
#endif  /*  HAVE_lo_sum  */
	    {
	      if (GET_CODE (orig) == REG)
		{
		  return orig;
		}
	      else
		{
		  rtx pic = pic_offset_table_rtx;
		  if (GET_CODE (pic) != REG)
		    {
		      emit_move_insn (reg, pic);
		      pic = reg;
		    }
#if 0
		  emit_insn (gen_rtx_USE (VOIDmode,
					  pic_offset_table_rtx));
#endif
		  pic_ref = gen_rtx_PLUS (Pmode,
					  pic,
					  gen_rtx_CONST (Pmode,
					      gen_rtx_MINUS (Pmode,
							     orig, pic_base)));
		}
	    }
	}

      if (GET_CODE (pic_ref) != REG)
        {
          if (reg != 0)
            {
              emit_move_insn (reg, pic_ref);
              return reg;
            }
          else
            {
              return force_reg (mode, pic_ref);
            }
        }
      else
        {
          return pic_ref;
        }
    }

  else if (GET_CODE (orig) == SYMBOL_REF)
    return orig;

  else if (GET_CODE (orig) == PLUS
	   && (GET_CODE (XEXP (orig, 0)) == MEM
	       || GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	   && XEXP (orig, 0) != pic_offset_table_rtx
	   && GET_CODE (XEXP (orig, 1)) != REG)

    {
      rtx base;
      int is_complex = (GET_CODE (XEXP (orig, 0)) == MEM);

      base = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
      orig = machopic_legitimize_pic_address (XEXP (orig, 1),
					      Pmode, (base == reg ? 0 : reg));
      if (GET_CODE (orig) == CONST_INT)
	{
	  pic_ref = plus_constant (base, INTVAL (orig));
	  is_complex = 1;
	}
      else
	pic_ref = gen_rtx_PLUS (Pmode, base, orig);

      if (reg && is_complex)
	{
	  emit_move_insn (reg, pic_ref);
	  pic_ref = reg;
	}
      /* Likewise, should we set special REG_NOTEs here?  */
    }

  else if (GET_CODE (orig) == CONST)
    {
      return machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
    }

  else if (GET_CODE (orig) == MEM
	   && GET_CODE (XEXP (orig, 0)) == SYMBOL_REF)
    {
      rtx addr = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
      addr = replace_equiv_address (orig, addr);
      emit_move_insn (reg, addr);
      pic_ref = reg;
    }

  return pic_ref;
}

/* Output the stub or non-lazy pointer in *SLOT, if it has been used.
   DATA is the FILE* for assembly output.  Called from
   htab_traverse.  */

static int
machopic_output_indirection (void **slot, void *data)
{
  machopic_indirection *p = *((machopic_indirection **) slot);
  FILE *asm_out_file = (FILE *) data;
  rtx symbol;
  const char *sym_name;
  const char *ptr_name;
  
  if (!p->used)
    return 1;

  symbol = p->symbol;
  sym_name = XSTR (symbol, 0);
  ptr_name = IDENTIFIER_POINTER (p->ptr_name);
  
  if (p->stub_p)
    {
      char *sym;
      char *stub;

      sym = alloca (strlen (sym_name) + 2);
      if (sym_name[0] == '*' || sym_name[0] == '&')
	strcpy (sym, sym_name + 1);
      else if (sym_name[0] == '-' || sym_name[0] == '+')
	strcpy (sym, sym_name);
      else
	sprintf (sym, "%s%s", user_label_prefix, sym_name);

      stub = alloca (strlen (ptr_name) + 2);
      if (ptr_name[0] == '*' || ptr_name[0] == '&')
	strcpy (stub, ptr_name + 1);
      else
	sprintf (stub, "%s%s", user_label_prefix, ptr_name);

      machopic_output_stub (asm_out_file, sym, stub);    
    }
  else if (machopic_symbol_defined_p (symbol))
    {
      data_section ();
      assemble_align (GET_MODE_ALIGNMENT (Pmode));
      assemble_label (ptr_name);
      assemble_integer (gen_rtx_SYMBOL_REF (Pmode, sym_name),
			GET_MODE_SIZE (Pmode),
			GET_MODE_ALIGNMENT (Pmode), 1);
    }
  else
    {
      machopic_nl_symbol_ptr_section ();
      assemble_name (asm_out_file, ptr_name);
      fprintf (asm_out_file, ":\n");
      
      fprintf (asm_out_file, "\t.indirect_symbol ");
      assemble_name (asm_out_file, sym_name);
      fprintf (asm_out_file, "\n");
      
      assemble_integer (const0_rtx, GET_MODE_SIZE (Pmode),
			GET_MODE_ALIGNMENT (Pmode), 1);
    }
  
  return 1;
}

void
machopic_finish (FILE *asm_out_file)
{
  if (machopic_indirections)
    htab_traverse_noresize (machopic_indirections, 
			    machopic_output_indirection,
			    asm_out_file);
}

int
machopic_operand_p (rtx op)
{
  if (MACHOPIC_JUST_INDIRECT)
    {
      while (GET_CODE (op) == CONST)
	op = XEXP (op, 0);

      if (GET_CODE (op) == SYMBOL_REF)
	return machopic_symbol_defined_p (op);
      else
	return 0;
    }

  while (GET_CODE (op) == CONST)
    op = XEXP (op, 0);

  if (GET_CODE (op) == MINUS
      && GET_CODE (XEXP (op, 0)) == SYMBOL_REF
      && GET_CODE (XEXP (op, 1)) == SYMBOL_REF
      && machopic_symbol_defined_p (XEXP (op, 0))
      && machopic_symbol_defined_p (XEXP (op, 1)))
      return 1;

  return 0;
}

/* This function records whether a given name corresponds to a defined
   or undefined function or variable, for machopic_classify_ident to
   use later.  */

void
darwin_encode_section_info (tree decl, rtx rtl, int first ATTRIBUTE_UNUSED)
{
  rtx sym_ref;

  /* Do the standard encoding things first.  */
  default_encode_section_info (decl, rtl, first);

  if (TREE_CODE (decl) != FUNCTION_DECL && TREE_CODE (decl) != VAR_DECL)
    return;

  sym_ref = XEXP (rtl, 0);
  if (TREE_CODE (decl) == VAR_DECL)
    SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_VARIABLE;

  if (!DECL_EXTERNAL (decl)
      && (!TREE_PUBLIC (decl) || (!DECL_ONE_ONLY (decl) && !DECL_WEAK (decl)))
      && ((TREE_STATIC (decl)
	   && (!DECL_COMMON (decl) || !TREE_PUBLIC (decl)))
	  || (!DECL_COMMON (decl) && DECL_INITIAL (decl)
	      && DECL_INITIAL (decl) != error_mark_node)))
    SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_DEFINED;
}

void
darwin_make_decl_one_only (tree decl)
{
  static const char *text_section = "__TEXT,__textcoal_nt,coalesced,no_toc";
  static const char *data_section = "__DATA,__datacoal_nt,coalesced,no_toc";

  const char *sec = TREE_CODE (decl) == FUNCTION_DECL
    ? text_section
    : data_section;
  TREE_PUBLIC (decl) = 1;
  DECL_ONE_ONLY (decl) = 1;
  DECL_SECTION_NAME (decl) = build_string (strlen (sec), sec);
}

void
darwin_mark_decl_preserved (const char *name)
{
  fprintf (asm_out_file, ".no_dead_strip ");
  assemble_name (asm_out_file, name);
  fputc ('\n', asm_out_file);
}

void
machopic_select_section (tree exp, int reloc,
			 unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  void (*base_function)(void);

  if (decl_readonly_section_1 (exp, reloc, MACHOPIC_INDIRECT))
    base_function = readonly_data_section;
  else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))
    base_function = const_data_section;
  else
    base_function = data_section;

  if (TREE_CODE (exp) == STRING_CST
      && ((size_t) TREE_STRING_LENGTH (exp)
	  == strlen (TREE_STRING_POINTER (exp)) + 1))
    cstring_section ();
  else if ((TREE_CODE (exp) == INTEGER_CST || TREE_CODE (exp) == REAL_CST)
	   && flag_merge_constants)
    {
      tree size = TYPE_SIZE (TREE_TYPE (exp));

      if (TREE_CODE (size) == INTEGER_CST &&
	  TREE_INT_CST_LOW (size) == 4 &&
	  TREE_INT_CST_HIGH (size) == 0)
	literal4_section ();
      else if (TREE_CODE (size) == INTEGER_CST &&
	       TREE_INT_CST_LOW (size) == 8 &&
	       TREE_INT_CST_HIGH (size) == 0)
	literal8_section ();
      else
	base_function ();
    }
  else if (TREE_CODE (exp) == CONSTRUCTOR
	   && TREE_TYPE (exp)
	   && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE
	   && TYPE_NAME (TREE_TYPE (exp)))
    {
      tree name = TYPE_NAME (TREE_TYPE (exp));
      if (TREE_CODE (name) == TYPE_DECL)
	name = DECL_NAME (name);
      if (!strcmp (IDENTIFIER_POINTER (name), "NSConstantString"))
	objc_constant_string_object_section ();
      else if (!strcmp (IDENTIFIER_POINTER (name), "NXConstantString"))
	objc_string_object_section ();
      else
	base_function ();
    }
  else if (TREE_CODE (exp) == VAR_DECL &&
	   DECL_NAME (exp) &&
	   TREE_CODE (DECL_NAME (exp)) == IDENTIFIER_NODE &&
	   IDENTIFIER_POINTER (DECL_NAME (exp)) &&
	   !strncmp (IDENTIFIER_POINTER (DECL_NAME (exp)), "_OBJC_", 6))
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (exp));

      if (!strncmp (name, "_OBJC_CLASS_METHODS_", 20))
	objc_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_METHODS_", 23))
	objc_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_CLASS_METHODS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_INSTANCE_METHODS_", 23))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_VARIABLES_", 22))
	objc_class_vars_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_VARIABLES_", 25))
	objc_instance_vars_section ();
      else if (!strncmp (name, "_OBJC_CLASS_PROTOCOLS_", 22))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_NAME_", 17))
	objc_class_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_NAME_", 20))
	objc_meth_var_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_TYPE_", 20))
	objc_meth_var_types_section ();
      else if (!strncmp (name, "_OBJC_CLASS_REFERENCES", 22))
	objc_cls_refs_section ();
      else if (!strncmp (name, "_OBJC_CLASS_", 12))
	objc_class_section ();
      else if (!strncmp (name, "_OBJC_METACLASS_", 16))
	objc_meta_class_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_", 15))
	objc_category_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_REFERENCES", 25))
	objc_selector_refs_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_FIXUP", 20))
	objc_selector_fixup_section ();
      else if (!strncmp (name, "_OBJC_SYMBOLS", 13))
	objc_symbols_section ();
      else if (!strncmp (name, "_OBJC_MODULES", 13))
	objc_module_info_section ();
      else if (!strncmp (name, "_OBJC_IMAGE_INFO", 16))
	objc_image_info_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_INSTANCE_METHODS_", 32))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_CLASS_METHODS_", 29))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_REFS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_", 15))
	objc_protocol_section ();
      else
	base_function ();
    }
  else
    base_function ();
}

/* This can be called with address expressions as "rtx".
   They must go in "const".  */

void
machopic_select_rtx_section (enum machine_mode mode, rtx x,
			     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  if (GET_MODE_SIZE (mode) == 8)
    literal8_section ();
  else if (GET_MODE_SIZE (mode) == 4
	   && (GET_CODE (x) == CONST_INT
	       || GET_CODE (x) == CONST_DOUBLE))
    literal4_section ();
  else if (MACHOPIC_INDIRECT
	   && (GET_CODE (x) == SYMBOL_REF
	       || GET_CODE (x) == CONST
	       || GET_CODE (x) == LABEL_REF))
    const_data_section ();
  else
    const_section ();
}

void
machopic_asm_out_constructor (rtx symbol, int priority ATTRIBUTE_UNUSED)
{
  if (MACHOPIC_INDIRECT)
    mod_init_section ();
  else
    constructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .constructors_used\n");
}

void
machopic_asm_out_destructor (rtx symbol, int priority ATTRIBUTE_UNUSED)
{
  if (MACHOPIC_INDIRECT)
    mod_term_section ();
  else
    destructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .destructors_used\n");
}

void
darwin_globalize_label (FILE *stream, const char *name)
{
  if (!!strncmp (name, "_OBJC_", 6))
    default_globalize_label (stream, name);
}

void
darwin_asm_named_section (const char *name, unsigned int flags ATTRIBUTE_UNUSED)
{
  if (flag_reorder_blocks_and_partition)
    fprintf (asm_out_file, SECTION_FORMAT_STRING, name);
  else
    fprintf (asm_out_file, ".section %s\n", name);
}

unsigned int
darwin_section_type_flags (tree decl, const char *name, int reloc)
{
  unsigned int flags = default_section_type_flags (decl, name, reloc);
 
  /* Weak or linkonce variables live in a writable section.  */
  if (decl != 0 && TREE_CODE (decl) != FUNCTION_DECL
      && (DECL_WEAK (decl) || DECL_ONE_ONLY (decl)))
    flags |= SECTION_WRITE;
  
  return flags;
}              

void 
darwin_unique_section (tree decl, int reloc ATTRIBUTE_UNUSED)
{
  /* Darwin does not use unique sections.  However, the target's
     unique_section hook is called for linkonce symbols.  We need
     to set an appropriate section for such symbols. */
  if (DECL_ONE_ONLY (decl) && !DECL_SECTION_NAME (decl))
    darwin_make_decl_one_only (decl);
}

#define HAVE_DEAD_STRIP 0

static void
no_dead_strip (FILE *file, const char *lab)
{
  if (HAVE_DEAD_STRIP)
    fprintf (file, ".no_dead_strip %s\n", lab);
}

/* Emit a label for an FDE, making it global and/or weak if appropriate. 
   The third parameter is nonzero if this is for exception handling.
   The fourth parameter is nonzero if this is just a placeholder for an
   FDE that we are omitting. */

void 
darwin_emit_unwind_label (FILE *file, tree decl, int for_eh, int empty)
{
  tree id = DECL_ASSEMBLER_NAME (decl)
    ? DECL_ASSEMBLER_NAME (decl)
    : DECL_NAME (decl);

  const char *prefix = "_";
  const int prefix_len = 1;

  const char *base = IDENTIFIER_POINTER (id);
  unsigned int base_len = IDENTIFIER_LENGTH (id);

  const char *suffix = ".eh";

  int need_quotes = name_needs_quotes (base);
  int quotes_len = need_quotes ? 2 : 0;
  char *lab;

  if (! for_eh)
    suffix = ".eh1";

  lab = xmalloc (prefix_len + base_len + strlen (suffix) + quotes_len + 1);
  lab[0] = '\0';

  if (need_quotes)
    strcat(lab, "\"");
  strcat(lab, prefix);
  strcat(lab, base);
  strcat(lab, suffix);
  if (need_quotes)
    strcat(lab, "\"");

  if (TREE_PUBLIC (decl))
    fprintf (file, "%s %s\n",
	     (DECL_VISIBILITY (decl) != VISIBILITY_HIDDEN
	      ? ".globl"
	      : ".private_extern"),
	     lab);

  if (DECL_ONE_ONLY (decl) && TREE_PUBLIC (decl))
    fprintf (file, ".weak_definition %s\n", lab);

  if (empty)
    {
      fprintf (file, "%s = 0\n", lab);

      /* Mark the absolute .eh and .eh1 style labels as needed to
	 ensure that we don't dead code strip them and keep such
	 labels from another instantiation point until we can fix this
	 properly with group comdat support.  */
      no_dead_strip (file, lab);
    }
  else
    fprintf (file, "%s:\n", lab);

  free (lab);
}

/* Generate a PC-relative reference to a Mach-O non-lazy-symbol.  */ 

void
darwin_non_lazy_pcrel (FILE *file, rtx addr)
{
  const char *nlp_name;

  if (GET_CODE (addr) != SYMBOL_REF)
    abort ();

  nlp_name = machopic_indirection_name (addr, /*stub_p=*/false);
  fputs ("\t.long\t", file);
  ASM_OUTPUT_LABELREF (file, nlp_name);
  fputs ("-.", file);
}

/* Emit an assembler directive to set visibility for a symbol.  The
   only supported visibilities are VISIBILITY_DEFAULT and
   VISIBILITY_HIDDEN; the latter corresponds to Darwin's "private
   extern".  There is no MACH-O equivalent of ELF's
   VISIBILITY_INTERNAL or VISIBILITY_PROTECTED. */

void 
darwin_assemble_visibility (tree decl, int vis)
{
  if (vis == VISIBILITY_DEFAULT)
    ;
  else if (vis == VISIBILITY_HIDDEN)
    {
      fputs ("\t.private_extern ", asm_out_file);
      assemble_name (asm_out_file,
		     (IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl))));
      fputs ("\n", asm_out_file);
    }
  else
    warning ("internal and protected visibility attributes not supported"
	     "in this configuration; ignored");
}

/* Output a difference of two labels that will be an assembly time
   constant if the two labels are local.  (.long lab1-lab2 will be
   very different if lab1 is at the boundary between two sections; it
   will be relocated according to the second section, not the first,
   so one ends up with a difference between labels in different
   sections, which is bad in the dwarf2 eh context for instance.)  */

static int darwin_dwarf_label_counter;

void
darwin_asm_output_dwarf_delta (FILE *file, int size ATTRIBUTE_UNUSED,
			       const char *lab1, const char *lab2)
{
  int islocaldiff = (lab1[0] == '*' && lab1[1] == 'L'
		     && lab2[0] == '*' && lab2[1] == 'L');

  if (islocaldiff)
    fprintf (file, "\t.set L$set$%d,", darwin_dwarf_label_counter);
  else
    fprintf (file, "\t%s\t", ".long");
  assemble_name (file, lab1);
  fprintf (file, "-");
  assemble_name (file, lab2);
  if (islocaldiff)
    fprintf (file, "\n\t.long L$set$%d", darwin_dwarf_label_counter++);
}

void
darwin_file_end (void)
{
  machopic_finish (asm_out_file);
  if (strcmp (lang_hooks.name, "GNU C++") == 0)
    {
      constructor_section ();
      destructor_section ();
      ASM_OUTPUT_ALIGN (asm_out_file, 1);
    }
}

#include "gt-darwin.h"
