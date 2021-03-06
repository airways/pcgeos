/* Secode.c   The secode compiler/interpreter
 */

/* (c) COPYRIGHT 1993-98           NOMBAS, INC.
 *                                 64 SALEM ST.
 *                                 MEDFORD, MA 02155  USA
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Nombas, Inc. and is furnished under
 * license by Nombas, Inc.; this software may be used only in accordance
 * with the terms of said license.  This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of Nombas, Inc.
 *
 * This software is a Trade Secret of Nombas, Inc.
 *
 * This software may not be copied, transmitted, provided to or otherwise made
 * available to any other person, company, corporation or other entity except
 * as specified in the terms of said license.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred.
 *
 * The information contained herein is subject to change without notice and
 * should not be construed as a commitment by Nombas, Inc.
 */


#include "srccore.h"


/* SECODE Compiler/Interpreter
 *
 * 01/13/00 - updated comments, Rich Robinson
 *
 * A lot of rewriting has been going on for SE430/ECMA2.0 release.
 * I've done a *lot* of work to simplify the whole internal workings
 * of the core. The tokenizer no longer does a bunch of 'pre-parsing',
 * it just produces tokens. The core compiler is a lot smarter. It
 * defers accessing variables until it has to.
 *
 * So what does 'defer accessing variables' mean? Let's consider
 * the se420 method first. Remember, the entire engine is historic,
 * so in a lot of cases, poor code is not as dumb as it looks. It
 * may be simple to look back and say, "sure we just drop all this
 * and make it this way", but in practice it just isn't that easy.
 * At any rate, se420 had its parser based on its variables which
 * were quite historic. A variable was completely self-contained.
 * For instance, when you pushed a global variable onto the secode
 * stack, all the information necessary to read or write to that
 * variable was there. For instance, if you parsed:
 *
 *   var b = a;
 *
 * That basically came down to 'push b', 'push a', 'assign'.
 * Assign did 'get a's value' (i.e. GET_READABLE), did a
 * 'get b for writing' (i.e. GET_WRITEABLE) then assigned,
 * At each step in the expression, all of the information
 * necessary to use that expression in any possible way was
 * generated. As a result, the variable structure was
 * quite complex. We had readable variables, writeable
 * variables, dynamic puts waiting to happen, and the works.
 *
 * Now, how does se430 differ? First, for any basic item
 * we remember what it is and don't generate code to load
 * it. For instance, instead of loading b, we just note
 * that the current expression is the variable 'b'.
 * Now, if we do b + 4, when we see the '+', we say,
 * "AHA!" we are doing a read on this, so the variable
 * 'b' is spilled into code 'push b's value onto the stack'.
 * similarly, 'b = 4' gets translated into 'push 4 onto the
 * stack' followed by 'assign the top of the stack to b'.
 * The variables are always a real value, resulting in
 * a lot less genericness and a significant performance
 * boost (tests show between 1.5x minimum, to 2x on
 * average, to 3.5x for the best case stuff.)
 *
 * There are some caveats. For instance, References are
 * still generated in places. The API uses them because it
 * has the 'jseMember' generic routines that return variables
 * to be used in both cases. We get the correct value depending
 * on how a particular API call is using it. Similarly, they
 * are used to allow pass-by-reference parameters to point
 * back to other things. Pass-by-reference parameters are one
 * of the major reasons for complexity I was forced to
 * leave in. The key is that we cannot know until runtime
 * if a parameter is going to be by-reference or not,
 * which forces us to do some work I would love to be
 * able to avoid. Maybe I'll get another obvious-in-hindsight
 * solution for se440.
 *
 * Because of this 'hanging', calls to the expression parser
 * leave the expression unresolved. You have to either
 * 'secompileDiscard()' to get rid of it, or 'secompileGetValue()'
 * to ensure that the stack will have the actual value,
 * or whatever else makes sense. See statemnt.c for how
 * the expression parser is used, and expressn.c for the
 * parser itself.
 *
 * Please note that there is a limit on how much information
 * the 'hanging expression' can contain. We are writing an
 * interpreter, so generating whole code sections that hang
 * around is generally done in compilers in which compile speed
 * is not overly important, for us it is not appropriate. Sure, it
 * would allow for better code in places, but it is a pretty complex
 * thing to do well, and so maybe sometime in the future. This
 * means, for instance, that parsing 'foo[a+6]; will put
 * 'foo' and 'a+6' onto the stack rather than keep the bytecodes
 * to handle them hanging in limbo.
 *
 * See var.h. The generic seVar structure, used by the core, is
 * just a type and data. The API version tacks on the extra
 * information to track a reference, which only it needs.
 *
 *
 * Here is old comments, which is still accurate and thus worth
 * keeping:
 *
 * 6/29/99 - updated comments, Rich Robinson
 *
 * This file and its companions (statemnt.c, expressn.c, operator.c)
 * compiles the tokens into Secode bytecodes. After each function is
 * compiled, it is run through a peephole optimizer.
 *
 * Each secode consists of a single byte opcode and some are followed
 * by a single operand (either a pointer or a int32 value.) In non-
 * memory critical systems, it is just an array of 4-byte values with
 * the operands and opcodes intermixed. For min memory systems (i.e. DOS)
 * the secodes are stored as an array of bytes, and things are packed
 * in to use as little space as possible.
 *
 * To ease in debugging, or if you just want to see what gets generated,
 * there is a routine called secodeListing() which prints out (using
 * DebugPrintf so you'll have to use a debug version) a human-readable
 * version of the secode program. Currently, there is a
 * '#define SECODE_LISTINGS' in 'expressn.c', commented out. Uncomment
 * it to get listings. Note that there are probably more functions than
 * you'd expect - the engine calls jseInterpret() internally at times,
 * so there will be some extra 'global initialization' functions. Also,
 * each function is output twice, first before peephole optimizing, second
 * after it. Please keep the table of secode names in sync with the secode
 * defines in secode.h - it is annoying when I get a listing that looks
 * like junk, only to realize someone added or remove secodes without
 * updating the secode listing table.
 *
 *
 * The compilation uses a recursive-descent, one token look ahead scheme.
 * That means that the 'next' token is the one we are looking at, and
 * we make all parsing decisions by the value of the next token. All parsing
 * routines take a call, and the token chain, and are responsible for
 * advancing the token chain over any tokens it uses (it eats them), and
 * outputting appropriate code via secodeAddItem(). All such routines
 * return False if there was a parsing error and will have printed out
 * some error message. If there is an error, the code produced is considered
 * to be garbage.
 *
 * There are two main sections of the compiler - statements and expressions.
 * The last option for a statement is for it to be an expression, which
 * is why the 'this is not a valid expression' error pops up in a lot of
 * bad programs. At runtime, the code generated for a statement must finish
 * processing with nothing on the stack. The code for an expression finishes
 * processing with the value of the expression on the stack. The code to
 * process For..in and With both keep information on the stack and the
 * secode interpretter knows to get rid of it if there is a return in
 * the middle of one of those loops.
 *
 *
 * Currently, there is a distinction between 'transfer' and 'goto' secodes.
 * I may remove that in the future. Goto secodes (i.e. gotoTrue, gotoFalse,
 * gotoAlways) should only be used when the goto can never cross out of
 * a block. This is the case for things like if()... because the if statement
 * is self-contained, and you'll only jump ahead to the second part of the
 * if statement, never outside a block. The goto statement, as well as
 * labelled breaks and continues can jump outside a block. The try/catch/
 * finally needs to trap the transfers outside a block, which the transfer
 * secodes can do. Of course, that involves extra checking, so use the
 * goto secodes whenever possible.
 */

#if defined(__JSE_GEOS__)

/* Flag that script engine should callQuit as soon as possible due to 
   an out of memory condition. */
extern jsebool jseOutOfMemory;

#endif

/* The top of the stack is an object. Push the given member. If create
 * reference, push a reference to it instead.
 *
 * If either create_reference or to_object, then the member must
 * exist, if both false, can be undefined. Create it as undefined/
 * object based on 'to_object'.
 *
 * If not reference, push the actual value. In either case, overwrite the
 * object on the stack with it
 */
   static void NEAR_CALL
do_op_member(struct Call *call,VarName mem,jsebool create_reference,jsebool to_object)
{
   wSEVar wObjVar = STACK0;
   wSEVar tmp = STACK_PUSH;
   
   
   /* If this is a C-string, it cannot be passed by reference
    * due to the limitation of the way a reference is stored
    * internally. The base must be an object, it cannot be a string.
    * Turning it into a string object is worse, since say we
    * are doing "foo"[1], new String(foo)[1] is an undefined member.
    * Therefore, we just pass the correct value by value rather
    * than an incorrect one by reference.
    */
#if (defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)) || \
    (defined(JSE_TYPE_BUFFER) && (0!=JSE_TYPE_BUFFER))
   if( IsNumericStringTableEntry(mem) && (
#   if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
      (SEVAR_GET_TYPE(wObjVar)==VString && CALL_CBEHAVIOR)
#   endif
#   if defined(JSE_TYPE_BUFFER) && (0!=JSE_TYPE_BUFFER)
    || ( SEVAR_GET_TYPE(wObjVar)==VBuffer)
#   endif
      ) )
   {
      /* replace STACK0 with new value */
      sevarGetValue(call,wObjVar,mem,wObjVar,GV_DEFAULT);
   }
   else
#endif
   {
      SEVAR_INIT_UNDEFINED(tmp);
      
      if( SEVAR_GET_TYPE(wObjVar)!=VObject )
         sevarConvertToObject(call,wObjVar);
      if( CALL_QUIT(call) )
      {
         SEVAR_INIT_BLANK_OBJECT(call,wObjVar);
      }
      assert( SEVAR_GET_TYPE(wObjVar)==VObject );
      
      if( to_object || !create_reference )
      {
         sevarGetValue(call,wObjVar,mem,tmp,GV_DEFAULT);
      }
      
      if( to_object )
      {
         if( SEVAR_GET_TYPE(tmp)==VUndefined )
         {
            /* transform it into an object */
            SEVAR_INIT_BLANK_OBJECT(call,tmp);
            sevarPutValueEx(call,wObjVar,mem,tmp,False);
         }
      }
      
      if( create_reference )
      {
         rSEObjectMem rObjMem;
         rSEObject rObj;
         
         /* Can't allow 'doubly-indirect' things. Note that a member
          * being a reference can only happen in a couple of internal
          * objects that can never be dynamic. Therefore, if dynamic,
          * then the member will never exists and also be a reference.
          */
         SEOBJECT_ASSIGN_LOCK_R(rObj,SEVAR_GET_OBJECT(wObjVar));
         rObjMem = seobjChildMemberStruct(call,rObj,mem);
         SEOBJECT_UNLOCK_R(rObj);
         if ( NULL != SEOBJECTMEM_PTR(rObjMem)
           && SEVAR_GET_TYPE(SEOBJECTMEM_VAR(rObjMem))>=VReference )
         {
            SEVAR_COPY(wObjVar,SEOBJECTMEM_VAR(rObjMem));
         }
         else
         {
            SEVAR_INIT_REFERENCE(wObjVar,SEVAR_GET_OBJECT(wObjVar),mem);
         }
#     if JSE_MEMEXT_MEMBERS!=0
            if ( NULL != SEOBJECTMEM_PTR(rObjMem) )
            {
               SEOBJECTMEM_UNLOCK_R(rObjMem);
            }
#     endif
      }
      else
      {
         SEVAR_COPY(wObjVar,tmp);
      }
   }
   STACK_POP;
}

   static void NEAR_CALL
varGrabStringTableEntry(struct Call *call,wSEVar wVar,VarName *name,uword8 *nametmp)
{
   if( SEVAR_GET_TYPE(wVar)==VNumber )
   {
      slong val = SEVAR_GET_SLONG(wVar);
      *name = (val<0)?NegativeStringTableEntry(val):PositiveStringTableEntry(val);
   }
   else
   {
      const jsecharptr tmp;

      if( SEVAR_GET_TYPE(wVar)!=VString )
         sevarConvertToString(call,wVar);

      tmp = sevarGetData(call,wVar);
      *name = GrabStringTableEntry(call,tmp,(stringLengthType)SEVAR_STRING_LEN(wVar),nametmp);
      SEVAR_FREE_DATA(call,(JSE_MEMEXT_R void *)tmp);
   }
}


   static void NEAR_CALL
do_array_member(struct Call *call,jsebool create_reference,jsebool to_object)
{
   VarName mem;
   uword8 nametmp;

   varGrabStringTableEntry(call,STACK0,&mem,&nametmp);
   STACK_POP;
   do_op_member(call,mem,create_reference,to_object);
   ReleaseStringTableEntry(/*call,*/mem,nametmp);
}

#if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
   static rSEObjectMem NEAR_CALL
