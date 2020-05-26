/*
    Copyright 2020      Joel Svensson	svenssonjoel@yahoo.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "symrepr.h"
#include "heap.h"
#include "env.h"
#include "stack.h"
#include "fundamental.h"
#include "extensions.h"
#include "typedefs.h"
#include "ec_eval.h"

typedef enum {
  EXP_KIND_ERROR,
  EXP_SELF_EVALUATING,
  EXP_VARIABLE,
  EXP_QUOTED,
  EXP_DEFINE,
  EXP_LAMBDA,
  EXP_IF,
  EXP_NO_ARGS,
  EXP_APPLICATION,
  EXP_LET
} exp_kind;

typedef enum {
  CONT_DONE,
  CONT_DEFINE,
  CONT_SETUP_NO_ARG_APPLY,
  CONT_EVAL_ARGS,
  CONT_ACCUMULATE_ARG,
  CONT_ACCUMULATE_LAST_ARG
} continuation;

typedef enum {
  EVAL_DISPATCH,
  EVAL_CONTINUATION,
  EVAL_APPLY_DISPATCH
} eval_state;

/* Register machine:
 * cont : Continuation register (what to do when hitting a leaf)
 * unev : Hold something un-evaluated for a while
 * prg  : Keeps track of a list of expressions to evaluate (top-level)
 * exp  : Current expression
 * argl : List of evaluated arguments to function
 * val  : Final or intermediate result
 * fun  : Evaluated function (for application)
 */

typedef struct {
  uint32_t cont;
  VALUE env;
  VALUE unev;
  VALUE prg;
  VALUE exp;
  VALUE argl;
  VALUE val;
  VALUE fun;

  stack S;
} register_machine_t;

register_machine_t rm_state;
VALUE ec_eval_global_env;

VALUE ec_eval_get_env(void) {
  return ec_eval_global_env;
}

exp_kind kind_of(VALUE exp) {

  switch (type_of(exp)) {
  case VAL_TYPE_SYMBOL:
    if (!is_special(exp))
      return EXP_VARIABLE;
    // fall through
  case PTR_TYPE_BOXED_F:
  case PTR_TYPE_BOXED_U:
  case PTR_TYPE_BOXED_I:
  case VAL_TYPE_I:
  case VAL_TYPE_U:
  case VAL_TYPE_CHAR:
  case PTR_TYPE_ARRAY:
    return EXP_SELF_EVALUATING;
  case PTR_TYPE_CONS: {
    VALUE head = car(rm_state.exp);
    if (type_of(head) == VAL_TYPE_SYMBOL) {
      UINT sym_id = dec_sym(head);

      if (sym_id == symrepr_quote())
	return EXP_QUOTED;
      if (sym_id == symrepr_define())
	return EXP_DEFINE;
      if (sym_id == symrepr_progn())
	return EXP_APPLICATION;
      if (sym_id == symrepr_lambda())
	return EXP_LAMBDA;
      if (sym_id == symrepr_if())
	return EXP_IF;
      if (sym_id == symrepr_let())
	return EXP_LET;
      if (type_of(cdr(exp)) == VAL_TYPE_SYMBOL &&
	  dec_sym(cdr(exp)) == symrepr_nil()) {
	return EXP_NO_ARGS;
      }
    } // end if symbol
    return EXP_APPLICATION;
  } // end case PTR_TYPE_CONS:
  }
  return EXP_KIND_ERROR;
}

static inline bool last_operand(VALUE exp) {
  return (type_of(cdr(exp)) == VAL_TYPE_SYMBOL &&
	  dec_sym(cdr(exp)) == symrepr_nil());
}

static inline void eval_self_evaluating(eval_state *es) {
  rm_state.val = rm_state.exp;
  *es = EVAL_CONTINUATION;
}

static inline void eval_variable(eval_state *es) {
  if (is_special(rm_state.exp)) rm_state.val = rm_state.exp;
  else rm_state.val = env_lookup(rm_state.exp, rm_state.env);

  if (type_of(rm_state.val) == VAL_TYPE_SYMBOL &&
      dec_sym(rm_state.val) == symrepr_not_found()) {
    rm_state.val = env_lookup(rm_state.exp, ec_eval_global_env);
  }
  *es = EVAL_CONTINUATION;
}

