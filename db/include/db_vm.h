/*
 * db_vm.h
 *
 *  Created on: Aug 16, 2013
 *      Author: sander
 */

#ifndef DB_VM_H_
#define DB_VM_H_

#include "stdint.h"
#include "db_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Instruction width postfixes
 * B	byte   (8 byte)
 * S	short  (16 bit)
 * L	long   (32 bit)
 * D	double (64 bit)
 *
 ** Instruction operand postfixes
 * V	value
 * R	register (addressed by 16 bit operand)
 * P	pointer  (word)
 * Q	pointer  (stored in registry, addressed by 16 bit)
 *
 ** Instruction type postfixes
 * I	signed integer
 * U	unsigned integer
 * F	floating point
 * S	string
 *
 ** Instruction type postfixes for collections
 * A	array
 * S	sequence
 * L	list
 * M	map
 */

#define VM_OPERAND(op,code)\
    DB_VM_##op##_##code\

#define VM_OPERAND_PQRV(op,type,lvalue)\
    VM_OPERAND(op, type##lvalue##V),\
    VM_OPERAND(op, type##lvalue##R),\
    VM_OPERAND(op ,type##lvalue##P),\
    VM_OPERAND(op, type##lvalue##Q)

#define VM_OPERAND_PQR(op,type,lvalue)\
    VM_OPERAND(op, type##lvalue##R),\
    VM_OPERAND(op ,type##lvalue##P),\
    VM_OPERAND(op, type##lvalue##Q)

#define VM_1OP_PQRV(op)\
    VM_OPERAND_PQR(op,B,),\
    VM_OPERAND_PQR(op,S,),\
    VM_OPERAND_PQR(op,L,),\
    VM_OPERAND_PQR(op,D,),\
    VM_OPERAND(op,BV),\
    VM_OPERAND(op,SV),\
    VM_OPERAND(op,LV),\
    VM_OPERAND(op,DV)

#define VM_1OP_ANY(op)\
    VM_OPERAND_PQR(op,B,),\
    VM_OPERAND_PQR(op,S,),\
    VM_OPERAND_PQR(op,L,),\
    VM_OPERAND_PQR(op,D,),\
    VM_OPERAND(op,BV),\
    VM_OPERAND(op,SV),\
    VM_OPERAND(op,LV),\
	/* Double values cannot be encoded together with a type for the any. Therefore the only three \
	 * types that can occur with DV are hard-encoded in the following three instructions. */\
    VM_OPERAND(op,DVU),\
    VM_OPERAND(op,DVI),\
    VM_OPERAND(op,DVF)

#define VM_1OP(op)\
    VM_OPERAND_PQR(op,B,),\
    VM_OPERAND_PQR(op,S,),\
    VM_OPERAND_PQR(op,L,),\
    VM_OPERAND_PQR(op,D,)

/* Expand operations for every type to match staged values */
#define VM_1OP_COND(op)\
	VM_OPERAND_PQR(op##B,B,),\
	VM_OPERAND_PQR(op##S,B,),\
	VM_OPERAND_PQR(op##L,B,),\
	VM_OPERAND_PQR(op##D,B,)

#define VM_1OP_COND_LD(op)\
	VM_OPERAND_PQR(op##L,B,),\
	VM_OPERAND_PQR(op##D,B,)

#define VM_LVALUE(op,type,postfix)\
	VM_OPERAND_##postfix(op,type,R),\
	VM_OPERAND_##postfix(op,type,P),\
	VM_OPERAND_##postfix(op,type,Q)

#define VM_LVALUE_V(op,type,postfix)\
	VM_LVALUE(op,type,postfix),\
	VM_OPERAND_##postfix(op,type,V)

#define VM_2OP(op,postfix)\
	VM_LVALUE(op,B,postfix),\
	VM_LVALUE(op,S,postfix),\
	VM_LVALUE(op,L,postfix),\
	VM_LVALUE(op,D,postfix)

#define VM_2OP_V(op,postfix)\
	VM_LVALUE_V(op,B,postfix),\
	VM_LVALUE_V(op,S,postfix),\
	VM_LVALUE_V(op,L,postfix),\
	VM_LVALUE_V(op,D,postfix)

#define VM_2OP_SLD(op,postfix)\
    VM_LVALUE(op,S,postfix),\
    VM_LVALUE(op,L,postfix),\
    VM_LVALUE(op,D,postfix)

#define VM_2OP_BLD(op,postfix)\
    VM_LVALUE(op,B,postfix),\
    VM_LVALUE(op,L,postfix),\
    VM_LVALUE(op,D,postfix)

#define VM_2OP_BSD(op,postfix)\
    VM_LVALUE(op,B,postfix),\
    VM_LVALUE(op,S,postfix),\
    VM_LVALUE(op,D,postfix)

#define VM_2OP_BSL(op,postfix)\
    VM_LVALUE(op,B,postfix),\
    VM_LVALUE(op,S,postfix),\
    VM_LVALUE(op,L,postfix)

#define VM_2OP_LD(op,postfix)\
	VM_LVALUE(op,L,postfix),\
	VM_LVALUE(op,D,postfix)

#define VM_2OP_L(op,postfix)\
    VM_LVALUE(op,L,postfix)

#define VM_2OP_D(op,postfix)\
    VM_LVALUE(op,D,postfix)


#define VM_2OPV_L(op,postfix)\
	VM_OPERAND_##postfix(op,L,V),\
    VM_LVALUE(op,L,postfix)

typedef enum db_vmOpKind {
	DB_VM_NOOP,

	/* Copy a value to a variable/object */
	VM_2OP(SET,PQRV),

	/* Special set instruction that takes the address of a register so calculations can be performed on it.
	 * This is useful when calculating dynamic offsets, for example when using arrays i.c.w. a variable index. */
	DB_VM_SET_LRX,

	/* Specialized SET for references */
	VM_2OP_L(SETREF,PQRV),

	/* Specialized SET for strings */
	VM_2OP_L(SETSTR,PQRV),
	VM_2OP_L(SETSTRDUP,PQRV),

	/* Initialize register-memory to zero */
	DB_VM_ZERO,

	/* Call initializer */
    DB_VM_INIT,

	/* Inc & dec */
	VM_1OP(INC),
	VM_1OP(DEC),

	/* Integer operations */
	VM_2OP(ADDI,PQRV),
	VM_2OP(SUBI,PQRV),
	VM_2OP(MULI,PQRV),
	VM_2OP(DIVI,PQRV),
	VM_2OP(MODI,PQRV),

	/* Float operations */
	VM_2OP_LD(ADDF,PQRV),
	VM_2OP_LD(SUBF,PQRV),
	VM_2OP_LD(MULF,PQRV),
	VM_2OP_LD(DIVF,PQRV),

	/* Logical operators */
	VM_2OP(AND,PQRV),
	VM_2OP(XOR,PQRV),
	VM_2OP(OR,PQRV),
	VM_1OP(NOT),

	/* Shift operators */
	VM_2OP(SHIFT_LEFT,PQRV),
	VM_2OP(SHIFT_RIGHT,PQRV),

	/* Stage 1 operand */
	VM_1OP_PQRV(STAGE1),

	/* Stage 2 operands */
	VM_2OP_V(STAGE2,PQRV),
    
    /* Stage 1 double operand in stage variable 2 */
    DB_VM_STAGE12_DP,
    DB_VM_STAGE12_DV,
    
	/* Comparisons */
	VM_1OP_COND(CAND),
	VM_1OP_COND(COR),
	VM_1OP_COND(CNOT),
	VM_1OP_COND(CEQ),
	VM_1OP_COND(CNEQ),

	/* Signed comparisons */
	VM_1OP_COND(CGTI),
	VM_1OP_COND(CLTI),
	VM_1OP_COND(CGTEQI),
	VM_1OP_COND(CLTEQI),

	/* Unsigned comparisons */
	VM_1OP_COND(CGTU),
	VM_1OP_COND(CLTU),
	VM_1OP_COND(CGTEQU),
	VM_1OP_COND(CLTEQU),

	/* Float comparisons */
	VM_1OP_COND_LD(CGTF),
	VM_1OP_COND_LD(CLTF),
	VM_1OP_COND_LD(CGTEQF),
	VM_1OP_COND_LD(CLTEQF),

	/* String comparisons */
	VM_OPERAND_PQR(CEQSTR,B,),
	VM_OPERAND_PQR(CNEQSTR,B,),

	/* Program control */
    VM_1OP(JEQ),
    VM_1OP(JNEQ),
	DB_VM_JUMP,

	/* Calculate member-address from register base */
	DB_VM_MEMBER, /* Takes destination register, base register and offset */

	/* Collections */
	VM_OPERAND_PQRV(ELEMA,L,R), /* Takes register(1), elementsize(3) and index variable(2) */
	VM_OPERAND_PQRV(ELEMS,L,R), /* Takes register(1), elementsize(3) and index variable(2) */
	VM_OPERAND_PQRV(ELEML,L,R), /* Takes register(1) and index variable(2) */
	VM_OPERAND_PQRV(ELEMLX,L,R),/* Takes register(1) and index variable(2) - obtains pointer to listnode */
	VM_OPERAND_PQRV(ELEMM,L,R), /* Takes register(1) and index variable(2) */
    VM_OPERAND_PQRV(ELEMMX,L,R), /* Takes register(1) and index variable(2) - obtains pointer to mapnode*/
    
	/* Calls */
	VM_1OP_PQRV(PUSH),
	VM_1OP(PUSHX), 			        /* Push address of value */
	VM_OPERAND_PQRV(PUSHANY,L,),	/* Push value as any */
	VM_1OP_ANY(PUSHANYX),	        /* Push address of value as any */

	VM_OPERAND_PQR(CALL,L,),		/* Call function with returnvalue */
	VM_OPERAND_PQR(CALLVM,L,),		/* Call vm function with returnvalue */
    DB_VM_CALLVOID,					/* Call function with void returnvalue */
    DB_VM_CALLVMVOID,				/* Call vm function with void returnvalue */
	VM_1OP(RET),					/* Return value smaller than 8 bytes */
	VM_OPERAND_PQR(RETCPY,L,), 		/* Return value larger than 8 bytes */

	/* Casts */
	VM_2OP(I2FL,PQRV),
	VM_2OP(I2FD,PQRV),
	VM_2OP_LD(F2IB,PQRV),
	VM_2OP_LD(F2IS,PQRV),
	VM_2OP_LD(F2IL,PQRV),
	VM_2OP_LD(F2ID,PQRV),
	VM_2OP(I2S,PQRV),
	VM_2OP(U2S,PQRV),
	VM_2OP_LD(F2S,PQRV),
    
    /* Primitive casts */
    VM_2OP(PCAST,PQR),

	/* Reference casting */
	VM_2OP_L(CAST,PQRV),

	/* String manipulations */
	VM_2OPV_L(STRCAT,PQRV),
	VM_2OP_L(STRCPY,PQRV),

    /* Memory management */
	VM_LVALUE(NEW,L,PQRV),
	VM_OPERAND_PQRV(DEALLOC,L,),
	VM_OPERAND_PQRV(KEEP,L,),
	VM_OPERAND_PQRV(FREE,L,),

	/* Object state */
	VM_OPERAND_PQRV(DEFINE,L,),

	/* Notifications */
	VM_OPERAND_PQRV(UPDATE,L,),
	VM_OPERAND_PQRV(UPDATEBEGIN,L,),
	VM_OPERAND_PQRV(UPDATEEND,L,),
	VM_2OP_L(UPDATEFROM,PQR),
	VM_2OP_L(UPDATEENDFROM,PQR),
	VM_OPERAND_PQRV(UPDATECANCEL,L,),

	/* Waiting */
	VM_OPERAND_PQRV(WAITFOR,L,), /* Add objects to wait-queue */
	VM_2OP_L(WAIT,PQRV), /* Do the actual wait */

	DB_VM_STOP /* Stop the current program (or function) */
} db_vmOpKind;

typedef union db_vmParameter16 {
    struct {
        uint16_t _1;
        uint16_t _2;
    } b;
    uint16_t s;
    intptr_t w;
}db_vmParameter16;

typedef union db_vmParameter {
    db_vmParameter16 s;
    intptr_t w;
}db_vmParameter;

typedef struct db_vmOpAddr {
    uint16_t jump;
    db_vmParameter16 p;
}db_vmOpAddr;

typedef struct db_vmOp {
    intptr_t op; /* direct jump to address of next operation */
    db_vmParameter16 ic;
    db_vmParameter lo;
    db_vmParameter hi;
#ifdef DB_VM_DEBUG
    db_vmOpKind opKind; /* Actual operation kind. Only used for debugging purposes */
#endif
}db_vmOp;

typedef struct db_vmDebugInfo {
    uint32_t line;
}db_vmDebugInfo;
    
typedef struct db_vmProgram_s *db_vmProgram;
typedef struct db_vmProgram_s {
	db_vmOp *program;
    db_vmDebugInfo *debugInfo;
    db_object function;
    char *filename;
	uint32_t size;
	uint32_t maxSize;
	uint32_t storage;
	uint32_t stack;
	uint8_t translated;
}db_vmProgram_s;

int32_t db_vm_run(db_vmProgram program, void *result);
char *db_vmProgram_toString(db_vmProgram program, db_vmOp *addr);
db_vmProgram db_vmProgram_new(char *filename, db_object function);
void db_vmProgram_free(db_vmProgram program);
db_vmOp *db_vmProgram_addOp(db_vmProgram program, uint32_t line);

#ifdef __cplusplus
}
#endif

#endif /* DB_VM_H_ */
