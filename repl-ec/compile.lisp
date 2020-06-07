
;; Work in progress
;; Experimenting with compilation of lispBM to bytecode
;; while peeking a lot in the SICP book.


;; Will need a way to set this definition to another value
(define compiler-symbols '())

(define all-regs '(env proc val argl cont))

(define is-symbol
  (lambda (exp) (= type-symbol (type-of exp))))

(define is-label
  (lambda (exp) (= (car exp) 'label)))

(define is-nil
  (lambda (exp) (= exp 'nil)))

(define is-number
  (lambda (exp)
    (let ((typ (type-of exp))) 
      (or (= typ type-i28)
	  (= typ type-u28)
	  (= typ type-i32)
	  (= typ type-u32)
	  (= typ type-float)))))

(define is-array
  (lambda (exp)
    (= (type-of exp) type-array)))

(define is-self-evaluating
  (lambda (exp)
    (or (is-number exp)
	(is-array  exp))))

(define label-counter 0)

(define incr-label
  (lambda () 
    (progn
      ;; Define used to redefine label-counter
      (define label-counter (+ 1 label-counter))
      label-counter)))

(define mk-label
  (lambda (name)
    (list 'label name (incr-label))))


(define mem
  (lambda (x xs)
    (if (is-nil xs)
	'nil
      (if (= x (car xs))
	  't
	(mem x (cdr xs))))))

(define list-union
   (lambda (s1 s2)
     (if (is-nil s1) s2
       (if (mem (car s1) s2)
	   (list-union (cdr s1) s2)
	 (cons (car s1) (list-union (cdr s1) s2))))))

(define list-diff
  (lambda (s1 s2)
    (if (is-nil s1) '()
      (if (mem (car s1) s2) (list-diff (cdr s1) s2)
	(cons (car s1) (list-diff (cdr s1) s2))))))

(define mk-instr-seq
  (lambda (needs mods stms)
    (list needs mods stms)))

(define empty-instr-seq (mk-instr-seq '() '() '()))

(define compile-linkage 
  (lambda (linkage)
    (if (= linkage 'return)
	(mk-instr-seq '(cont) '() '(jmpcnt))
      (if (= linkage 'next)
	  empty-instr-seq
	(mk-instr-seq '() '() `((jmpimm (label ,linkage))))))))

(define regs-needed
  (lambda (s)
    (if (is-label s)
	'()
      (car s))))

(define regs-modified
  (lambda (s)
    (if (is-label s)
	'()
      (car (cdr s)))))

(define statements
  (lambda (s)
    (if (is-label s)
	(list s)
      (car (cdr (cdr s))))))
      
(define needs-reg
  (lambda (s r)
    (mem r (regs-needed s))))

(define modifies-reg
  (lambda (s r)
    (mem r (regs-modified s))))


(define append-instr-seqs
  (let ((append-two
	 (lambda (s1 s2)
	   (mk-instr-seq
	    (list-union (regs-needed s1)
	   		(list-diff (regs-needed s2)
	   			   (regs-modified s1)))
	    (list-union (regs-modified s1)
	   		(regs-modified s2))
	    (append (statements s1) (statements s2))))))
    (lambda (seqs)
      (if (is-nil seqs)
      	  empty-instr-seq
      	(append-two (car seqs)
      		    (append-instr-seqs (cdr seqs)))))))
       
(define preserving
  (lambda (regs s1 s2)
    (if (is-nil regs)
	(append-instr-seqs (list s1 s2))
      (let ((first-reg (car regs)))
	(if (and (needs-reg s2 first-reg)
		 (modifies-reg s1 first-reg))
	    (preserving (cdr regs)
	     		(mk-instr-seq
	     		 (list-union (list first-reg)
	     			     (regs-needed s1))
	     		 (list-diff (regs-modified s1)
	     			    (list first-reg))
	     		 (append `((push ,first-reg))
	     			 (append (statements s1) `((pop ,first-reg)))))
	     		s2)

	  (preserving (cdr regs) s1 s2))))))

(define end-with-linkage
  (lambda (linkage seq)
    (preserving '(cont)
    		seq
    		(compile-linkage linkage))))
		
;; COMPILERS

(define compile-self-evaluating
  (lambda (exp target linkage)
    (end-with-linkage linkage
		      (mk-instr-seq '()
				    (list target)
				    `((movimm ,target ,exp))))))


(define compile-instr-list
  (lambda (exp target linkage)
    (if (is-self-evaluating exp)
	(compile-self-evaluating exp target linkage)
      '())))