static inline void eval_quoted(eval_state *es) {
  rm_state.val = car(cdr(rm_state.exp));
  *es = EVAL_CONTINUATION;
}

static inline void eval_define(eval_state *es) {
  rm_state.unev = car(cdr(rm_state.exp));
  rm_state.exp  = car(cdr(cdr(rm_state.exp)));
  push_u32_3(&rm_state.S,
	     rm_state.unev,
	     rm_state.env,
	     rm_state.cont);
  rm_state.cont = CONT_DEFINE;
  *es = EVAL_DISPATCH;
}

static inline void cont_define(eval_state *es) {
  pop_u32_3(&rm_state.S,
	    &rm_state.cont,
	    &rm_state.env,
	    &rm_state.unev);
  VALUE new_env = env_set(ec_eval_global_env,
			  rm_state.unev,
			  rm_state.val);
  ec_eval_global_env = new_env;

  // TODO: error checking and garbage collection
  rm_state.val = rm_state.unev;
  *es = EVAL_CONTINUATION;
}

static inline VALUE mkClosure(void) {

  VALUE env_end = cons(rm_state.env, enc_sym(symrepr_nil()));
  VALUE body    = cons(car(cdr(cdr(rm_state.exp))), env_end);
  VALUE params  = cons(car(cdr(rm_state.exp)),body);
  VALUE closure = cons(enc_sym(symrepr_closure()), params);

  //TODO: error checking and garbage collection
  return closure;
}

static inline void eval_lambda(eval_state *es) {
  rm_state.val = mkClosure();
  *es = EVAL_CONTINUATION;
}

static inline void eval_no_args(eval_state *es) {
  rm_state.exp = car(rm_state.exp);
  push_u32(&rm_state.S, rm_state.cont);
  rm_state.cont = CONT_SETUP_NO_ARG_APPLY;
  *es = EVAL_DISPATCH;
}

static inline void cont_setup_no_arg_apply(eval_state *es) {
  rm_state.fun = rm_state.val;
  rm_state.argl = enc_sym(symrepr_nil());
  *es = EVAL_APPLY_DISPATCH;
}

static inline void eval_application(eval_state *es) {
  rm_state.unev = cdr(rm_state.exp);
  rm_state.exp = car(rm_state.exp);
  push_u32_3(&rm_state.S, rm_state.cont, rm_state.env, rm_state.unev);
  rm_state.cont = CONT_EVAL_ARGS;
  *es = EVAL_DISPATCH;
}

static inline void eval_last_arg(eval_state *es) {
  rm_state.cont = CONT_ACCUMULATE_LAST_ARG;
  *es = EVAL_DISPATCH;
}

static inline void eval_arg_loop(eval_state *es) {
  push_u32(&rm_state.S, rm_state.argl);
  rm_state.exp = car(rm_state.unev);
  if (last_operand(rm_state.unev)) {
    eval_last_arg(es);
    return;
  }
  push_u32_2(&rm_state.S, rm_state.env, rm_state.unev);
  rm_state.cont = CONT_ACCUMULATE_ARG;
  *es = EVAL_DISPATCH;
}

static inline void cont_eval_args(eval_state *es) {
  pop_u32_2(&rm_state.S,&rm_state.unev, &rm_state.env);
  rm_state.fun = rm_state.val;
  push_u32(&rm_state.S,rm_state.fun);
  rm_state.argl = enc_sym(symrepr_nil());
  eval_arg_loop(es);
}

static inline void cont_accumulate_arg(eval_state *es) {
  pop_u32_3(&rm_state.S, &rm_state.unev, &rm_state.env, &rm_state.argl);
  rm_state.argl = cons(rm_state.val, rm_state.argl);  // TODO error checking and garbage collection
  rm_state.unev = cdr(rm_state.unev);
  eval_arg_loop(es);
}

static inline void cont_accumulate_last_arg(eval_state *es) {
  pop_u32(&rm_state.S, &rm_state.argl);
  rm_state.argl = cons(rm_state.val, rm_state.argl); // TODO error checking and garbage collection
  pop_u32(&rm_state.S, &rm_state.fun);
  *es = EVAL_APPLY_DISPATCH;
}

