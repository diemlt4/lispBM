
#include "eval0.h"

#include "symtab.h"
#include "heap0.h"
#include "built_in.h"

#include <stdio.h>
/* TODO: Experiment with evaluation */
cons_t *eval_(cons_t *, int); 

cons_t *eval(cons_t *cell) {
  return eval_(cell, 1); 
}

/* TODO: Think more about how various things are represented on the 
   heap. It must always be possible to tell a difference between 
   lists (a (b (c (d ()), dotted pair (a . b) etc. 
   Head position of a list has a special meaning as it is potentially 
   a function name. 
   
   Is it possible to tell the difference between (1 . (2 3 4)) and (1 2 3 4)?
   ~ This may be a non-problem~  
   
   [1 | POINTER] 
          - [2 | POINTER] 
	            - [3 | POINTER] 
		             - [ 4 | NIL ]

			     
			     
  Currently cannot tell apart (+ 1 + 2 3) and (+ 1 (+ 2 3))... 
  What should the representation of these be?


*/ 

cons_t *eval_(cons_t *cell, int head_position) {

  /* The result of evaluation will be a reduced expression 
     represented by heap allocated cells */ 
  cons_t *result = heap_allocate_cell(); 
  uint32_t type = 0;
  int head_is_symbol = 0; 
  cons_t *ptr;
  
  switch (GET_CAR_TYPE(cell->type))
    {
    case NIL:
      type = SET_CAR_TYPE(type, NIL);
      result->type = type;
      break; 
      
    case INTEGER:
      type = SET_CAR_TYPE(type,INTEGER);
      result->type = type;
      result->car.i = cell->car.i;
      printf(" CAR(%d) ", result->car.i);
      break;
     
    case FLOAT:
      type = SET_CAR_TYPE(type,FLOAT);
      result->type = type;
      result->car.f = cell->car.f;
      break; 

    case SYMBOL:
      if (head_position) head_is_symbol = 1;
      
      type = SET_CAR_TYPE(type, SYMBOL);
      result->type = type;
      result->car.s = cell->car.s;
      printf("CARSYM(%d) ", result->car.s);
      break;


      /* TODO This case is broken. 
	 if the car cell is a pointer 
	 evaluating it does not need to result in a pointer! 
	 Some more machinery is needed here to check what kind 
	 of result is generated by this evaluation is needed. 
	
	 WIll probably take the form of some cases. 
	 - is the result of evaluation a list or some "primitive"

      */
        
    case POINTER:
      type = SET_CAR_TYPE(type, POINTER); 
      printf(" CARPOINTER "); 
      if (cell->car.cons == NULL) return NULL; 
      ptr = eval_(cell->car.cons, 1);
      result->type = type;
      result->car.cons = ptr;
      
      break; 
    }

  
  switch(GET_CDR_TYPE(cell->type))
    {
    case NIL:
      printf("CDR NIL"); 
      type = SET_CDR_TYPE(type, NIL);
      result->type = type;
      break;
    case INTEGER:
      type = SET_CDR_TYPE(type, INTEGER);
      result->type = type;
      result->cdr.i = cell->cdr.i;
      printf(" CDR(%d) ", result->cdr.i);
      break;

    case FLOAT:
      type = SET_CDR_TYPE(type,FLOAT);
      result->type = type;
      result->cdr.f = cell->cdr.f;
      break; 

    case SYMBOL: 
      type = SET_CDR_TYPE(type, SYMBOL);
      result->type = type;
      result->cdr.s = cell->cdr.s;
      printf("%d ", result->cdr.s); 
      break;
      
    case POINTER:
      type = SET_CDR_TYPE(type,POINTER); 
      printf(" CDR POINTER ");
      if (cell->cdr.cons == NULL) { printf( "NULL CASE!\n");  return NULL; } 
      ptr = eval_(cell->cdr.cons, 0);
      result->type = type; 
      result->cdr.cons = ptr;
      
      break; 
    } 
   
  if (head_is_symbol) {
    /* potential function application */ 
    printf("head is symbol\n"); 
    /* hack */ 
    if ( result->car.s == 15438 ) {
      printf("applying add\n"); 
      cons_t *fun_res;

      fun_res = bi_add(result->cdr.cons); 
      return fun_res;
    }
  }

  return result;
}
  