findCachedGlobalEx(struct Call *call,VarName mem
#  if JSE_MEMEXT_READONLY!=0
      ,jsebool returnReadOnly /* if false the what this returns is really a rSEObjectMem */
#  endif
)
{
   wSEObjectMem wRet;

   if( call->useCache )
   {
      uint i;
      struct call_cache *callCache;
      rSEObject rGlobalObject;
      wSEMembers wMembers;

      SEOBJECT_ASSIGN_LOCK_R(rGlobalObject,call->hGlobalObject);

#     if JSE_MEMEXT_READONLY!=0
      if ( returnReadOnly )
         SEMEMBERS_ASSIGN_LOCK_R(SEMEMBERS_CAST_R(wMembers),SEOBJECT_PTR(rGlobalObject)->hsemembers);
      else
#     endif
         SEMEMBERS_ASSIGN_LOCK_W(wMembers,SEOBJECT_PTR(rGlobalObject)->hsemembers);

      for( i=0, callCache = call->cache; i<JSE_CACHE_SIZE; i++, callCache++ )
      {
         if( callCache->entry == mem )
         {
            SEOBJECTMEM_ASSIGN_INDEX(wRet,wMembers,callCache->slot);

            if( SEOBJECTMEM_PTR(wRet)->name != mem )
            {
               /* It has moved around or been deleted from the
                * object, update its location
                */
#              if JSE_MEMEXT_READONLY!=0
               if ( returnReadOnly )
                  SEOBJECTMEM_CAST_R(wRet) = rseobjGetMemberStruct(call,rGlobalObject,mem);
               else
#              endif
                  SEOBJECTMEM_CAST_R(wRet) = wseobjGetMemberStruct(call,rGlobalObject,mem);
               if ( NULL == SEOBJECTMEM_PTR(wRet) )
               {
                  /* It has been removed */
                  callCache->entry = NULL;
                  break;
               }
#              if JSE_MEMEXT_MEMBERS!=0
                  assert( SEMEMBERS_PTR(wMembers) == SEMEMBERS_PTR(wRet.semembers) );
#              endif
               callCache->slot = (uint)(SEOBJECTMEM_PTR(wRet) - SEMEMBERS_PTR(wMembers));
#              if JSE_MEMEXT_READONLY!=0
               if ( returnReadOnly )
                  SEMEMBERS_UNLOCK_R(SEMEMBERS_CAST_R(wMembers));
               else
#              endif
                  SEMEMBERS_UNLOCK_W(wMembers);
            }
            SEOBJECT_UNLOCK_R(rGlobalObject);
            assert( SEOBJECTMEM_PTR(wRet)->name == mem );
            /* bump this cache entry when it is accessed, so frequently
             * accessed stuff is at the top of the cache. This means it
             * is found faster and also it is not dropped to make room
             * for new cache entries
             */
            if( 0 < i )
            {
               uint slot = callCache->slot;
               callCache[0] = callCache[-1];
               callCache[-1].entry = mem;
               callCache[-1].slot = slot;
            }
            return SEOBJECTMEM_CAST_R(wRet);
         }
      }
#     if JSE_MEMEXT_READONLY!=0
      if ( returnReadOnly )
         SEMEMBERS_UNLOCK_R(SEMEMBERS_CAST_R(wMembers));
      else
#     endif
         SEMEMBERS_UNLOCK_W(wMembers);
      SEOBJECT_UNLOCK_R(rGlobalObject);
   }
   SEOBJECTMEM_PTR(wRet) = NULL;
   return SEOBJECTMEM_CAST_R(wRet);
}
#  if JSE_MEMEXT_READONLY==0
#     define findCachedGlobal(CALL,NAME,READONLY)  findCachedGlobalEx((CALL),(NAME))
#  else
#     define findCachedGlobal(CALL,NAME,READONLY)  findCachedGlobalEx((CALL),(NAME),(READONLY))
#  endif
#endif

/* returns true if it was found in the with objects, else load to
 * top of the stack.  If ref then load to top of stack, but load an
 * indirect reference so that it can be passed to a CFunction, or
 * assigned to.
 */
   static jsebool NEAR_CALL
callWithCatchEntry(struct Call *call,WITH_TYPE with_depth,sword32 index,jsebool ref)
{
   VarName name;
   wSEVar wLocVar;
   rSEObject rScopeChain;
   rSEMembers rMembers;
   rSEObjectMem rIt;
   jsebool found;

   name = ((struct LocalFunction *)FUNCPTR)->items
        [ (index<=0) ? index : ((struct LocalFunction *)FUNCPTR)->InputParameterCount+index-1].VarName;
   wLocVar = STACK0;
   SEOBJECT_ASSIGN_LOCK_R(rScopeChain,call->hScopeChain);
   SEMEMBERS_ASSIGN_LOCK_R(rMembers,SEOBJECT_PTR(rScopeChain)->hsemembers);
   SEOBJECTMEM_ASSIGN_INDEX(rIt,rMembers,SEOBJECT_PTR(rScopeChain)->used);
   SEOBJECT_UNLOCK_R(rScopeChain);
   found = False;

   /* so don't have to init it each time it is allocated in caller */
   SEVAR_INIT_UNDEFINED(wLocVar);
   assert( with_depth!=0 );
   while( 0 < with_depth-- )
   {
      rSEObject robj;
      SEOBJECTMEM_PTR(rIt)--;
      assert( SEVAR_GET_TYPE(SEOBJECTMEM_VAR(rIt)) == VObject );
      SEOBJECT_ASSIGN_LOCK_R(robj,SEVAR_GET_OBJECT(SEOBJECTMEM_VAR(rIt)));
      found = seobjHasProperty(call,robj,name,wLocVar,ref?HP_REFERENCE:HP_DEFAULT);
      SEOBJECT_UNLOCK_R(robj);
      if ( found )
         break;
   }
   SEOBJECTMEM_UNLOCK_R(rIt);
   return found;
}

#if 0 == JSE_INLINES
   static wSEVar NEAR_CALL
stackLocalOrParam(struct Call *call,VAR_INDEX_TYPE index)
{
   return (index<=0) ? CALL_PARAM(-index) : CALL_LOCAL(index);
}
#else
#  define stackLocalOrParam(c,i)   ((i)<=0) ? CALL_PARAM(-(i)) : CALL_LOCAL((i))
#endif

/* ---------------------------------------------------------------------- */
/* secode interpreter main loop                                           */
/* ---------------------------------------------------------------------- */

#pragma codeseg SECODE2_TEXT

/*
 * Return value is True as long as more code remains to be executed.
 *
 * <old note about errors removed as I changed it to go to a field
 *  in the call for just this reason, too many times it broke.>
 */
   jsebool NEAR_CALL