//TODO Args are in reversed order
static inline void eval_apply_fundamental(eval_state *es) {
  UINT count = 0;
  VALUE args = rm_state.argl;
  while (type_of(args) == PTR_TYPE_CONS) {
    push_u32(&rm_state.S, car(args));
    count ++;
    args = cdr(args);
  }
  UINT *fun_args = stack_ptr(&rm_state.S, count);
  rm_state.val = fundamental_exec(fun_args, count, rm_state.fun);
  stack_drop(&rm_state.S, count);
  pop_u32(&rm_state.S, &rm_state.cont);
  *es = EVAL_CONTINUATION;
}

static inline void eval_apply_closure(eval_state *es) {
  VALUE local_env = env_build_params_args(car(cdr(rm_state.fun)),
					  rm_state.argl,
					  car(cdr(cdr(cdr(rm_state.fun)))));
  //TODO: Error checking and garbage collection
  rm_state.env = local_env;
  rm_state.exp = car(cdr(cdr(rm_state.fun)));
  pop_u32(&rm_state.S, &rm_state.cont);
  *es = EVAL_DISPATCH;
}

static inline void eval_apply_extension(eval_state *es) {
  (void) es;
}

static inline void eval_apply_dispatch(eval_state *es) {
  if (is_fundamental(rm_state.fun)) eval_apply_fundamental(es);
  else if (is_closure(rm_state.fun)) eval_apply_closure(es);
  else if (is_extension(rm_state.fun)) eval_apply_extension(es);
  // TODO: else is an error. Set cont to done
}

static inline void cont_done(eval_state *es, bool *done) {
  if (type_of(rm_state.prg) != PTR_TYPE_CONS) {
    *done = true;
    return;
  }
  rm_state.exp = car(rm_state.prg);
  rm_state.prg = cdr(rm_state.prg);
  rm_state.cont = CONT_DONE;
  rm_state.env = enc_sym(symrepr_nil());
  rm_state.argl = enc_sym(symrepr_nil());
  rm_state.val = enc_sym(symrepr_nil());
  rm_state.fun = enc_sym(symrepr_nil());
  stack_clear(&rm_state.S);
  *done = false;
  *es = EVAL_DISPATCH;
}

void ec_eval(void) {

  eval_state es = EVAL_DISPATCH;

  bool done = false;

  while (!done) {

    switch(es) {
    case EVAL_DISPATCH:
      switch (kind_of(rm_state.exp)) {
      case EXP_SELF_EVALUATING: eval_self_evaluating(&es); break;
      case EXP_VARIABLE:        eval_variable(&es);        break;
      case EXP_QUOTED:          eval_quoted(&es);          break;
      case EXP_DEFINE:          eval_define(&es);          break;
      case EXP_NO_ARGS:         eval_no_args(&es);         break;
      case EXP_APPLICATION:     eval_application(&es);     break;
      case EXP_LAMBDA:          eval_lambda(&es);          break;
      case EXP_IF:                                         break;
      case EXP_LET:                                        break;
      case EXP_KIND_ERROR:      done = true;               break;
      }
      break;
    case EVAL_CONTINUATION:
      switch (rm_state.cont) {
      case CONT_DONE:                cont_done(&es, &done);         break;
      case CONT_DEFINE:              cont_define(&es);              break;
      case CONT_SETUP_NO_ARG_APPLY:  cont_setup_no_arg_apply(&es);  break;
      case CONT_EVAL_ARGS:           cont_eval_args(&es);           break;
      case CONT_ACCUMULATE_ARG:      cont_accumulate_arg(&es);      break;
      case CONT_ACCUMULATE_LAST_ARG: cont_accumulate_last_arg(&es); break;
      }
      break;
    case EVAL_APPLY_DISPATCH:  eval_apply_dispatch(&es); break;
    }
  }
}


VALUE ec_eval_program(VALUE prg) {

  rm_state.prg = cdr(prg);
  rm_state.exp = car(prg);
  rm_state.cont = CONT_DONE;
  rm_state.env = enc_sym(symrepr_nil());
  rm_state.argl = enc_sym(symrepr_nil());
  rm_state.val = enc_sym(symrepr_nil());
  rm_state.fun = enc_sym(symrepr_nil());
  stack_allocate(&rm_state.S, 256, false);
  ec_eval();

  stack_free(&rm_state.S);
  return rm_state.val;
}