secodeInterpret(register struct Call *call)
{
   secodeelem t;

   /* shared by the crement operators */
   wSEVar w_c_lhs,w_c_rhs;
   jsebool c_precrement;
   wSEVar wVarTmp;

   /* do not make the other variables 'global' to the function. By
    * making them local to the switch cases, the optimizer is able to
    * see that the lifetimes of the variables is short, and this
    * brings significant performance improvements with typical
    * code generators.
    */



   /* we have reached the initial API call boundary to call our
    * function chain. Nothing more to execute, tell the guy
    * who called us that is the case.
    */
   if( FRAME==NULL ) return False;


   /* we return explicitly when appropriate, else loop forever */
   while( 1 )
   {
      assert( FRAME!=NULL );
#     if JSE_MEMEXT_SECODES==0
      assert( IPTR < ((struct LocalFunction *)FUNCPTR)->opcodes +
                     ((struct LocalFunction *)FUNCPTR)->opcodesUsed );
#     endif
      assert( STACK0>=FRAME );

#     if defined(__JSE_GEOS__)
      if (jseOutOfMemory)
      {
	 jseOutOfMemory = FALSE;
	 callQuit(call,textcoreOUT_OF_MEMORY);
      }
#     endif

      /* Handle possibly catching of errors or 'returns' */
      if( call->state!=FlowNoReasonToQuit )
      {
         /* keep popping off stuff until we either trap it or we reach
          * the top level. Note that one reason why call->continue_count==1
          * is to make sure it returns after every function call, so
          * do that as well.
          */
         while( FRAME!=NULL )
         {
            if( call->tries!=NULL && STACK_FROM_STACKPTR(call->tries->fptr)==FRAME )
            {
               /* aha, an appropriate trap */
               if( call->state==FlowError && call->tries->catch!=(ADDR_TYPE)-1 )
               {
                  /* error object on top of stack */
                  rSEVar r_error_var = &(call->error_var);
                  wSEVar w_new_loc;

                  call->iptr = IPTR_FROM_INDEX(call,call->tries->catch);
                  call->stackptr = call->tries->sptr;

                  call->tries->catch = (ADDR_TYPE)-1;
                  call->state = FlowNoReasonToQuit;
                  w_new_loc = STACK_PUSH;
                  /* copy the return error to top of stack, so that
                   * the catch start instruction can add it to the
                   * scope chain.
                   */
                  SEVAR_COPY(w_new_loc,r_error_var);
                  break;
               }
               /* no trap, but we do have some 'finally' code to execute */
               else if( call->tries->fin!=(ADDR_TYPE)-1 )
               {
               handle_finally:
                  /* We've tried to branch but have a finally */
                  if( call->tries->loc!=(ADDR_TYPE)-1 )
                  {
                     /* we are inside finally, just discard this structure and do it */
                     struct TryBlock *f = call->tries;
                     ADDR_TYPE loc = call->tries->loc;

                     if( call->tries->endtryreached )
                     {
                        call->state = call->tries->state;
                        call->iptr  = IPTR_FROM_INDEX(call,loc);

                        /* If we are in a normal execution, like trying
                         * to so a 'return', we don't mess the stack
                         * up. Only if we are exiting due to an erro
                         * do we.
                         */
                        if( CALL_QUIT(call) )
                           call->stackptr = call->tries->sptr;
                     }

                     /* remove the catch variable scope item added */
                     if( call->tries->incatch ) CALL_REMOVE_SCOPE_OBJECT(call);
                     call->tries = call->tries->prev;
                     jseMustFree(f);

                     /* It is possible to have a transfer trying to get
                      * out of a nested finally.
                      */
                     /* assert( 0 <= loc );*/ /* for following (uint) cast */
                     if( call->tries!=NULL && FRAME==STACK_FROM_STACKPTR(call->tries->fptr) &&
                         ((uint)loc<call->tries->begin || (uint)loc>=call->tries->end) )
                        goto handle_finally;
                  }
                  else
                  {
                     /* store the state in preparation to execute
                      * finally code.
                      */
                     call->tries->state = call->state;
                     call->tries->loc = (uint)INDEX_FROM_IPTR(call,IPTR);
                     IPTR = IPTR_FROM_INDEX(call,call->tries->fin);
                     call->state = 0;
                     /* We don't muck with the stack. It might have
                      * a return variable all set up on it. We want to
                      * get back to the same state, so try cannot net
                      * alter the stack. No problem, if we ourselves
                      * do a return, the undoing do to 'unframepointing'
                      * will kill any extra stuff hanging around. If we
                      * don't, then when we get back, the stack will
                      * be as it was.
                      *
                      * call->tries->sptr is only used if we need to
                      * pop back up stack frames due to a error
                      * that will be caught
                      */
                  }
                  break;
               }
            }
            else
            {
               callReturnFromFunction(call);
               if( call->continue_count==1 )
                  return FRAME!=NULL;
            }
         }
         return FRAME!=NULL;
      }

      /* make each set of variables local to the case (except for ones
       * shared by the crement goto.) This helps optimizers.
       */

      /* Ok, we have something to execute, let's do so */
      switch( t = *(IPTR++) )
      {
         case sePushLocalWith:
         case sePushLocalAsObject:
         case sePushLocalParam:
         {
            wSEVar w_lhs = STACK_PUSH;
            VAR_INDEX_TYPE index = IPTR_GET(VAR_INDEX_TYPE);
            WITH_TYPE tmp = IPTR_GET(WITH_TYPE);

            /* In many of the cases, 'tmp' is the 'with-depth' read
             * from the bytecode which is determined at compile time.
             * If the with-depth is 0, we don't need to search
             * the entries on the scope chain for it (there will be
             * none.) Thus, we only do a with search for items if
             * with-depth is not 0.
             */
            if( tmp==0 || !callWithCatchEntry(call,tmp,index,t==sePushLocalParam) )
            {
               wSEVar w_rhs = stackLocalOrParam(call,index);

               if ( t == sePushLocalParam )
               {
                  /* Unfortunately, because we could be passing by
                   * reference, we need to make sure all locals
                   * are pointing into their appropriate object
                   */
                  SEVAR_INIT_UNDEFINED(w_lhs);
                  if( call->hVariableObject==hSEObjectNull )
                     callCreateVariableObject(call,NULL);

                  SEVAR_COPY(w_lhs,w_rhs);

                  /* Locals must always be moved into the VariableObject and
                   * refered to by a reference index. Params can be the same,
                   * but they can also be pass-by-reference params which can
                   * be just a regular VReference.
                   */
                  assert( SEVAR_GET_TYPE(w_lhs)==VReferenceIndex || \
                          ((index<=0) && SEVAR_GET_TYPE(w_lhs)==VReference) );
               }
               else
               {
                  SEVAR_COPY(w_lhs,w_rhs);
                  SEVAR_DEREFERENCE(call,w_lhs);
                  if ( t==sePushLocalAsObject )
                  {
                     if( SEVAR_GET_TYPE(w_lhs)==VUndefined )
                     {
                        SEVAR_INIT_BLANK_OBJECT(call,w_lhs);

                        /* Put it back so permanently changes */
                        SEVAR_DO_PUT(call,w_rhs,w_lhs);
                     }
                  }
               }

            }

            /* Should no longer be any 'special' thing, the secode
             * stack only deals with real data items.
             */
            assert( t==sePushLocalParam  || SEVAR_GET_TYPE(w_lhs)<VStorage );
            break;
         }

         case sePostDecLocal:
         case sePostIncLocal:
         case sePreDecLocal:
         case sePreIncLocal:
         {
            VAR_INDEX_TYPE index = IPTR_GET(VAR_INDEX_TYPE);
            WITH_TYPE tmp =     IPTR_GET(WITH_TYPE);
            w_c_lhs = STACK_PUSH;
            w_c_rhs = STACK_PUSH;
            SEVAR_INIT_UNDEFINED(w_c_lhs);
            if( tmp==0 || !callWithCatchEntry(call,tmp,index,True) )
            {
               SEVAR_INIT_UNDEFINED(w_c_rhs);
               w_c_rhs = stackLocalOrParam(call,index);
            }
            c_precrement = ( t<=sePreDecLocal );
            assert( c_precrement || (t==sePostIncLocal || t==sePostDecLocal) );

            goto do_crement;
         }
         case seDecOnlyLocal:
         case seIncOnlyLocal:
         {
            VAR_INDEX_TYPE index = IPTR_GET(VAR_INDEX_TYPE);
            WITH_TYPE tmp =     IPTR_GET(WITH_TYPE);
            w_c_lhs = NULL;
            w_c_rhs = STACK_PUSH;
            if( tmp==0 || !callWithCatchEntry(call,tmp,index,True) )
            {
               SEVAR_INIT_UNDEFINED(w_c_rhs);
               w_c_rhs = stackLocalOrParam(call,index);
            }
            c_precrement = False;
         }

      do_crement:
         {
            /* when getting here, the following should be set up:
             *
             * c_lhs = place to put the result or NULL if no result
             * c_rhs = the item to crement, can be a Reference
             * c_increment = true if this is an increment, else decrement
             * c_precrement = true if this is a precrement
             *
             * The stack should have 1 item to be popped.
             */
            jsebool decrement = (jsebool)(1&t); /* increment codes are even, decrement are odd */
#           ifndef NDEBUG
               if ( decrement )
               {
                  assert( t==sePostDecLocal  || t==sePreDecLocal \
                       || t==seDecOnlyLocal \
                       || t==sePostDecMember || t==sePreDecMember \
                       || t==sePostDecGlobal || t==sePreDecGlobal \
                       || t==sePostDecArray  || t==sePreDecArray );
               }
               else
               {
                  assert( t==sePostIncLocal  || t==sePreIncLocal \
                       || t==seIncOnlyLocal \
                       || t==sePostIncMember || t==sePreIncMember \
                       || t==sePostIncGlobal || t==sePreIncGlobal \
                       || t==sePostIncArray  || t==sePreIncArray );
               }
#           endif

            if( SEVAR_GET_TYPE(w_c_rhs)==VNumber
#            if defined(JSE_FP_EMULATOR) && (0!=JSE_FP_EMULATOR)
             && jseIsFinite(w_c_rhs->data.num_val)
#            endif
              )
            {
               if( c_precrement )
               {
                  if ( decrement )
                     JSE_FP_DECREMENT(w_c_rhs->data.num_val);
                  else
                     JSE_FP_INCREMENT(w_c_rhs->data.num_val);
                  if( w_c_lhs ) SEVAR_COPY(w_c_lhs,w_c_rhs);
               }
               else
               {
                  if( w_c_lhs ) SEVAR_COPY(w_c_lhs,w_c_rhs);
                  if ( decrement )
                     JSE_FP_DECREMENT(w_c_rhs->data.num_val);
                  else
                     JSE_FP_INCREMENT(w_c_rhs->data.num_val);
               }
            }
            else
            {
               /* Not the easy case */
               wSEObjectMem wRHSObjectMem;
               wSEVar wRetTmp;

               SEOBJECTMEM_PTR(wRHSObjectMem) = NULL;
               wVarTmp = NULL;
               if( SEVAR_GET_TYPE(w_c_rhs)==VReferenceIndex )
               {
                  rSEObject robj;
                  SEOBJECT_ASSIGN_LOCK_R(robj,w_c_rhs->data.ref_val.hBase);
                  SEOBJECTMEM_CAST_R(wRHSObjectMem) = wseobjIndexMemberStruct(call,robj,
                     (MemCountUInt)(JSE_POINTER_UINT)w_c_rhs->data.ref_val.reference);
                  SEOBJECT_UNLOCK_R(robj);
                  assert( NULL != SEOBJECTMEM_PTR(wRHSObjectMem) );
                  w_c_rhs = SEOBJECTMEM_VAR(wRHSObjectMem);
               }
               else if( SEVAR_GET_TYPE(w_c_rhs)==VReference )
               {
                  hSEObject hobj;
                  rSEObject robj;

                  hobj = w_c_rhs->data.ref_val.hBase;
                  SEOBJECT_ASSIGN_LOCK_R(robj,hobj);

                  if( SEOBJ_IS_DYNAMIC(robj) )
                  {
                     VarName mem = w_c_rhs->data.ref_val.reference;

                     /* we must make sure to put the value back when done because could
                      * be dynamic put
                      */
                     wVarTmp = STACK_PUSH;
                     SEVAR_COPY(wVarTmp,w_c_rhs);


                     /* can't use SEVAR_DEREFERENCE because this is a dynamic object
                      */
                     SEOBJECT_UNLOCK_R(robj);
                     SEVAR_INIT_OBJECT(w_c_rhs,hobj);
                     if( !sevarGetValue(call,w_c_rhs,mem,w_c_rhs,GV_DEFAULT) ) break;
                  }
                  else
                  {
                     jsebool found;
                     wSEObject wobj;

                     SEOBJECT_ASSIGN_LOCK_W(wobj,hobj);
                     SEOBJECT_UNLOCK_R(robj);
                     wRHSObjectMem = seobjNewMember(call,wobj,
                        w_c_rhs->data.ref_val.reference,&found);
                     SEOBJECT_UNLOCK_W(wobj);
                     assert( NULL != SEOBJECTMEM_PTR(wRHSObjectMem) );
                     w_c_rhs = SEOBJECTMEM_VAR(wRHSObjectMem);
                     if( !found )
                     {
                        SEVAR_INIT_NUMBER(w_c_rhs,jseZero);
                     }
                  }
               }

               /* The operator return must not overwrite the existing
                * value, that is the 'default' behavior, instead we let
                * the operator function do its version. The result is
                * simply returned as the result of the expression.
                */
               wRetTmp = STACK_PUSH;
               SEVAR_COPY(wRetTmp,w_c_rhs);
               IF_OPERATOR_NOT_OVERLOADED(call,wRetTmp,NULL,
                                          (secodeelem)(decrement?seDecOnlyLocal:seIncOnlyLocal))
               {
#                 if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
                  if( SEVAR_ARRAY_PTR(w_c_rhs) && CALL_CBEHAVIOR )
                  {
                     /* This is something like 'var a = "foo"; a++;'
                      * We just increment the index.
                      */
                     if ( !c_precrement && w_c_lhs )
                        SEVAR_COPY(w_c_lhs,w_c_rhs);
                     w_c_rhs->data.string_val.loffset += (JSE_POINTER_SINDEX)(decrement?-1:1);
                     if ( c_precrement && w_c_lhs )
                        SEVAR_COPY(w_c_lhs,w_c_rhs);
                  }
                  else
#                 endif
                  {
                     if( SEVAR_GET_TYPE(w_c_rhs)!=VNumber )
                     {
                        /* We need to copy it because convertToNumber
                         * will convert objects in place, but we only
                         * want to change the object if the conversion
                         * was successful.
                         */
                        wSEVar wTmp = STACK_PUSH;
                        jsenumber val;

                        SEVAR_COPY(wTmp,w_c_rhs);
                        val = sevarConvertToNumber(call,wTmp);
                        if( CALL_QUIT(call) )
                        {
#                          if 0!=JSE_MEMEXT_MEMBERS
                              if ( NULL != SEOBJECTMEM_PTR(wRHSObjectMem) )
                                 SEOBJECTMEM_UNLOCK_W(wRHSObjectMem);
#                          endif
                           break;
                        }
                        STACK_POP;
                        SEVAR_INIT_NUMBER(w_c_rhs,val);
                     }
                     if ( !c_precrement && w_c_lhs )
                        SEVAR_COPY(w_c_lhs,w_c_rhs);
                     if ( jseIsFinite(w_c_rhs->data.num_val) )
                     {
                        if ( decrement )
                           JSE_FP_DECREMENT(w_c_rhs->data.num_val);
                        else
                           JSE_FP_INCREMENT(w_c_rhs->data.num_val);
                     }
                     if ( c_precrement && w_c_lhs )
                        SEVAR_COPY(w_c_lhs,w_c_rhs);
                  }
                  if( wVarTmp )
                  {
                     wSEVar wTmpObj = STACK_PUSH;

                     /* do dynamic put back */

                     SEVAR_INIT_OBJECT(wTmpObj,wVarTmp->data.ref_val.hBase);
                     if( !SEVAR_PUT_VALUE(call,wTmpObj,wVarTmp->data.ref_val.reference,w_c_rhs) )
                     {
#                       if 0!=JSE_MEMEXT_MEMBERS
                           if ( NULL != SEOBJECTMEM_PTR(wRHSObjectMem) )
                              SEOBJECTMEM_UNLOCK_W(wRHSObjectMem);
#                       endif
                        break;
                     }
                     STACK_POPX(2);
                  }
               }
#              if defined(JSE_OPERATOR_OVERLOADING) && (0!=JSE_OPERATOR_OVERLOADING)
               else
               {
                  /* If operator is overloaded, we still need to make
                   * sure the stack gets a sensible value, but we don't
                   * do the Put - the operator overloading takes
                   * precedence
                   */
                  if( w_c_lhs ) SEVAR_COPY(w_c_lhs,wRetTmp);

                  if( wVarTmp )
                  {
                     STACK_POP;
                  }
               }
#              endif
               STACK_POP;
#              if 0!=JSE_MEMEXT_MEMBERS
                  if ( NULL != SEOBJECTMEM_PTR(wRHSObjectMem) )
                     SEOBJECTMEM_UNLOCK_W(wRHSObjectMem);
#              endif
            }
            STACK_POP;
            break;
         }

         case seAssignLocalWith:
         {
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs = STACK_PUSH;
            VAR_INDEX_TYPE index;
            WITH_TYPE tmp;

            SEVAR_INIT_UNDEFINED(w_lhs);
            /* We have to duplicate a string when copying now because at
             * the point the string is being used, we no longer know
             * where the string came from to make a copy only then.
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
            if ( SEVAR_GET_TYPE(w_rhs)==VString
                 && CALL_CBEHAVIOR
                 && SESTR_IS_CONSTANT(SEVAR_GET_STRING(w_rhs).data) )
            {
               sevarDuplicateString(call,w_rhs);
            }
#           endif
            index = IPTR_GET(VAR_INDEX_TYPE);
            tmp =   IPTR_GET(WITH_TYPE);
            if( tmp==0 || !callWithCatchEntry(call,tmp,index,True) )
            {
               SEVAR_INIT_UNDEFINED(w_lhs);
               w_lhs = stackLocalOrParam(call,index);
            }
            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,w_rhs,seAssignLocal)
            {
               SEVAR_DO_PUT(call,w_lhs,w_rhs);
            }
            STACK_POP;
            break;
         }

         case seContinueFunc:
         {
            (void)IPTR_GET(CONST_TYPE);        /* skip over line number datum */
            if( call->continue_count==0 ) break;
            if( call->continue_count>1 )
            {
               if( --call->continue_count!=2 )
                  break;
               else
                  call->continue_count = 2+JSE_INFREQUENT_COUNT;
            }
            return True;
         }

         /* These are both only used when looking up source locations,
          * not during actual execution.
          */
         case seLineNumber:
            (void)IPTR_GET(CONST_TYPE);        /* skip over line number datum */
            break;

         case sePushLocal:
         {
            wSEVar w_lhs = STACK_PUSH;
            VAR_INDEX_TYPE index = IPTR_GET(VAR_INDEX_TYPE);
            rSEVar r_rhs = stackLocalOrParam(call,index);
            SEVAR_COPY(w_lhs,r_rhs);
            SEVAR_DEREFERENCE(call,w_lhs);
            assert( SEVAR_GET_TYPE(w_lhs)<VStorage );
            break;
         }

         case seAssignLocal:
         case seAssignLocalPop:
         {
            wSEVar w_rhs = STACK0;
            VAR_INDEX_TYPE index;
            wSEVar w_lhs;

            /* We have to duplicate a string when copying now because at
             * the point the string is being used, we no longer know
             * where the string came from to make a copy only then.
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
               if ( SEVAR_GET_TYPE(w_rhs)==VString
                 && CALL_CBEHAVIOR
                 && SESTR_IS_CONSTANT(SEVAR_GET_STRING(w_rhs).data) )
               {
                  sevarDuplicateString(call,w_rhs);
               }
#           endif
            /* No WithCatch entry because that is the point of this
             * second secode, it doesn't need it
             */
            index = IPTR_GET(VAR_INDEX_TYPE);
            w_lhs = stackLocalOrParam(call,index);
            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,w_rhs,seAssignLocal)
            {
               SEVAR_DO_PUT(call,w_lhs,w_rhs);
            }
            if( t!=seAssignLocal ) STACK_POP;
            break;
         }

         case sePushConstant:
         {
            rSEMembers rMembers;
            wSEVar w_lhs = STACK_PUSH;
            CONST_TYPE index = IPTR_GET(CONST_TYPE);

            SEMEMBERS_ASSIGN_LOCK_R(rMembers,call->hConstants);
            SEVAR_COPY(w_lhs,&(SEMEMBERS_PTR(rMembers)[index].value));
            SEMEMBERS_UNLOCK_R(rMembers);

            /* if function literal, add the current scope chain
             * as well. This must include 'with' and stuff,
             * so we need to construct a new one right here
             * and now.
             *
             * We need to also add an item to that chain, namely
             * a single variable equal to the name of the function,
             * so function literals can refer to themself by name.
             */
            if( SEVAR_GET_TYPE(w_lhs)==VObject )
            {
               MemCountUInt index;
               wSEObjectMem wMem;
               hSEObject hobj;
               wSEObject wobj, wNameObj;
               const jsecharptr name;
	       jsecharptr fname;
               rSEObject rScopeChain;
               rSEMembers rMembers;

               assert( sevarGetFunction(call,w_lhs)!=NULL );

               if( call->hVariableObject==hSEObjectNull )
                  callCreateVariableObject(call,NULL);
               assert( call->hVariableObject!=hSEObjectNull );

               hobj = w_lhs->data.object_val.hSavedScopeChain = seobjNew(call,False);

               /* Build the saved scope chain up from the end of the scope
                * chain stack, so it will be in the regular order (i.e. the
                * first member should be the first item searched, etc.
                */

               /* First add the object to allow the function to refer to itself */
               SEOBJECT_ASSIGN_LOCK_W(wobj,hobj);
               wMem = SEOBJ_CREATE_MEMBER(call,wobj,NULL);
               SEVAR_INIT_BLANK_OBJECT(call,SEOBJECTMEM_VAR(wMem));
               fname = functionName(sevarGetFunction(call,w_lhs),call);
	       name = LFOM(fname);
               SEOBJECT_ASSIGN_LOCK_W(wNameObj,SEVAR_GET_OBJECT(SEOBJECTMEM_VAR(wMem)));
               SEOBJECTMEM_UNLOCK_W(wMem);
               wMem = SEOBJ_CREATE_MEMBER(call,wNameObj,
                         LockedStringTableEntry(call,name,(stringLengthType)strlen_jsechar(name)));
	       UFOM(fname);
               SEOBJECT_UNLOCK_W(wNameObj);
               SEVAR_COPY(SEOBJECTMEM_VAR(wMem),w_lhs);
               SEOBJECTMEM_PTR(wMem)->attributes = jseReadOnly|jseDontDelete;
               SEOBJECTMEM_UNLOCK_W(wMem);

               /* then copy the existing scope chain members. */
               SEOBJECT_ASSIGN_LOCK_R(rScopeChain,call->hScopeChain);
               index = SEOBJECT_PTR(rScopeChain)->used;
               SEMEMBERS_ASSIGN_LOCK_R(rMembers,SEOBJECT_PTR(rScopeChain)->hsemembers);
               while( SEVAR_GET_TYPE(&(SEMEMBERS_PTR(rMembers)[--index].value))!=VNull )
               {
                  assert( SEVAR_GET_TYPE(&(SEMEMBERS_PTR(rMembers)[index].value))==VObject );
                  wMem = SEOBJ_CREATE_MEMBER(call,wobj,NULL);
                  SEVAR_COPY(SEOBJECTMEM_VAR(wMem),&(SEMEMBERS_PTR(rMembers)[index].value));
                  SEOBJECTMEM_UNLOCK_W(wMem);
               }
               SEMEMBERS_UNLOCK_R(rMembers);
               SEOBJECT_UNLOCK_R(rScopeChain);
               SEOBJECT_UNLOCK_W(wobj);
            }
            assert( SEVAR_GET_TYPE(w_lhs)<VStorage );
            break;
         }


         /* try/catch/finally handling opcodes */

         case seDereferParam:
         {
            const struct Function *function;
            uword8 varAttrib = 0;
            /* This is not a variable index, it is simply a numeric
             * count of the parameter to the function, but it uses the
             * same instruction 'format' as a constant index
             */
            CONST_TYPE index = IPTR_GET(CONST_TYPE);
            wSEVar r_lhs = STACK0;

            /* look back past the params to get the function variable */
            rSEVar r_rhs = STACKX(1+index);

            function = sevarGetFunction(call,r_rhs);

            assert( function!=NULL );

            if( FUNCTION_IS_LOCAL(function) &&
                index < ((struct LocalFunction*)function)->InputParameterCount)
            {
               varAttrib = ((struct LocalFunction*)function)->items[index].VarAttrib;
               assert(varAttrib == 0 || varAttrib == 1);
            }

            if( varAttrib==0 && !FUNCTION_C_BEHAVIOR(function) )
            {
               /* Pass by value parameter, undo the reference */
               SEVAR_DEREFERENCE(call,r_lhs);
            }
            break;
         }

         case seNewFunction:
         case seCallFunction:
            callFunction(call,(uword16)IPTR_GET(CONST_TYPE),t==seNewFunction);
            break;

         case seStartTry:
         {
            struct TryBlock *newtry;

            newtry = jseMustMalloc(struct TryBlock,sizeof(struct TryBlock));

            newtry->prev = call->tries;
            call->tries = newtry;

            newtry->fptr = call->frameptr;
            newtry->sptr = call->stackptr;
            newtry->begin = (uint)INDEX_FROM_IPTR(call,IPTR-1);
            newtry->end = IPTR_GET(ADDR_TYPE);
            newtry->fin = (ADDR_TYPE)-1;
            newtry->catch = (ADDR_TYPE)-1;
            newtry->incatch = False;
            newtry->loc = (ADDR_TYPE)-1;
            newtry->endtryreached = False;
            break;
         }
         case seCatchTry:
         {
            secode iptr_tmp = IPTR;
            ADDR_TYPE tmp = IPTR_GET(ADDR_TYPE);

            if( IPTR_FROM_INDEX(call,tmp)==(iptr_tmp-1) ) tmp = (ADDR_TYPE)-1;
            assert( call->tries!=NULL );
            call->tries->catch = tmp;
            break;
         }

         case seFinallyTry:
            assert( call->tries!=NULL );
            assert( call->tries->fin==(ADDR_TYPE)-1 );
            call->tries->fin = IPTR_GET(ADDR_TYPE);
            break;

         case seGoto:
         {
            ADDR_TYPE tmp = SECODE_GET_ONLY(IPTR,ADDR_TYPE);
;           call->iptr = IPTR_FROM_INDEX(call,tmp);
            break;
         }

         case seTransfer:
         {
            ADDR_TYPE tmp = IPTR_GET(ADDR_TYPE);

            /* set new instruction first, so when the finally is
             * done we end up in the place we are going to.
             */
            call->iptr = IPTR_FROM_INDEX(call,tmp);

            if( call->tries!=NULL && FRAME==STACK_FROM_STACKPTR(call->tries->fptr) &&
                (tmp<call->tries->begin || tmp>=call->tries->end) )
               goto handle_finally;

            break;
         }

         /* 2 items on stack, -2=base object, -1=current object/mem as ref index
          *
          * On exit, it pushes the next member's property name,
          * else goes to the given address (with the two items on the
          * stack.) It cannot remove the two items, as the exit address
          * is the same as the break address, and both must cleanup.
          */
         case seGotoForIn:
         {
            wSEVar w_rhs = STACK0;         /* index */
            rSEVar r_lhs = w_rhs-1;        /* base object */
            ADDR_TYPE tmp = IPTR_GET(ADDR_TYPE);
            rSEObject rBaseObj;
            rSEObjectMem rLHSMem;
            rSEObject rObj;
            rSEMembers rMembers;
            jsebool bad = True;

            SEOBJECTMEM_PTR(rLHSMem) = NULL;
            SEOBJECT_HANDLE(rBaseObj) = hSEObjectNull;

            while( bad )
            {
               assert( SEVAR_GET_TYPE(r_lhs)==VObject );
               if( SEVAR_GET_TYPE(w_rhs)==VNull )
               {
                  /* First item */
                  SEVAR_INIT_REFERENCE_INDEX(w_rhs,r_lhs->data.object_val.hobj,0);
               }
               else
               {
                  assert( SEVAR_GET_TYPE(w_rhs)==VReferenceIndex );
                  w_rhs->data.ref_val.reference = (VarName)((JSE_POINTER_UINT)w_rhs->data.ref_val.reference+1);
               }

               /* see if we should skip this one */
               assert( hSEObjectNull != w_rhs->data.ref_val.hBase );
               if ( SEOBJECT_HANDLE(rBaseObj) != w_rhs->data.ref_val.hBase )
               {
#                 if 0!=JSE_MEMEXT_OBJECTS
                     if ( SEOBJECT_HANDLE(rBaseObj) != hSEObjectNull )
                        SEOBJECT_UNLOCK_R(rBaseObj);
#                 endif
                  SEOBJECT_ASSIGN_LOCK_R(rBaseObj,w_rhs->data.ref_val.hBase);
               }
               if( SEOBJECT_PTR(rBaseObj)->used <= (MemCountUInt)(JSE_POINTER_UINT)w_rhs->data.ref_val.reference )
               {
                  /* end of object, move into its prototype */
                  rSEObjectMem rMem = rseobjGetMemberStruct(call,rBaseObj,
							    STOCK_STRING(_prototype));
                  if( SEOBJECTMEM_PTR(rMem)==NULL
                   || SEVAR_GET_TYPE(SEOBJECTMEM_VAR(rMem))!=VObject )
                  {
                     /* nothing more to iterate */
#                    if 0!=JSE_MEMEXT_MEMBERS
                        if ( NULL != SEOBJECTMEM_PTR(rMem) )
                           SEOBJECTMEM_UNLOCK_R(rMem);
#                    endif
                     call->iptr = IPTR_FROM_INDEX(call,tmp);
                     goto out;
                  }

                  w_rhs->data.ref_val.hBase = SEVAR_GET_OBJECT(SEOBJECTMEM_VAR(rMem));
                  SEOBJECTMEM_UNLOCK_R(rMem);
                  /* so it will immediately wrap back to 0 */
                  w_rhs->data.ref_val.reference = (VarName)-1;
                  continue;
               }

               /* we are looking at a valid entry, first we skip if DontEnum */
               {
                  rSEMembers rMembers;
                  seAttribs attribs;

                  SEMEMBERS_ASSIGN_LOCK_R(rMembers,SEOBJECT_PTR(rBaseObj)->hsemembers);
                  attribs = SEMEMBERS_PTR(rMembers)
                      [(MemCountUInt)(JSE_POINTER_UINT)w_rhs->data.ref_val.reference].attributes;
                  SEMEMBERS_UNLOCK_R(rMembers);
                  if ( (attribs & jseDontEnum) != 0 )
                     continue;
               }

               bad = False;
               /* we check to see if we have already done this name in a parent */
               SEOBJECT_ASSIGN_LOCK_R(rObj,SEVAR_GET_OBJECT(r_lhs));
               SEMEMBERS_ASSIGN_LOCK_R(rMembers,SEOBJECT_PTR(rBaseObj)->hsemembers);
               while( SEVAR_GET_OBJECT(r_lhs)!=SEOBJECT_HANDLE(rBaseObj) )
               {
                  rSEObjectMem rMem;

                  rMem = rseobjGetMemberStruct(call,rObj,
		     SEMEMBERS_PTR(rMembers)[(MemCountUInt)(JSE_POINTER_UINT)w_rhs->data.ref_val.reference].name);
                  /* AHA! We have */
                  if( NULL != SEOBJECTMEM_PTR(rMem) )
                  {
                     SEOBJECTMEM_UNLOCK_R(rMem);
                     bad = True;
                     break;
                  }
#                 if 0!=JSE_MEMEXT_MEMBERS
                     if ( 0 != SEOBJECTMEM_PTR(rLHSMem) )
                        SEOBJECTMEM_UNLOCK_R(rLHSMem);
#                 endif
                  rLHSMem = rseobjGetMemberStruct(call,rObj,STOCK_STRING(_prototype));

                  /* this could happen if the user deletes a prototype in the
                   * middle. In this case, he has effectively removed all the later
                   * objects from the original enumerated object, so we should
                   * no longer enumerate them.
                   */
                  if( NULL == SEOBJECTMEM_PTR(rLHSMem) )
                  {
                     SEMEMBERS_UNLOCK_R(rMembers);
                     SEOBJECT_UNLOCK_R(rObj);
                     call->iptr = IPTR_FROM_INDEX(call,tmp);
                     goto out;
                  }
                  r_lhs = SEOBJECTMEM_VAR(rLHSMem);
               }
               if( !bad )
               {
                  /* AHA! This one we ought to enumerate */
                  const jsecharptr vNameStr;
                  stringLengthType vNameStrLen;
                  VarName mem;
                  wSEVar w_lhs;

                  w_lhs = STACK_PUSH;
                  mem = SEMEMBERS_PTR(rMembers)[(MemCountUInt)(JSE_POINTER_UINT)w_rhs->data.ref_val.reference].name;
                  vNameStr = GetStringTableEntry(call,mem,&vNameStrLen);
                  SEVAR_INIT_STRING_STRLEN(call,w_lhs,(jsecharptr)vNameStr,vNameStrLen);
               }
               SEMEMBERS_UNLOCK_R(rMembers);
               SEOBJECT_UNLOCK_R(rObj);
            }
      out:
#           if 0!=JSE_MEMEXT_MEMBERS
               if ( 0 != SEOBJECTMEM_PTR(rLHSMem) )
                  SEOBJECTMEM_UNLOCK_R(rLHSMem);
#           endif

            assert( NULL != SEOBJECT_PTR(rBaseObj) );
            SEOBJECT_UNLOCK_R(rBaseObj);
            break;
         }

         /* Do not merge these (gotoFalse/gotoTrue), it actually makes
          * a pretty big difference in tight loops, and these routines
          * are very small.
          */
         case seGotoFalse:
         {
            wSEVar r_lhs = STACK0;
            ADDR_TYPE tmp = IPTR_GET(ADDR_TYPE);
            if( !SEVAR_CONVERT_TO_BOOLEAN(call,r_lhs) )
               call->iptr = IPTR_FROM_INDEX(call,tmp);
            STACK_POP;
            break;
         }

         case seGotoTrue:
         {
            wSEVar r_lhs = STACK0;
            ADDR_TYPE tmp = IPTR_GET(ADDR_TYPE);
            if( SEVAR_CONVERT_TO_BOOLEAN(call,r_lhs) )
               call->iptr = IPTR_FROM_INDEX(call,tmp);
            STACK_POP;
            break;
         }

         case seFilename:
            (void)IPTR_GET(VarName);
            break;

         case sePushMember:
         case sePushMemberParam:
         case sePushMemberAsObject:
            do_op_member(call,
                         IPTR_GET(VarName),
                         (t==sePushMemberParam),
                         (t==sePushMemberAsObject));
#           ifndef NDEBUG
            {
               rSEVar r_lhs = STACK0;
               assert( t==sePushMemberParam || SEVAR_GET_TYPE(r_lhs)<VStorage );
            }
#           endif
            break;

         case seDeleteMember:
         {
            wSEVar w_lhs = STACK0;
            VarName mem = IPTR_GET(VarName);
            /* ECMA spec doesn't say to convert it to an object */
            if( SEVAR_GET_TYPE(w_lhs)!=VObject )
            {
               callQuit(call,textcoreBAD_DELETE_VAR);
               break;
            }
            SEVAR_INIT_BOOLEAN(w_lhs,seobjFullDeleteMember(call,SEVAR_GET_OBJECT(w_lhs),mem));
            break;
         }

         case seAssignMember:
         {
            VarName mem = IPTR_GET(VarName);
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs = w_rhs-1;
            /* We have to duplicate a string when copying now because at
             * the point the string is being used, we no longer know
             * where the string came from to make a copy only then.
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
               if ( SEVAR_GET_TYPE(w_rhs)==VString
                 && CALL_CBEHAVIOR
                 && SESTR_IS_CONSTANT(SEVAR_GET_STRING(w_rhs).data) )
               {
                  sevarDuplicateString(call,w_rhs);
               }
#           endif
            if( !SEVAR_PUT_VALUE(call,w_lhs,mem,w_rhs) ) break;
            /* leave the assigned-from value on the stack, copying over
             * object underneath it.
             */
            SEVAR_COPY(w_lhs,w_rhs);
            /* drop second item on stack */
            STACK_POP;
            break;
         }

         case sePostDecMember:
         case sePostIncMember:
         case sePreDecMember:
         case sePreIncMember:
            do_op_member(call,IPTR_GET(VarName),True,False);
            w_c_lhs = STACK0;
            w_c_rhs = STACK_PUSH; /* crement expects 1 item on stack to be deleted */
            c_precrement = ( t<=sePreDecMember );
            assert( c_precrement || (t==sePostIncMember || t==sePostDecMember) );
            SEVAR_COPY(w_c_rhs,w_c_lhs);
            SEVAR_INIT_UNDEFINED(w_c_lhs);
            goto do_crement;

         case seCheckGlobal:
         {
            VarName mem = IPTR_GET(VarName);
#           if defined(JSE_GROWABLE_STACK) && (0!=JSE_GROWABLE_STACK)
            STACK_PUSH_ONLY;
#           else
            STACK_PUSH;
#           endif
            if( !callFindAnyVariable(call,mem,False,False) )
            {
               callQuit(call,textcoreVAR_TYPE_UNKNOWN,GetStringTableEntry(call,mem,NULL));
            }
            STACK_POP;
            break;
         }

         case sePushGlobal:
         case sePushGlobalAsObject:
         case sePushGlobalParam:
         {
            wSEVar w_lhs = STACK_PUSH;
            VarName mem = IPTR_GET(VarName);

#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
            rSEObjectMem cachedGlobal = findCachedGlobal(call,mem,True);
            if ( NULL != SEOBJECTMEM_PTR(cachedGlobal) )
            {
               if( t!=sePushGlobal )
               {
                  SEVAR_INIT_REFERENCE(w_lhs,call->hGlobalObject,mem);
               }
               else
               {
                  SEVAR_COPY(w_lhs,SEOBJECTMEM_VAR(cachedGlobal));
               }
               SEOBJECTMEM_UNLOCK_R(cachedGlobal);
            }
            else
#           endif
            if( !callFindAnyVariable(call,mem,False,(t!=sePushGlobal)) )
            {
               if( t==sePushGlobal )
               {
                  callQuit(call,textcoreVAR_TYPE_UNKNOWN,GetStringTableEntry(call,mem,NULL));
                  break;
               }
               else
               {
                  wSEObjectMem w_smem;

#                 if 0!=JSE_MEMEXT_MEMBERS
                     SEOBJECTMEM_PTR(w_smem) = NULL;
#                 endif

                  if( jseOptDefaultLocalVars & call->Global->ExternalLinkParms.options )
                  {
                     wSEObject wObj;
                     if( call->hVariableObject==hSEObjectNull )
                        callCreateVariableObject(call,NULL);
                     SEOBJECT_ASSIGN_LOCK_W(wObj,call->hVariableObject);
                     w_smem = SEOBJ_CREATE_MEMBER(call,wObj,mem);
                     if( t==sePushGlobalParam )
                        SEVAR_INIT_REFERENCE(w_lhs,call->hVariableObject,mem);
                     SEOBJECT_UNLOCK_W(wObj);
                  }
                  else
                  {
                     if( t==sePushGlobalParam )
                     {
                        SEVAR_INIT_REFERENCE(w_lhs,CALL_GLOBAL(call),mem);
                     }
                     else
                     {
                        wSEObject wObj;
                        SEOBJECT_ASSIGN_LOCK_W(wObj,CALL_GLOBAL(call));
                        w_smem = SEOBJ_CREATE_MEMBER(call,wObj,mem);
                        SEOBJECT_UNLOCK_W(wObj);
                     }
                  }
                  if( t==sePushGlobalAsObject )
                  {
                     SEVAR_INIT_BLANK_OBJECT(call,w_lhs);
                     SEVAR_COPY(SEOBJECTMEM_VAR(w_smem),w_lhs);
                  }
#                 if 0!=JSE_MEMEXT_MEMBERS
                     if ( NULL != SEOBJECTMEM_PTR(w_smem) )
                        SEOBJECTMEM_UNLOCK_W(w_smem);
#                 endif
               }
            }
            if( t==sePushGlobalAsObject )
            {
               wSEVar wTmp2 = STACK_PUSH;
               SEVAR_COPY(wTmp2,w_lhs);
               SEVAR_DEREFERENCE(call,wTmp2);
               /* Update using original reference to do any dynamic stuff */
               if( SEVAR_GET_TYPE(wTmp2)==VUndefined )
               {
                  SEVAR_INIT_BLANK_OBJECT(call,wTmp2);
                  sevarDoPut(call,w_lhs,wTmp2);
               }
               /* and copy that object into the correct slot on the stack */
               SEVAR_COPY(w_lhs,wTmp2);
               STACK_POP;
            }
            assert( t==sePushGlobalParam || SEVAR_GET_TYPE(w_lhs)<VStorage );
            break;
         }

         case sePostDecGlobal:
         case sePostIncGlobal:
         case sePreDecGlobal:
         case sePreIncGlobal:
         {
            VarName mem = IPTR_GET(VarName);
#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
               rSEObjectMem cachedGlobal;
#           endif

            w_c_lhs = STACK_PUSH;
            w_c_rhs = STACK_PUSH;
            SEVAR_INIT_UNDEFINED(w_c_lhs);

#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
            cachedGlobal = findCachedGlobal(call,mem,True);
            if ( NULL != SEOBJECTMEM_PTR(cachedGlobal) )
            {
               SEVAR_INIT_REFERENCE(w_c_rhs,call->hGlobalObject,mem);
               SEOBJECTMEM_UNLOCK_R(cachedGlobal);
            }
            else
#           endif
            if( !callFindAnyVariable(call,mem,False,True) )
            {
               callQuit(call,textcoreVAR_TYPE_UNKNOWN,GetStringTableEntry(call,mem,NULL));
               break;
            }
            c_precrement = ( t<=sePreDecGlobal );
            assert( c_precrement || (t==sePostIncGlobal || t==sePostDecGlobal) );
            goto do_crement;
         }

         case seAssignGlobal:
         {
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs;
            VarName mem;
#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
               wSEObjectMem cachedGlobal;
#           endif

            /* We have to duplicate a string when copying now because at
             * the point the string is being used, we no longer know
             * where the string came from to make a copy only then.
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
               if ( SEVAR_GET_TYPE(w_rhs)==VString
                 && CALL_CBEHAVIOR
                 && SESTR_IS_CONSTANT(SEVAR_GET_STRING(w_rhs).data) )
               {
                  sevarDuplicateString(call,w_rhs);
               }
#           endif
            w_lhs = STACK_PUSH;
            mem = IPTR_GET(VarName);

#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
            SEOBJECTMEM_CAST_R(cachedGlobal) = findCachedGlobal(call,mem,False);
            if ( NULL != SEOBJECTMEM_PTR(cachedGlobal) )
            {
               wSEVar loc = SEOBJECTMEM_VAR(cachedGlobal);
               rSEObject rGlobalObject;

               SEOBJECT_ASSIGN_LOCK_R(rGlobalObject,call->hGlobalObject);
               if( SEOBJ_IS_DYNAMIC(rGlobalObject) )
               {
#                 if defined(JSE_OPERATOR_OVERLOADING) && (0!=JSE_OPERATOR_OVERLOADING)
                  forceDynamic:
#                 endif
                  /* force the put to call the operator function */
                  SEVAR_INIT_REFERENCE(w_lhs,call->hGlobalObject,mem);
                  SEVAR_DO_PUT(call,w_lhs,w_rhs);
               }
               else
               {
#                 if defined(JSE_OPERATOR_OVERLOADING) && (0!=JSE_OPERATOR_OVERLOADING)
                     if ( VObject == SEVAR_GET_TYPE(loc) )
                     {
                        rSEObject rLocObj;
                        jsebool isDynamic;
                        SEOBJECT_ASSIGN_LOCK_R(rLocObj,SEVAR_GET_OBJECT(loc));
                        isDynamic = SEOBJ_IS_DYNAMIC(rLocObj);
                        SEOBJECT_UNLOCK_R(rLocObj);
                        if ( isDynamic )
                           goto forceDynamic;
                     }
#                 endif
                  if( (SEOBJECTMEM_PTR(cachedGlobal)->attributes & jseReadOnly)==0 )
                  {
                     SEVAR_COPY(loc,w_rhs);
                  }
               }
               SEOBJECT_UNLOCK_R(rGlobalObject);
               SEOBJECTMEM_UNLOCK_W(cachedGlobal);
            }
            else
#           endif
            {
               if( !callFindAnyVariable(call,mem,False,True) )
               {
                  wSEObjectMem w_smem;
                  hSEObject hobj;
                  wSEObject wobj;

                  if( jseOptReqVarKeyword & call->Global->ExternalLinkParms.options )
                  {
                     callQuit(call,textcoreFUNC_OR_VAR_NOT_DECLARED,mem);
                     break;
                  }

                  if( jseOptDefaultLocalVars & call->Global->ExternalLinkParms.options )
                  {
                     if( call->hVariableObject==hSEObjectNull )
                        callCreateVariableObject(call,NULL);
                     hobj = call->hVariableObject;
                  }
                  else
                  {
                     rSEObject robj;
                     jsebool isDynamic;

                     hobj = CALL_GLOBAL(call);
                     SEOBJECT_ASSIGN_LOCK_W(robj,hobj);
                     isDynamic = SEOBJ_IS_DYNAMIC(robj);
                     SEOBJECT_UNLOCK_R(robj);

                     if( isDynamic )
                     {
                        wSEVar tmp = STACK_PUSH;
                        SEVAR_INIT_OBJECT(tmp,CALL_GLOBAL(call));
                        sevarPutValueEx(call,tmp,mem,w_rhs,False);
                        STACK_POP;
                        hobj = hSEObjectNull;
                     }
                  }
                  if( hobj != hSEObjectNull )
                  {
                     SEOBJECT_ASSIGN_LOCK_W(wobj,hobj);
                     w_smem = SEOBJ_CREATE_MEMBER(call,wobj,mem);
                     SEOBJECT_UNLOCK_W(wobj);
                     /* never can be an object, is undefined */
                     SEVAR_COPY(SEOBJECTMEM_VAR(w_smem),w_rhs);
                     SEOBJECTMEM_UNLOCK_W(w_smem);
                  }
               }
               else
               {
                  if( SEVAR_GET_TYPE(w_lhs)<VReference )
                  {
                     /* you can't assign to 'global' */
                     callQuit(call,textcoreNOT_LVALUE);
                     break;
                  }

                  SEVAR_DO_PUT(call,w_lhs,w_rhs);
               }
            }
            /* get rid of temporary, leaving assigned value on stack */
            STACK_POP;
            break;
         }

         case seTypeofGlobal:
         {
            wSEVar w_lhs = STACK_PUSH;
            if( !callFindAnyVariable(call,IPTR_GET(VarName),False,False) )
            {
               SEVAR_INIT_UNDEFINED(w_lhs);
            }
            goto do_typeof;
         }

         case seStartCatch:
         {
            rSEVar r_rhs = STACK0;  /* current error object top of stack */
            wSEVar w_lhs = STACK_PUSH;
            wSEObjectMem w_smem;
            wSEObject wobj;

            assert( call->tries!=NULL );
            assert( !call->tries->incatch );
            call->tries->incatch = True;

            /* create a new scope object with the error object and the name
             * and stick it on the scope chain.
             */
            SEVAR_INIT_BLANK_OBJECT(call,w_lhs);
            SEOBJECT_ASSIGN_LOCK_W(wobj,SEVAR_GET_OBJECT(w_lhs));
            w_smem = SEOBJ_CREATE_MEMBER(call,wobj,IPTR_GET(VarName));
            SEOBJECT_UNLOCK_W(wobj);
            SEVAR_COPY(SEOBJECTMEM_VAR(w_smem),r_rhs);
            SEOBJECTMEM_UNLOCK_W(w_smem);
            SEOBJECT_ASSIGN_LOCK_W(wobj,call->hScopeChain);
            CALL_ADD_SCOPE_OBJECT(call,wobj,w_lhs);
            SEOBJECT_UNLOCK_W(wobj);
#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
               /* NYI: this should only be False during the with, once the
                * with exits we can again use the cache, so we should store
                * this value and replace it during seScopeRemove below.
                * The current method will make caching stop getting used
                * once we hit a with for the rest of the function. It is
                * much less efficient (though safe.)
                */
               call->useCache = False;
#           endif

            /* pop two items off of stack */
            STACK_POPX(2);
            break;
         }

         case sePopDiscard:
            STACK_POP;
            break;

         case sePushUndefined:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_UNDEFINED(w_lhs);
            break;
         }

         case sePushFalse:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_BOOLEAN(w_lhs,False);
            break;
         }

         case sePushTrue:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_BOOLEAN(w_lhs,True);
            break;
         }

         case sePushNull:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_NULL(w_lhs);
            break;
         }

         case sePushThis:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_COPY(w_lhs,CALL_THIS);
            break;
         }

         case sePushGlobalObject:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_OBJECT(w_lhs,CALL_GLOBAL(call));
            break;
         }

         case sePushNewObject:
         {
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_INIT_BLANK_OBJECT(call,w_lhs);
            break;
         }

         case sePushNewArray:
         {
            wSEVar w_lhs = STACK_PUSH;
            wSEObjectMem w_mem;
            jsebool found;
            wSEObject wobj;

            SEVAR_INIT_BLANK_OBJECT(call,w_lhs);
            SEOBJECT_ASSIGN_LOCK_W(wobj,SEVAR_GET_OBJECT(w_lhs));
            w_mem = seobjNewMember(call,wobj,STOCK_STRING(length),&found);
            SEVAR_INIT_NUMBER(SEOBJECTMEM_VAR(w_mem),jseZero);
            SEOBJECTMEM_UNLOCK_W(w_mem);
            seobjMakeEcmaArray(call,wobj);
            SEOBJECT_UNLOCK_W(wobj);
            break;
         }

         case sePushArray:
         case sePushArrayParam:
         case sePushArrayAsObject:
            do_array_member(call,(t==sePushArrayParam),(t==sePushArrayAsObject));
            assert( t==sePushArrayParam || SEVAR_GET_TYPE(STACK0)<VStorage );
            break;

         case seDeleteArray:
         {
            uword8 nametmp;
            VarName mem;
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs = w_rhs-1;  /* STACK1 */

            /* ECMA spec doesn't say to convert it to an object */
            if( SEVAR_GET_TYPE(w_lhs)!=VObject )
            {
               callQuit(call,textcoreBAD_DELETE_VAR);
               break;
            }

            /* first calculate the member name using the object
             * index on the stack
             */
            varGrabStringTableEntry(call,w_rhs,&mem,&nametmp);
            SEVAR_INIT_BOOLEAN(w_lhs,seobjFullDeleteMember(call,SEVAR_GET_OBJECT(w_lhs),mem));
            STACK_POP;
            ReleaseStringTableEntry(/*call,*/mem,nametmp);

            break;
         }

         case seAssignArray:
         {
            uword8 nametmp;
            VarName mem;
            wSEVar w_rhs = STACK1;
            wSEVar w_lhs = w_rhs-1; /* STACK2 */

            /* first calculate the member name using the object
             * index on the stack
             */
            varGrabStringTableEntry(call,w_rhs,&mem,&nametmp);
            w_rhs++;      /* now point at the value to put */
            /* We have to duplicate a string when copying now because at
             * the point the string is being used, we no longer know
             * where the string came from to make a copy only then.
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
               if ( SEVAR_GET_TYPE(w_rhs)==VString
                 && CALL_CBEHAVIOR
                 && SESTR_IS_CONSTANT(SEVAR_GET_STRING(w_rhs).data) )
               {
                  sevarDuplicateString(call,w_rhs);
               }
#           endif
            if( !SEVAR_PUT_VALUE(call,w_lhs,mem,w_rhs) ) break;
            /* leave the assigned-from value on the stack, copying over
             * object underneath it.
             */
            SEVAR_COPY(w_lhs,w_rhs);
            /* drop two top items on stack */
            STACK_POPX(2);

            ReleaseStringTableEntry(/*call,*/mem,nametmp);

            break;
         }

         case seTypeof:
         {
            jsecharptr type;
            wSEVar w_lhs;

      do_typeof:
            type = UNISTR("undefined");

            /* one item in stack in, one out, we just change the top of the stack */
            w_lhs = STACK0;
            switch( SEVAR_GET_TYPE(w_lhs) )
            {
               case VNull:
                  type = UNISTR("object"); break;
               case VBoolean:
                  type = UNISTR("boolean"); break;
               case VNumber:
                  type = UNISTR("number"); break;
               case VString:
                  type = UNISTR("string"); break;
#              if defined(JSE_TYPE_BUFFER) && (0!=JSE_TYPE_BUFFER)
               case VBuffer:
                  type = UNISTR("buffer"); break;
#              endif
               case VObject:
                  type = SEVAR_IS_FUNCTION(call,w_lhs) ?
                     UNISTR("function") : UNISTR("object");
                  break;
               SEDBG( case VUndefined: break; )
               SEDBG( default: assert( False ); break; )
            }
            SEVAR_INIT_STRING_NULLLEN(call,w_lhs,type,strlen_jsechar(type));
            break;
         }

         case sePostDecArray:
         case sePostIncArray:
         case sePreDecArray:
         case sePreIncArray:
         {
            do_array_member(call,True,False);
            w_c_lhs = STACK0;
            w_c_rhs = STACK_PUSH;
            SEVAR_COPY(w_c_rhs,w_c_lhs);
            SEVAR_INIT_UNDEFINED(w_c_lhs);
            c_precrement = ( t<=sePreDecArray );
            assert( c_precrement || (t==sePostIncArray || t==sePostDecArray) );
            goto do_crement;
         }

         case sePushDup:
         {
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_COPY(w_lhs,r_rhs);
            break;
         }

         case sePushDup2:
         {
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = STACK_PUSHX(2);
            SEVAR_COPY(w_lhs,r_rhs);
            w_lhs--; r_rhs--;
            SEVAR_COPY(w_lhs,r_rhs);
            break;
         }

         case sePushDupUnder:
         {
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = STACK_PUSH;
            SEVAR_COPY(w_lhs,r_rhs);
            w_lhs--; r_rhs--;
            SEVAR_COPY(w_lhs,r_rhs);
            break;
         }

         case seSwap:
         {
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs = STACK1;
            /* can make it inline space since we are just using
             * it to move the data around, no possibility of a
             * garbage collection here.
             */
            struct _SEVar tmp;

            SEVAR_COPY(&tmp,w_rhs);
            SEVAR_COPY(w_rhs,w_lhs);
            SEVAR_COPY(w_lhs,&tmp);
            break;
         }

         case seToNumber:
         {
            wSEVar w_lhs = STACK0;
            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,NULL,seAdd)
            {
               if( SEVAR_GET_TYPE(w_lhs)!=VNumber )
               {
                  SEVAR_INIT_NUMBER(w_lhs,sevarConvertToNumber(call,w_lhs));
               }
            }
            break;
         }

         case seToObject:
         {
            wSEVar r_lhs = STACK0;
            if( SEVAR_GET_TYPE(r_lhs)!=VObject )
               sevarConvertToObject(call,r_lhs);
            break;
         }

         case seToObjectUnder:
         {
            wSEVar r_lhs = STACK1;
            if( SEVAR_GET_TYPE(r_lhs)!=VObject )
               sevarConvertToObject(call,r_lhs);
            break;
         }

         case seToCallFunc:
         case seToNewFunc:
         {
            wSEVar w_lhs = STACK0;
            rSEVar r_rhs;
            rSEObjectMem r_rhs_mem;

            /* first turn it into object */
            if( SEVAR_GET_TYPE(w_lhs)!=VObject )
            {
               /* If currently undefined, print the 'not a function'
                * error. The problem is we will not have a name for it,
                * all we have is an VUndefined value on the stack with
                * no idea where it came from
                */
               if( SEVAR_GET_TYPE(w_lhs)==VUndefined )
               {
                  callQuit(call,textcoreNOT_FUNCTION_VARIABLE,UNISTR(""));
               }
               else
               {
                  sevarConvertToObject(call,w_lhs);
               }
               if( CALL_QUIT(call) ) break;
            }

            if( t==seToCallFunc )
            {
               /* replace the given function variable with the _call if present */
               r_rhs = seobjGetFuncVar(call,w_lhs,STOCK_STRING(_call),&r_rhs_mem);
            }
            else
            {
               wSEVar tmp;
               assert( t==seToNewFunc );
               /* replace the 'this' variable with a new object, do this
                * first so if _call redirection, still get a new version
                * of original object
                */
               tmp = STACK1;
               sevarInitNewObject(call,tmp,w_lhs);
               /* replace the given function variable with the constructor */
               r_rhs = seobjGetFuncVar(call,w_lhs,STOCK_STRING(_construct),&r_rhs_mem);
            }

            if( r_rhs )
            {
               SEVAR_COPY(w_lhs,r_rhs);
#              if 0!=JSE_MEMEXT_MEMBERS
                  if ( 0 != SEOBJECTMEM_PTR(r_rhs_mem) )
                     SEOBJECTMEM_UNLOCK_R(r_rhs_mem);
#              endif
            }
            else
            {
               struct InvalidVarDescription BadDesc;
               JSECHARPTR_PUTC((jsecharptr)BadDesc.VariableName,UNICHR('\"'));
               if ( FindNames(call,w_lhs,JSECHARPTR_NEXT((jsecharptr)BadDesc.VariableName),
                              sizeof(BadDesc.VariableName)/sizeof(jsechar)-3,UNISTR("")) )
               {
                  strcat_jsechar((jsecharptr)BadDesc.VariableName,UNISTR("\" "));
               }
               else
               {
                  JSECHARPTR_PUTC((jsecharptr)BadDesc.VariableName,UNICHR('\0'));
               }
               callQuit(call,textcoreNOT_FUNCTION_VARIABLE,BadDesc.VariableName);
            }
            break;
         }

         case seScopeAdd:
         {
            wSEVar r_lhs = STACK0;
            wSEObject wobj;

            if( SEVAR_GET_TYPE(r_lhs)!=VObject ) sevarConvertToObject(call,r_lhs);
            SEOBJECT_ASSIGN_LOCK_W(wobj,call->hScopeChain);
            CALL_ADD_SCOPE_OBJECT(call,wobj,r_lhs);
            SEOBJECT_UNLOCK_W(wobj);
            STACK_POP;

#           if defined(JSE_CACHE_GLOBAL_VARS) && JSE_CACHE_GLOBAL_VARS==1
               /* NYI: this should only be False during the with, once the
                * with exits we can again use the cache, so we should store
                * this value and replace it during seScopeRemove below.
                * The current method will make caching stop getting used
                * once we hit a with for the rest of the function. It is
                * much less efficient (though safe.)
                */
               call->useCache = False;
#           endif

            break;
         }

         case seScopeRemove:
            CALL_REMOVE_SCOPE_OBJECT(call);
            break;

         case seEndTry:
            assert( call->tries!=NULL );
            /* pretend we already did the finally so we can just do the
             * cleanup and go on to the next instruction. Otherwise,
             * we already have some place to go.
             */
            assert( call->tries->loc!=(ADDR_TYPE)-1 );
            assert( STACK_FROM_STACKPTR(call->tries->fptr)==FRAME );
            call->tries->endtryreached = True;
            goto handle_finally;

         case seEndCatch:
            assert( call->tries!=NULL );
            assert( call->tries->incatch );
            /* remove the added 'with' count */
            CALL_REMOVE_SCOPE_OBJECT(call);
            call->tries->incatch = False;
            break;

         case seReturn:
            /* If we are trying to return and are currently inside a try for
             * this function
             */
            if( call->tries!=NULL && STACK_FROM_STACKPTR(call->tries->fptr)==FRAME )
            {
               /* first must do finally, then get back here to 'return'
                * again.
                */
               IPTR--;
               goto handle_finally;
            }

            callReturnFromFunction(call);
            if( FRAME==NULL ) return False;
            if( call->continue_count==0 ) break;
            if( call->continue_count>1 )
            {
               if( --call->continue_count!=2 )
                  break;
               else
                  call->continue_count = 2+JSE_INFREQUENT_COUNT;
            }
            return True;

         case seReturnThrow:
            SEVAR_COPY(&(call->error_var),STACK0);
            CALL_SET_ERROR(call,FlowError);
            if( !callErrorTrapped(call) ) callPrintError(call);
            /* now that we have an error state, the above code to
             * catch an error will trigger and do error handling, we
             * do not explicitly return.
             */
            break;


         /* ----------------------------------------------------------------------
          * operators: all operators take no parameters.
          * ---------------------------------------------------------------------- */


         case seNegate:
         {
            wSEVar w_lhs = STACK0;
            jsenumber num;

            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,NULL,seSubtract)
            {
               if( SEVAR_GET_TYPE(w_lhs)==VNumber )
               {
                  num = SEVAR_GET_NUMBER(w_lhs);
               }
               else
               {
                  num = sevarConvertToNumber(call,w_lhs);
               }
               if( !jseIsNaN(num) )
                  num = JSE_FP_NEGATE(num);
               SEVAR_INIT_NUMBER(w_lhs,num);
            }
            break;
         }

         case seBoolNot:
         {
            wSEVar w_lhs = STACK0;
            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,NULL,t)
            {
               SEVAR_INIT_BOOLEAN(w_lhs,!sevarConvertToBoolean(call,w_lhs));
            }
            break;
         }

         case seBitNot:
         {
            wSEVar w_lhs = STACK0;
            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,NULL,t)
            {
               sevarConvert(call,w_lhs,jseToInt32);
               SEVAR_INIT_SLONG(w_lhs,~SEVAR_GET_SLONG(w_lhs));
            }
            break;
         }

         case seInstanceof:
         {
            jsebool result = False;
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = (wSEVar)(r_rhs-1);
            rSEObjectMem rObjMem;

#           if 0!=JSE_MEMEXT_MEMBERS
               SEOBJECTMEM_PTR(rObjMem) = NULL;
#           endif

            if( VObject == SEVAR_GET_TYPE(w_lhs) )
            {
               if( !SEVAR_IS_FUNCTION(call,r_rhs) )
               {
                  callQuit(call,textcoreINSTANCEOF_ONLY_FOR_FUNCTIONS);
               }
               else
               {
                  rSEObject robj;
                  rSEObjectMem rFuncObjMem;

                  assert( VObject == SEVAR_GET_TYPE(r_rhs) );

                  SEOBJECT_ASSIGN_LOCK_R(robj,SEVAR_GET_OBJECT(r_rhs));
                  rFuncObjMem = rseobjGetMemberStruct(call,robj,
                     STOCK_STRING(prototype));
                  SEOBJECT_UNLOCK_R(robj);
                  if( SEOBJECTMEM_PTR(rFuncObjMem) == NULL )
                  {
                     /* In the ECMA spec, all functions get an object
                      * prototype created. Since we have auto-conversion
                      * to object, we do not do this (it saves a lot
                      * of memory.) Therefore if the entry is NULL,
                      * it just hasn't been created yet.
                      */
                  }
                  else if( VObject != SEVAR_GET_TYPE(SEOBJECTMEM_VAR(rFuncObjMem)) )
                  {
                     SEOBJECTMEM_UNLOCK_R(rFuncObjMem);
                     callQuit(call,textcoreFUNCTION_BAD_PROTOTYPE_PROPERTY);
                  }
                  else
                  {
                     rSEVar rObjectPrototype;
                     jsebool looping = False;
#                    if 0!=defined(JSE_MEMEXT_OBJECTS)
                        /* real marking is too slow, good enough that if we run this
                         * loop too many times then it is a recursive prototype loop.
                         */
#                       define TOO_MANY_PROTOTYPES 30
                        uint prototypeCount = 0;
#                    endif
                     hSEObject hFuncObj = SEVAR_GET_OBJECT(SEOBJECTMEM_VAR(rFuncObjMem));
                     SEOBJECTMEM_UNLOCK_R(rFuncObjMem);

                     /* none of the code in the loop can do anything that
                      * can cause a garbage collection, which would bomb
                      * with us having marked stuff.
                      */
                     rObjectPrototype = w_lhs;
                     while( 1 )
                     {
#                       if 0==defined(JSE_MEMEXT_OBJECTS)
                           if( SEOBJ_WAS_FLAGGED(SEVAR_GET_OBJECT(rObjectPrototype)) )
                           {
                              looping = True; break;
                           }
                           else
                           {
                              SEOBJ_MARK_FLAGGED(SEVAR_GET_OBJECT(rObjectPrototype));
                           }
#                       else
                           if ( ++prototypeCount == TOO_MANY_PROTOTYPES )
                           {
                              looping = True;
                              break;
                           }
#                       endif
                        SEOBJECT_ASSIGN_LOCK_R(robj,SEVAR_GET_OBJECT(rObjectPrototype));
#                       if 0!=JSE_MEMEXT_MEMBERS
                           if ( NULL != SEOBJECTMEM_PTR(rObjMem) )
                              SEOBJECTMEM_UNLOCK_R(rObjMem);
#                       endif
                        rObjMem = rseobjGetMemberStruct(call,robj,STOCK_STRING(_prototype));
                        SEOBJECT_UNLOCK_R(robj);
                        if( NULL != SEOBJECTMEM_PTR(rObjMem) )
                        {
                           if( SEVAR_GET_TYPE(rObjectPrototype) != VObject)
                              break;
                           if( SEVAR_GET_OBJECT(SEOBJECTMEM_VAR(rObjMem)) == hFuncObj )
                           {
                              result = True;
                              break;
                           }
                           rObjectPrototype = SEOBJECTMEM_VAR(rObjMem);
                        }
                        else
                        {
                           rSEObject rPrObj;
                           SEOBJECT_ASSIGN_LOCK_R(rPrObj,SEVAR_GET_OBJECT(rObjectPrototype));
                           /* check default prototype */
                           if( SEOBJ_DEFAULT_PROTOTYPE(call,SEOBJECT_PTR(rPrObj)) == hFuncObj )
                              result = True;
                           SEOBJECT_UNLOCK_R(rPrObj);

                           /* at any rate, this is the last chance. */
                           break;
                        }
                     }

#                    if 0==defined(JSE_MEMEXT_OBJECTS)
                        rObjectPrototype = lhs;
                        while( 1 )
                        {
                           seVar newproto;

                           if( SEOBJ_WAS_FLAGGED(SEVAR_GET_OBJECT(rObjectPrototype)) )
                           {
                              SEOBJ_MARK_NOT_FLAGGED(SEVAR_GET_OBJECT(rObjectPrototype));

                              newproto = seobjGetMember(call,SEVAR_GET_OBJECT(rObjectPrototype),
                                                        STOCK_STRING(_prototype));
                              if( newproto )
                              {
                                 if( SEVAR_GET_TYPE(rObjectPrototype) != VObject)
                                    break;
                                 rObjectPrototype = newproto;
                              }
                              else
                                 break;
                           }
                           else
                           {
                              break;
                           }
                        }
#                    endif

                     /* now that we've cleared up our mess, we can
                      * do an error if one
                      */
                     if( looping )
                     {
                        callQuit(call,textcorePROTOTYPE_LOOPS);
                        result = False;
                     }
                  }
               }
            }

            SEVAR_INIT_BOOLEAN(w_lhs,result);
            STACK_POP;
#           if 0!=JSE_MEMEXT_MEMBERS
               if ( NULL != SEOBJECTMEM_PTR(rObjMem) )
                  SEOBJECTMEM_UNLOCK_R(rObjMem);
#           endif
            break;
         }

         case seIn:
         {
            wSEVar r_rhs = STACK0;
            wSEVar w_lhs = (wSEVar)(r_rhs-1); /* STACK1 */

            if( SEVAR_GET_TYPE(r_rhs)!=VObject )
            {
               callError(call,textcoreIN_NEEDS_OBJECT);
            }
            else
            {
               uword8 nametmp;
               jsebool result;
               VarName mem;
               rSEObject robj;

               varGrabStringTableEntry(call,w_lhs,&mem,&nametmp);

               /* junk the result to overright 'rhs' which is going to be
                * dropped anyway.
                */
               SEOBJECT_ASSIGN_LOCK_R(robj,SEVAR_GET_OBJECT(r_rhs));
               result = seobjHasProperty(call,robj,mem,r_rhs,HP_DEFAULT);
               SEOBJECT_UNLOCK_R(robj);
               ReleaseStringTableEntry(/*call,*/mem,nametmp);
               SEVAR_INIT_BOOLEAN(w_lhs,result);
            }
            STACK_POP;
            break;
         }

         /* Overloaded operators from here on. Each overloaded op
          * starts with the same code, load lhs and rhs, call
          * 'IF_OPERATOR_NOT_OVERLOADED'. Do NOT try to move this
          * to the top of the loop. It makes the compiler's optimizer
          * not be able to do much with it. As a result, the code
          * will not only become quite a bit slower, it will
          * actually be slightly bigger too.
          */
         case seEqual:
         case seNotEqual:
         case seStrictEqual:
         case seStrictNotEqual:
         {
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = (wSEVar)(r_rhs-1);  /* STACK1 */

            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,r_rhs,t)
            {
               jsebool bool;
               jsenumber lhs_num, rhs_num;

               if ( ( SEVAR_GET_TYPE(w_lhs)==VNumber && SEVAR_GET_TYPE(r_rhs)==VNumber ) \
                 && ( ((lhs_num=SEVAR_GET_NUMBER(w_lhs)),!(jseIsNaN(lhs_num))) \
                   && ((rhs_num=SEVAR_GET_NUMBER(r_rhs)),!(jseIsNaN(rhs_num))) ) )
               {
                  bool = JSE_FP_EQ(lhs_num,rhs_num);
               }
#              if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
                  else if ( t < seStrictEqual )
                  {
                     assert( t==seEqual || t==seNotEqual);
                     bool = SEVAR_COMPARE_EQUALITY(call,w_lhs,r_rhs);
                  }
                  else
                  {
                     assert( t==seStrictEqual || t==seStrictNotEqual );
                     bool = sevarECMACompareEquality(call,w_lhs,r_rhs,True);
                  }
#              else
                  else
                  {
                     assert( ((seNotEqual<t) && (t==seStrictEqual || t==seStrictNotEqual)) \
                          || (!(seNotEqual<t) && (t==seEqual || t==seNotEqual)) );
                     bool = sevarECMACompareEquality(call,w_lhs,r_rhs,seNotEqual<t);
                  }
#              endif
               /* the NOT operators get their result reversed */
               assert( ((t&1) && (t==seNotEqual || t==seStrictNotEqual)) \
                    || (!(t&1) && (t==seEqual || t==seStrictEqual)) );
               assert( ((t&1)^bool)==True  || ((t&1)^bool)==False );
               SEVAR_INIT_BOOLEAN(w_lhs, 0 != ((t&1) ^ bool) );
               STACK_POP;
            }
            break;
         }

         case seLess:
         case seGreaterEqual:
         case seGreater:
         case seLessEqual:
         {
            wSEVar r_rhs = STACK0;
            wSEVar w_lhs = r_rhs-1;

            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,r_rhs,t)
            {
               sint result;
               jsenumber lhs_num, rhs_num;

               /* following (t < seGreater) relies on certain order of opcodes */
               assert( ( ( t < seGreater ) && (t==seLess || t==seGreaterEqual) ) \
                    || ( !( t < seGreater ) && (t==seGreater || t==seLessEqual) ) );
               if ( ( SEVAR_GET_TYPE(w_lhs)==VNumber && SEVAR_GET_TYPE(r_rhs)==VNumber ) \
                 && ( ((lhs_num=SEVAR_GET_NUMBER(w_lhs)),!(jseIsNaN(lhs_num))) \
                   && ((rhs_num=SEVAR_GET_NUMBER(r_rhs)),!(jseIsNaN(rhs_num))) ) )
               {
                  result = ( t < seGreater )
                         ? JSE_FP_LT(lhs_num,rhs_num)
                         : JSE_FP_LT(rhs_num,lhs_num);
               }
               else
               {
                  result = ( t < seGreater )
                         ? SEVAR_COMPARE_LESS(call,w_lhs,r_rhs)
                         : SEVAR_COMPARE_LESS(call,r_rhs,w_lhs) ;
               }
               /* if one of the seXXXEqual operators then we need to reverse the
                * result, except on -1 still must return False.  This works
                * on the following line because the seXXXXEqual opcodes are odd,
                * while the other opcodes are not, so XOR will flip 0 to 1 and 1
                * to 0, while -1^1 will still be <0, as the following asserts show
                */
               assert( ( (t&1) && (t==seGreaterEqual || t==seLessEqual) ) \
                    || ( !(t&1) && (t==seGreater || t==seLess) ) );
               assert( 1==result  ||  0==result  ||  -1==result );
               SEVAR_INIT_BOOLEAN(w_lhs,1 == (result ^ (t&1)) );
               STACK_POP;
            }
            break;
         }

         case seSubtract:
         {
            rSEVar r_rhs = STACK0;
            wSEVar w_lhs = (wSEVar)(r_rhs-1);
            /* No need to check operator overloading because the
             * special case for subtract is mutually exclusive:
             * fall thru into the test in add
             */
#           if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
            if( CALL_CBEHAVIOR && SEVAR_ARRAY_PTR(w_lhs) && SEVAR_ARRAY_PTR(r_rhs) )
            {
               if( SEVAR_GET_STRING(w_lhs).data!=SEVAR_GET_STRING(r_rhs).data )
               {
                  callError(call,textcoreCAN_ONLY_SUBTRACT_SAME_ARRAY);
                  break;
               }
               else
               {
                  SEVAR_INIT_SLONG(w_lhs,(SEVAR_GET_STRING(w_lhs).loffset -
                                          SEVAR_GET_STRING(r_rhs).loffset));
               }
               STACK_POP;
               break;
            }
#           endif
         }
     /* !!!!!!!!!!!!!fallthru intentional!!!!!!!!!!!!!!! */
         case seAdd:
         {
            jsenumber lnum, rnum;
            jseConversionTarget dest_type;
            jsebool subtracting = ( seSubtract==t ) ; /* else adding */
            wSEVar w_rhs = STACK0;
            wSEVar w_lhs = w_rhs-1;

            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,w_rhs,t)
            {
               /* This is the most common occurrence, so handle it specially here */
               if( VNumber == SEVAR_GET_TYPE(w_lhs) && VNumber == SEVAR_GET_TYPE(w_rhs) )
               {
                  jsenumber result;

               GetAndAddTwoNumbers:
                  lnum = SEVAR_GET_NUMBER(w_lhs);
                  rnum = SEVAR_GET_NUMBER(w_rhs);

               QuickAddTwoNumbers:
                  if ( jseIsFinite(lnum) && jseIsFinite(rnum) )
                  {
                     /* easiest case; two regular numbers */
                     result = ( subtracting ) ? JSE_FP_SUB(lnum,rnum) : JSE_FP_ADD(lnum,rnum) ;
                  }
                  else
                  {
                     result = SpecialMathOnNonFiniteNumbers(call,lnum,rnum,t);
                  }
                  /* turn 2 operands into one result */
                  STACK_POP;
                  SEVAR_INIT_NUMBER(w_lhs,result);
                  break;
               }

               dest_type = ( subtracting ) ? jseToNumber : jseToPrimitive ;
               if( SEVAR_GET_TYPE(w_lhs)==VObject )
               {
                  sevarConvert(call,w_lhs,dest_type);
               }
               if( SEVAR_GET_TYPE(w_rhs)==VObject )
               {
                  sevarConvert(call,w_rhs,dest_type);
               }

               if (
#                  if defined(JSE_C_EXTENSIONS) && (0!=JSE_C_EXTENSIONS)
                   (!CALL_CBEHAVIOR ||
                    (VString==SEVAR_GET_TYPE(w_lhs) && VString==SEVAR_GET_TYPE(w_rhs) &&
                     !subtracting)) &&
#                  endif
                   (VString==SEVAR_GET_TYPE(w_lhs) || VString==SEVAR_GET_TYPE(w_rhs)) )
               {
                  /* pure javascript string behavior */
                  if ( subtracting )
                  {
                     /* subtraction will treat both sides as numbers */
                     lnum = (SEVAR_GET_TYPE(w_lhs)==VNumber)?SEVAR_GET_NUMBER(w_lhs):
                        sevarConvertToNumber(call,w_lhs);
                     rnum = (SEVAR_GET_TYPE(w_rhs)==VNumber)?SEVAR_GET_NUMBER(w_rhs):
                        sevarConvertToNumber(call,w_rhs);

                     if( (jseOptWarnBadMath & call->Global->ExternalLinkParms.options) &&
                         (jseIsNaN(lnum) || jseIsNaN(rnum)) )
                     {
                        callError(call,textcoreIS_NAN);
                        break;
                     }
                     goto QuickAddTwoNumbers;
                  }
                  else
                  {
                     /* addition; concatenate strings together */
                     if( SEVAR_GET_TYPE(w_lhs)!=VString )
                     {
                        sevarConvertToString(call,w_lhs);
                     }
                     if( SEVAR_GET_TYPE(w_rhs)!=VString )
                     {
                        sevarConvertToString(call,w_rhs);
                     }

                     ConcatenateStrings(call,w_lhs,w_lhs,w_rhs);
                     STACK_POP;
                  }
               }
               else if( SEVAR_ARRAY_PTR(w_lhs) || SEVAR_ARRAY_PTR(w_rhs) )
               {
                  /* Not necessarily c-behavior - buffers always act this way
                   * array can only add to an integer
                   */
                  wSEVar wVars[2];
                  int which = ( SEVAR_ARRAY_PTR(w_lhs) || VObject == SEVAR_GET_TYPE(w_lhs) )
                     ? 0 : 1 ;
                  wVars[0] = w_lhs, wVars[1] = w_rhs;
#              define other which ^ 1
                  if( SEVAR_ARRAY_PTR(wVars[other]) || VObject == SEVAR_GET_TYPE(wVars[other]) )
                  {
                     callError(call,textcoreCANNOT_ADD_ARRAYS);
                     break;
                  }
                  else
                  {
                     JSE_POINTER_SINDEX offset =
                        (JSE_POINTER_SINDEX)JSE_FP_CAST_TO_SLONG(SEVAR_GET_NUMBER_VALUE(wVars[other]));
                     if ( subtracting )
                        offset = -offset;

                     SEVAR_INIT_ARRAY_SIBLING(w_lhs,wVars[which],offset);
                     STACK_POP;
                  }
               }
               else
               {
                  /* Not an array; Must add and convert to the largest element type. */
                  if( SEVAR_GET_TYPE(w_lhs)!=VNumber )
                  {
                     jsenumber val = sevarConvertToNumber(call,w_lhs);
                     SEVAR_INIT_NUMBER(w_lhs,val);
                  }
                  if( SEVAR_GET_TYPE(w_rhs)!=VNumber )
                  {
                     jsenumber val = sevarConvertToNumber(call,w_rhs);
                     SEVAR_INIT_NUMBER(w_rhs,val);
                  }
                  goto GetAndAddTwoNumbers;
               }
            }
            break;
         }
         case seMultiply: case seDivide:
         case seModulo: case seShiftLeft: case seSignedShiftRight:
         case seUnsignedShiftRight: case seBitOr: case seBitXor: case seBitAnd:
         {
            jsenumber fone, ftwo;
            slong lone UNUSED_INITIALIZER(0);
            slong ltwo UNUSED_INITIALIZER(0);
            jsenumber Result;
            wSEVar r_rhs = STACK0;
            wSEVar w_lhs = r_rhs-1;

            IF_OPERATOR_NOT_OVERLOADED(call,w_lhs,r_rhs,t)
            {
               fone = (SEVAR_GET_TYPE(w_lhs)==VNumber)?SEVAR_GET_NUMBER(w_lhs):
                  sevarConvertToNumber(call,w_lhs);
               ftwo = (SEVAR_GET_TYPE(r_rhs)==VNumber)?SEVAR_GET_NUMBER(r_rhs):
                  sevarConvertToNumber(call,r_rhs);

               /* Get rid of the top operator, we will simply overwrite the second */
               STACK_POP;

               if( (jseOptWarnBadMath & call->Global->ExternalLinkParms.options) &&
                   (jseIsNaN(fone) || jseIsNaN(ftwo)) )
               {
                  callError(call,textcoreIS_NAN);
                  break;
               }

               if ( BEGIN_SEOP_32BITS <= t )
               {
                  /* bitwise operators mod into signed 32-bit integer */
                  assert( t <= END_SEOP_32BITS );
                  lone = JSE_FP_CAST_TO_SLONG(convertToSomeInteger(fone,jseToInt32));
                  ltwo = JSE_FP_CAST_TO_SLONG(convertToSomeInteger(ftwo,jseToInt32));
               }
               else
               {
                  if( !jseIsFinite(fone) || !jseIsFinite(ftwo) )
                  {
                     SEVAR_INIT_NUMBER(w_lhs,SpecialMathOnNonFiniteNumbers(call,fone,ftwo,t));
                     break;
                  }
               }

               switch( t )
               {
                  case seDivide:
                     if( jseIsZero(ftwo) )
                     {
                        if ( (jseOptWarnBadMath & call->Global->ExternalLinkParms.options) )
                        {
                           callError(call,textcoreCANNOT_DIVIDE_BY_ZERO);
                           break;
                        }
                        if ( jseIsZero(fone) )
                           Result = jseNaN ;
                        else
                           Result = ( jseIsNegative(ftwo) == jseIsNegative(fone) )
                              ? jseInfinity : jseNegInfinity ;
                     }
                     else
                     {
                        Result = JSE_FP_DIV(fone,ftwo);
                     }
                     break;
                  case seMultiply:
                     Result = JSE_FP_MUL(fone,ftwo);
                     break;
                  case seModulo:
                     if ( jseIsZero(ftwo) )
                     {
                        if ( (jseOptWarnBadMath & call->Global->ExternalLinkParms.options) )
                        {
                           callError(call,textcoreCANNOT_DIVIDE_BY_ZERO);
                           break;
                        }
                        Result = jseNaN;
                     }
                     else
                     {
                        /* ecmascript % is more like C library fmod */
#                       if (0!=JSE_FLOATING_POINT)
                           Result = JSE_FP_FMOD(fone,ftwo);
#                       else
                           Result = JSE_FP_MOD(fone,ftwo);
#                       endif
                     }
                     break;
                     /* Under Metrowerks, a (ulong) can fail with the minimum integer
                      * stored as a jsenumber.  Therefore, the following casts have been
                      * changed to (slong).  If this causes a problem with another compiler,
                      * then put the (slong) casts in an #ifdef __MWERKS__ block.
                      * It should not make a difference, however
                      */
                     /* Under Watcom11, casts from jsenumber to slong fail when the slong value
                      * should be negative (it gives the wrong negative number.)
                      */
                  case seBitOr:
                     Result = JSE_FP_CAST_FROM_SLONG(lone | ltwo);
                     break;
                  case seBitAnd:
                     Result = JSE_FP_CAST_FROM_SLONG(lone & ltwo);
                     break;
                  case seBitXor:
                     Result = JSE_FP_CAST_FROM_SLONG(lone ^ ltwo);
                     break;
                  case seShiftLeft:
                     Result = JSE_FP_CAST_FROM_SLONG(lone << (ltwo & 0x1F));
                     break;
                  case seSignedShiftRight:
                  case seUnsignedShiftRight:
                  {
                     /* NYI: this algorithm sucks, at least make a table, get
                      * rid of this junk
                      */

                     /* these shift algorithms might be a bit slower, but they will work on
                      * all systems regardless of the way that systems >> operator works.
                      */
                     uint bits = (uint)ltwo & 0x1f;
                     /* construct a mask of 0x80000000 signed shifted right
                      * bits times and or it with the value. We do this by
                      * shifting right 1 32-bits times and oring in ones
                      * each time, then notting the value.
                      */
                     ulong mask = 1;
                     uint maskbits = 32 - bits;

                     while( (--maskbits)>0 )
                     {
                        mask = (mask<<1) | 1;
                     }
                     if ( (lone < 0)  && seSignedShiftRight==t )
                     {
                        /* signed shifting */
                        Result = JSE_FP_CAST_FROM_SLONG( (lone >> bits) | (slong)(~mask) ) ;
                     }
                     else
                     {
                        /* unsigned shifting */
                        Result = JSE_FP_CAST_FROM_ULONG(((ulong)lone >> bits) & mask);
                     }
                     break;
                  }
               }

               /* lhs is the stack 'slot' we will overwrite */
               SEVAR_INIT_NUMBER(w_lhs,Result);
            }
            break;
         }

         case seThisAndValue:
         {
            wSEVar w_lhs = STACK0;
            wSEVar w_rhs = STACK_PUSH;

            SEVAR_COPY(w_rhs,w_lhs);
            SEVAR_DEREFERENCE(call,w_rhs);
            if( SEVAR_GET_TYPE(w_lhs)==VReference )
               SEVAR_INIT_OBJECT(w_lhs,w_lhs->data.ref_val.hBase);
            else
               SEVAR_INIT_OBJECT(w_lhs,CALL_GLOBAL(call));
            break;
         }

#        ifndef NDEBUG
            /* catch bad opcodes */
         default: assert( False ); break;
#        endif
      }
   }
}
