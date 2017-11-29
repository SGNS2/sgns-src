/* $Id: pluto.c,v 1.24 2004/08/15 20:47:13 bsunshin Exp $ */

/* Pluto - Heavy-duty persistence for Lua
 * Copyright (C) 2004 by Ben Sunshine-Hill, and released into the public 
 * domain. People making use of this software as part of an application
 * are politely requested to email the author at ben@bluelabelgames.com 
 * with a brief description of the application, primarily to satisfy his
 * curiosity.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Modified by Jason Lloyd-Price, 2012
   to work with Lua 5.1.5 */

#include "lua.h"
#include "pluto.h"

#include "lapi.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llimits.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "lauxlib.h"


#include <stdint.h>

/* #define PLUTO_DEBUG */




#ifdef PLUTO_DEBUG
#include <stdio.h>
#endif

#define PLUTO_TPERMANENT 101

#define verify(x) { int v = (int)((x)); (void)v; lua_assert(v); }

typedef struct PersistInfo_t {
	lua_State *L;
	intptr_t counter;
	lua_Chunkwriter writer;
	void *ud;
#ifdef PLUTO_DEBUG
	int level;
#endif
} PersistInfo;

#ifdef PLUTO_DEBUG
void printindent(int indent)
{
	int il;
	for(il=0; il<indent; il++) {
		printf("  ");
	}
}
#endif

/* Mutual recursion requires prototype */
static void persist(PersistInfo *pi);

/* A simple reimplementation of the unfortunately static function luaA_index.
 * Does not support the global table, registry, or upvalues. */
static StkId getobject(lua_State *L, int stackpos)
{
	if(stackpos > 0) {
		lua_assert(L->base+stackpos-1 < L->top);
		return L->base+stackpos-1;
	} else {
		lua_assert(L->top-stackpos >= L->base);
		return L->top+stackpos;
	}
}

/* Choose whether to do a regular or special persistence based on an object's
 * metatable. "default" is whether the object, if it doesn't have a __persist
 * entry, is literally persistable or not.
 * Pushes the unpersist closure and returns true if special persistence is 
 * used. */
static int persistspecialobject(PersistInfo *pi, int defaction)
{
					/* perms reftbl ... obj */
	/* Check whether we should persist literally, or via the __persist
	 * metafunction */
	if(!lua_getmetatable(pi->L, -1)) {
		if(defaction) {
			{
				int zero = 0;
				pi->writer(pi->L, &zero, sizeof(int), pi->ud);
			}
			return 0;
		} else {
			lua_pushstring(pi->L, "Type not literally persistable by default");
			lua_error(pi->L);
		}
	}
					/* perms reftbl sptbl ... obj mt */
	lua_pushstring(pi->L, "__persist");
					/* perms reftbl sptbl ... obj mt "__persist" */
	lua_rawget(pi->L, -2);
					/* perms reftbl sptbl ... obj mt __persist? */
	if(lua_isnil(pi->L, -1)) {
					/* perms reftbl sptbl ... obj mt nil */
		lua_pop(pi->L, 2);
					/* perms reftbl sptbl ... obj */
		if(defaction) {
			{
				int zero = 0;
				pi->writer(pi->L, &zero, sizeof(int), pi->ud);
			}
			return 0;
		} else {
			lua_pushstring(pi->L, "Type not literally persistable by default");
			lua_error(pi->L);
			return 0; /* not reached */
		}
	} else if(lua_isboolean(pi->L, -1)) {
					/* perms reftbl sptbl ... obj mt bool */
		if(lua_toboolean(pi->L, -1)) {
					/* perms reftbl sptbl ... obj mt true */
			lua_pop(pi->L, 2);
					/* perms reftbl sptbl ... obj */
			{
				int zero = 0;
				pi->writer(pi->L, &zero, sizeof(int), pi->ud);
			}
			return 0;
		} else {
			lua_pushstring(pi->L, "Metatable forbade persistence");
			lua_error(pi->L);
			return 0; /* not reached */
		}
	} else if(!lua_isfunction(pi->L, -1)) { 
		lua_pushstring(pi->L, "__persist not nil, boolean, or function");
		lua_error(pi->L);
	}
					/* perms reftbl ... obj mt __persist */
	lua_pushvalue(pi->L, -3);
					/* perms reftbl ... obj mt __persist obj */
#ifdef PLUTO_PASS_USERDATA_TO_PERSIST
	lua_pushlightuserdata(pi->L, (void*)pi->writer);
	lua_pushlightuserdata(pi->L, pi->ud);
					/* perms reftbl ... obj mt __persist obj ud */
	lua_call(pi->L, 3, 1);
					/* perms reftbl ... obj mt func? */
#else
	lua_call(pi->L, 1, 1);
					/* perms reftbl ... obj mt func? */
#endif
					/* perms reftbl ... obj mt func? */
	if(!lua_isfunction(pi->L, -1)) {
		lua_pushstring(pi->L, "__persist function did not return a function");
		lua_error(pi->L);
	}
					/* perms reftbl ... obj mt func */
	{
		int one = 1;
		pi->writer(pi->L, &one, sizeof(int), pi->ud);
	}
	persist(pi);
					/* perms reftbl ... obj mt func */
	lua_pop(pi->L, 2);
					/* perms reftbl ... obj */
	return 1;
}

static void persisttable(PersistInfo *pi)
{
					/* perms reftbl ... tbl */
	if(persistspecialobject(pi, 1)) {
					/* perms reftbl ... tbl */
		return;
	}
					/* perms reftbl ... tbl */
	/* First, persist the metatable (if any) */
	if(!lua_getmetatable(pi->L, -1)) {
		lua_pushnil(pi->L);
	}
					/* perms reftbl ... tbl mt/nil */
	persist(pi);
	lua_pop(pi->L, 1);
					/* perms reftbl ... tbl */

	/* Now, persist all k/v pairs */
	lua_pushnil(pi->L);	
					/* perms reftbl ... tbl nil */
	while(lua_next(pi->L, -2)) {
					/* perms reftbl ... tbl k v */
		lua_pushvalue(pi->L, -2);
					/* perms reftbl ... tbl k v k */
		persist(pi);
		lua_pop(pi->L, 1);
					/* perms reftbl ... tbl k v */
		persist(pi);
		lua_pop(pi->L, 1);
					/* perms reftbl ... tbl k */
	}
					/* perms reftbl ... tbl */
	/* Terminate list */
	lua_pushnil(pi->L);
					/* perms reftbl ... tbl nil */
	persist(pi);
	lua_pop(pi->L, 1);
					/* perms reftbl ... tbl */
}

static void persistuserdata(PersistInfo *pi) {
					/* perms reftbl ... udata */
	if(persistspecialobject(pi, 0)) {
					/* perms reftbl ... udata */
		return;
	} else {
	/* Use literal persistence */
		int length = (int)(uvalue(getobject(pi->L, -2))->len);
		pi->writer(pi->L, &length, sizeof(int), pi->ud);
		pi->writer(pi->L, lua_touserdata(pi->L, -1), length, pi->ud);
		if(!lua_getmetatable(pi->L, -1)) {
					/* perms reftbl ... udata */
			lua_pushnil(pi->L);
					/* perms reftbl ... udata mt/nil */
		}
		persist(pi);
		lua_pop(pi->L, 1);
					/* perms reftbl ... udata */
	}
}


static Proto *toproto(lua_State *L, int stackpos)
{
	return &getobject(L, stackpos)->value.gc->p;
}

static UpVal *toupval(lua_State *L, int stackpos)
{
	return &getobject(L, stackpos)->value.gc->uv;
}

static void pushproto(lua_State *L, Proto *proto)
{
	TValue o;
	o.tt = LUA_TPROTO;
	o.value.gc = (GCObject*)proto;
	luaA_pushobject(L, &o);
}

static void pushupval(lua_State *L, UpVal *upval)
{
	TValue o;
	o.tt = LUA_TUPVAL;
	o.value.gc = (GCObject*)upval;
	luaA_pushobject(L, &o);
}

static void pushclosure(lua_State *L, Closure *closure)
{
	TValue o;
	o.tt = LUA_TFUNCTION;
	o.value.gc = (GCObject*)closure;
	luaA_pushobject(L, &o);
}

static void persistfunction(PersistInfo *pi)
{
					/* perms reftbl ... func */
	Closure *cl = clvalue(getobject(pi->L, -1));
	if(cl->c.isC) {
		/* It's a C function. For now, we aren't going to allow
		 * persistence of C closures, even if the "C proto" is
		 * already in the permanents table. */
		lua_pushstring(pi->L, "Attempt to persist a C function");
		lua_error(pi->L);
	} else { 
		/* It's a Lua closure. */
		{
			/* We don't really _NEED_ the number of upvals,
			 * but it'll simplify things a bit */
			pi->writer(pi->L, &cl->l.p->nups, sizeof(lu_byte), pi->ud);
		}
		/* Persist prototype */
		{
			pushproto(pi->L, cl->l.p);
					/* perms reftbl ... func proto */
			persist(pi);
			lua_pop(pi->L, 1);
					/* perms reftbl ... func */
		}
		/* Persist upvalue values (not the upvalue objects
		 * themselves) */
		{
			int i;
			for(i=0; i<cl->l.p->nups; i++) {
					/* perms reftbl ... func */
				pushupval(pi->L, cl->l.upvals[i]);
					/* perms reftbl ... func upval */
				persist(pi);
				lua_pop(pi->L, 1);
					/* perms reftbl ... func */
			}	
					/* perms reftbl ... func */
		}
		/* Persist function environment */
		{
			lua_getfenv(pi->L, -1);
					/* perms reftbl ... func fenv */
			if(lua_equal(pi->L, -1, LUA_GLOBALSINDEX)) {
				/* Function has the default fenv */
					/* perms reftbl ... func _G */
				lua_pop(pi->L, 1);
					/* perms reftbl ... func */
				lua_pushnil(pi->L);
					/* perms reftbl ... func nil */
			}
					/* perms reftbl ... func fenv/nil */
			persist(pi);
			lua_pop(pi->L, 1);
					/* perms reftbl ... func */
		}
	}
}

static void persistupval(PersistInfo *pi)
{
					/* perms reftbl ... upval */
	UpVal *uv = toupval(pi->L, -1);

	/* If this assertion fails, the upvalue is not "closed". */
	lua_assert(uv->v == &uv->value);

	luaA_pushobject(pi->L, uv->v);
					/* perms reftbl ... upval obj */
	persist(pi);
	lua_pop(pi->L, 1);
					/* perms reftbl ... upval */
}

static void persistproto(PersistInfo *pi)
{
					/* perms reftbl ... proto */
	Proto *p = toproto(pi->L, -1);

	/* Persist constant refs */
	{
		int i;
		pi->writer(pi->L, &p->sizek, sizeof(int), pi->ud);
		for(i=0; i<p->sizek; i++) {
			luaA_pushobject(pi->L, &p->k[i]);
					/* perms reftbl ... proto const */
			persist(pi);
			lua_pop(pi->L, 1);
					/* perms reftbl ... proto */
		}
	}
					/* perms reftbl ... proto */

	/* serialize inner Proto refs */
	{
		int i;
		pi->writer(pi->L, &p->sizep, sizeof(int), pi->ud);
		for(i=0; i<p->sizep; i++)
		{
			pushproto(pi->L, p->p[i]);
					/* perms reftbl ... proto subproto */
			persist(pi);
			lua_pop(pi->L, 1);
					/* perms reftbl ... proto */
		}
	}
					/* perms reftbl ... proto */
	/* Serialize code */
	{
		pi->writer(pi->L, &p->sizecode, sizeof(int), pi->ud);
		pi->writer(pi->L, p->code, sizeof(Instruction) * p->sizecode, pi->ud);
	}
	/* Serialize misc values */
	{
		pi->writer(pi->L, &p->nups, sizeof(lu_byte), pi->ud);
		pi->writer(pi->L, &p->numparams, sizeof(lu_byte), pi->ud);
		pi->writer(pi->L, &p->is_vararg, sizeof(lu_byte), pi->ud);
		pi->writer(pi->L, &p->maxstacksize, sizeof(lu_byte), pi->ud);
	}
	/* We do not currently persist upvalue names, local variable names,
	 * variable lifetimes, line info, or source code. */
}

/* Copies a stack, but the stack is reversed in the process
 */
static int revappendstack(lua_State *from, lua_State *to) 
{
	StkId o;
	for(o=from->top-1; o>=from->stack; o--) {
		setobj2s(to, to->top, o);
		to->top++;
	}
	return (int)(from->top - from->stack);
}

/* Persist all stack members
 */
static void persistthread(PersistInfo *pi)
{
	int posremaining;
	lua_State *L2;
					/* perms reftbl ... thr */
	L2 = lua_tothread(pi->L, -1);
	if(pi->L == L2) {
		lua_pushstring(pi->L, "Can't persist currently running thread");
		lua_error(pi->L);
		return; /* not reached */
	}
	posremaining = revappendstack(L2, pi->L);
					/* perms reftbl ... thr (rev'ed contents of L2) */
	pi->writer(pi->L, &posremaining, sizeof(int), pi->ud);
	for(; posremaining > 0; posremaining--) {
		persist(pi);
		lua_pop(pi->L, 1);
	}
	/* Now, persist the CallInfo stack. */
	{
		int i, numframes = (int)((L2->ci - L2->base_ci) + 1);
		pi->writer(pi->L, &numframes, sizeof(int), pi->ud);
		for(i=0; i<numframes; i++) {
			CallInfo *ci = L2->base_ci + i;
			int stackbase = (int)(ci->base - L2->stack);
			int stackfunc = (int)(ci->func - L2->stack);
			int stacktop = (int)(ci->top - L2->stack);
			int pc = (ci != L2->base_ci) ? 
				(int)(ci->savedpc - ci_func(ci)->l.p->code) :
				0;
			pi->writer(pi->L, &stackbase, sizeof(int), pi->ud);
			pi->writer(pi->L, &stackfunc, sizeof(int), pi->ud);
			pi->writer(pi->L, &stacktop, sizeof(int), pi->ud);
			pi->writer(pi->L, &pc, sizeof(int), pi->ud);
			pi->writer(pi->L, &ci->nresults, sizeof(int), pi->ud);
		}
	}

	/* Finally, serialize the state's top and base */
	{
		int stackbase = (int)(L2->base - L2->stack);
		int stacktop = (int)(L2->top - L2->stack);
		pi->writer(pi->L, &stackbase, sizeof(int), pi->ud);
		pi->writer(pi->L, &stacktop, sizeof(int), pi->ud);
	}
}

static void persistboolean(PersistInfo *pi)
{
	int b = lua_toboolean(pi->L, -1);
	pi->writer(pi->L, &b, sizeof(int), pi->ud);
}

static void persistlightuserdata(PersistInfo *pi)
{
	void *p = lua_touserdata(pi->L, -1);
	pi->writer(pi->L, &p, sizeof(void *), pi->ud);
}

static void persistnumber(PersistInfo *pi)
{
	lua_Number n = lua_tonumber(pi->L, -1);
	pi->writer(pi->L, &n, sizeof(lua_Number), pi->ud);
}

static void persiststring(PersistInfo *pi)
{
	int length = (int)lua_strlen(pi->L, -1);
	pi->writer(pi->L, &length, sizeof(int), pi->ud);
	pi->writer(pi->L, lua_tostring(pi->L, -1), length, pi->ud);
}

/* Top-level delegating persist function
 */
static void persist(PersistInfo *pi)
{
					/* perms reftbl ... obj */
	/* If the object has already been written, write a reference to it */
	lua_pushvalue(pi->L, -1);
					/* perms reftbl ... obj obj */
	lua_rawget(pi->L, 2);
					/* perms reftbl ... obj ref? */
	if(!lua_isnil(pi->L, -1)) {
					/* perms reftbl ... obj ref */
		intptr_t zero = 0;
		intptr_t ref = (intptr_t)lua_touserdata(pi->L, -1);
		pi->writer(pi->L, &zero, sizeof(int), pi->ud);
		pi->writer(pi->L, &ref, sizeof(intptr_t), pi->ud);
		lua_pop(pi->L, 1);
					/* perms reftbl ... obj ref */
#ifdef PLUTO_DEBUG
		printindent(pi->level);
		printf("0 %d\n", ref);
#endif
		return;
	}
					/* perms reftbl ... obj nil */
	lua_pop(pi->L, 1);
					/* perms reftbl ... obj */
	/* If the object is nil, write the pseudoreference 0 */
	if(lua_isnil(pi->L, -1)) {
		int zero = 0;
		intptr_t zeroptr = 0;
		/* firsttime */
		pi->writer(pi->L, &zero, sizeof(int), pi->ud);
		/* ref */
		pi->writer(pi->L, &zeroptr, sizeof(intptr_t), pi->ud);
#ifdef PLUTO_DEBUG
		printindent(pi->level);
		printf("0 0\n");
#endif
		return;
	}
	{
		/* indicate that it's the first time */
		int one = 1;
		pi->writer(pi->L, &one, sizeof(int), pi->ud);
	}
	lua_pushvalue(pi->L, -1);
					/* perms reftbl ... obj obj */
	lua_pushlightuserdata(pi->L, (void*)(++(pi->counter)));
					/* perms reftbl ... obj obj ref */
	lua_rawset(pi->L, 2);
					/* perms reftbl ... obj */
	pi->writer(pi->L, &pi->counter, sizeof(intptr_t), pi->ud);

	/* At this point, we'll give the permanents table a chance to play. */
	{
		lua_pushvalue(pi->L, -1);
					/* perms reftbl ... obj obj */
		lua_gettable(pi->L, 1);
					/* perms reftbl ... obj permkey? */
		if(!lua_isnil(pi->L, -1)) {
					/* perms reftbl ... obj permkey */
#ifdef PLUTO_DEBUG
			printindent(pi->level);
			printf("1 %d PERM\n", pi->counter);
			pi->level++;
#endif
			int type = PLUTO_TPERMANENT;
			pi->writer(pi->L, &type, sizeof(int), pi->ud);
			persist(pi);
			lua_pop(pi->L, 1);
					/* perms reftbl ... obj */
#ifdef PLUTO_DEBUG
			pi->level--;
#endif
			return;
		} else {
					/* perms reftbl ... obj nil */
			lua_pop(pi->L, 1);
					/* perms reftbl ... obj */
		}
					/* perms reftbl ... obj */
	}
	{
		int type = lua_type(pi->L, -1);
		pi->writer(pi->L, &type, sizeof(int), pi->ud);

#ifdef PLUTO_DEBUG
		printindent(pi->level);
		printf("1 %d %d\n", pi->counter, type);
		pi->level++;
#endif
	}

	switch(lua_type(pi->L, -1)) {
		case LUA_TBOOLEAN:
			persistboolean(pi);
			break;
		case LUA_TLIGHTUSERDATA:
			persistlightuserdata(pi);
			break;
		case LUA_TNUMBER:
			persistnumber(pi);
			break;
		case LUA_TSTRING:
			persiststring(pi);
			break;
		case LUA_TTABLE:
			persisttable(pi);
			break;
		case LUA_TFUNCTION:
			persistfunction(pi);
			break;
		case LUA_TTHREAD:
			persistthread(pi);
			break;
		case LUA_TPROTO:
			persistproto(pi);
			break;
		case LUA_TUPVAL:
			persistupval(pi);
			break;
		case LUA_TUSERDATA:
			persistuserdata(pi);
			break;
		default:
			lua_assert(0);
	}
#ifdef PLUTO_DEBUG
	pi->level--;
#endif
}

void pluto_persist(lua_State *L, lua_Chunkwriter writer, void *ud)
{
	PersistInfo pi;
	
	pi.counter = 0;
	pi.L = L;
	pi.writer = writer;
	pi.ud = ud;
#ifdef PLUTO_DEBUG
	pi.level = 0;
#endif

					/* perms? rootobj? ...? */
	lua_assert(lua_gettop(L) == 2);
					/* perms rootobj */
	lua_assert(!lua_isnil(L, 2));
					/* perms rootobj */
	lua_newtable(L);
					/* perms rootobj reftbl */

	/* Now we're going to make the table weakly keyed. This prevents the
	 * GC from visiting it and trying to mark things it doesn't want to
	 * mark in tables, e.g. upvalues. All objects in the table are
	 * a priori reachable, so it doesn't matter that we do this. */
	lua_newtable(L);
					/* perms rootobj reftbl mt */
	lua_pushstring(L, "__mode");
					/* perms rootobj reftbl mt "__mode" */
	lua_pushstring(L, "k");
					/* perms rootobj reftbl mt "__mode" "k" */
	lua_settable(L, 4);
					/* perms rootobj reftbl mt */
	lua_setmetatable(L, 3);
					/* perms rootobj reftbl */
	lua_insert(L, 2);
					/* perms reftbl rootobj */
	persist(&pi);
					/* perms reftbl rootobj */
	lua_remove(L, 2);
					/* perms rootobj */
}

static int bufwriter (lua_State *L, const void* p, size_t sz, void* ud) {
	(void)L;
	luaL_addlstring((luaL_Buffer*)ud, (const char *)p, sz);
	return 1;
}

int persist_l(lua_State *L)
{
					/* perms? rootobj? ...? */
	luaL_Buffer buf;
	lua_settop(L, 2);
					/* perms? rootobj? */
	luaL_checktype(L, 1, LUA_TTABLE);
					/* perms rootobj? */
	luaL_checktype(L, 1, LUA_TTABLE);
					/* perms rootobj */
	
	luaL_buffinit(L, &buf);
	pluto_persist(L, bufwriter, &buf);

	lua_settop(L, 0);
					/* (empty) */
	luaL_pushresult(&buf);
					/* str */
	return 1;
}

typedef struct UnpersistInfo_t {
	lua_State *L;
	ZIO zio;
#ifdef PLUTO_DEBUG
	int level;
#endif
} UnpersistInfo;

static void unpersist(UnpersistInfo *upi);

/* The object is left on the stack. This is primarily used by unpersist, but
 * may be used by GCed objects that may incur cycles in order to preregister
 * the object. */
static void registerobject(intptr_t ref, UnpersistInfo *upi)
{
					/* perms reftbl ... obj */
	lua_pushlightuserdata(upi->L, (void*)ref);
					/* perms reftbl ... obj ref */
	lua_pushvalue(upi->L, -2);
					/* perms reftbl ... obj ref obj */
	lua_settable(upi->L, 2);
					/* perms reftbl ... obj */
}

static void unpersistboolean(UnpersistInfo *upi)
{
					/* perms reftbl ... */
	int b;
	verify(luaZ_read(&upi->zio, &b, sizeof(int)) == 0);
	lua_pushboolean(upi->L, b);
					/* perms reftbl ... bool */
}

static void unpersistlightuserdata(UnpersistInfo *upi)
{
					/* perms reftbl ... */
	void *p;
	verify(luaZ_read(&upi->zio, &p, sizeof(void *)) == 0);
	lua_pushlightuserdata(upi->L, p);
					/* perms reftbl ... ludata */
}

static void unpersistnumber(UnpersistInfo *upi)
{
					/* perms reftbl ... */
	lua_Number n;
	verify(luaZ_read(&upi->zio, &n, sizeof(lua_Number)) == 0);
	lua_pushnumber(upi->L, n);
					/* perms reftbl ... num */
}

static void unpersiststring(UnpersistInfo *upi)
{
					/* perms reftbl sptbl ref */
	int length;
	char* string;
	verify(luaZ_read(&upi->zio, &length, sizeof(int)) == 0);
	string = (char*)luaM_malloc(upi->L, length);
	verify(luaZ_read(&upi->zio, string, length) == 0);
	lua_pushlstring(upi->L, string, length);
					/* perms reftbl sptbl ref str */
	luaM_free(upi->L, string/*, length*/);
}

static void unpersistspecialtable(int ref, UnpersistInfo *upi)
{
	(void)ref;
					/* perms reftbl ... */
	unpersist(upi);
					/* perms reftbl ... spfunc? */
	lua_assert(lua_isfunction(upi->L, -1));
					/* perms reftbl ... spfunc */
	lua_call(upi->L, 0, 1);
					/* perms reftbl ... tbl? */
	lua_assert(lua_istable(upi->L, -1));
					/* perms reftbl ... tbl */
}

static void unpersistliteraltable(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	/* Preregister table for handling of cycles */
	lua_newtable(upi->L);
					/* perms reftbl ... tbl */
	registerobject(ref, upi);
					/* perms reftbl ... tbl */
	/* Unpersist metatable */
	{
		unpersist(upi);
					/* perms reftbl ... tbl mt/nil? */
		if(lua_istable(upi->L, -1)) {
					/* perms reftbl ... tbl mt */
			lua_setmetatable(upi->L, -2);
					/* perms reftbl ... tbl */
		} else {
					/* perms reftbl ... tbl nil? */
			lua_assert(lua_isnil(upi->L, -1));
					/* perms reftbl ... tbl nil */
			lua_pop(upi->L, 1);
					/* perms reftbl ... tbl */
		}
					/* perms reftbl ... tbl */
	}

	while(1)
	{
					/* perms reftbl ... tbl */
		unpersist(upi);
					/* perms reftbl ... tbl key/nil */
		if(lua_isnil(upi->L, -1)) {
					/* perms reftbl ... tbl nil */
			lua_pop(upi->L, 1);
					/* perms reftbl ... tbl */
			break;
		}
					/* perms reftbl ... tbl key */
		unpersist(upi);
					/* perms reftbl ... tbl key value? */
		lua_assert(!lua_isnil(upi->L, -1));
					/* perms reftbl ... tbl key value */
		lua_settable(upi->L, -3);
					/* perms reftbl ... tbl */
	}
}

static void unpersisttable(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	{
		int isspecial;
		verify(luaZ_read(&upi->zio, &isspecial, sizeof(int)) == 0);
		if(isspecial) {
			unpersistspecialtable(ref, upi);
					/* perms reftbl ... tbl */
		} else {
			unpersistliteraltable(ref, upi);
					/* perms reftbl ... tbl */
		}
					/* perms reftbl ... tbl */
	}
}

static UpVal *makeupval(lua_State *L, int stackpos)
{
	UpVal *uv = luaM_new(L, UpVal);
	uv->tt = LUA_TUPVAL;
	uv->v = &uv->u.value;
	setobj(L, uv->v, getobject(L, stackpos));
	luaC_link(L, (GCObject*)uv, LUA_TUPVAL);
	return uv;
}

static Proto *makefakeproto(lua_State *L, lu_byte nups)
{
	Proto *p = luaF_newproto(L);
	p->sizelineinfo = 1;
	p->lineinfo = luaM_newvector(L, 1, int);
	p->lineinfo[0] = 1;
	p->sizecode = 1;
	p->code = luaM_newvector(L, 1, Instruction);
	p->code[0] = CREATE_ABC(OP_RETURN, 0, 1, 0);
	p->source = luaS_newlstr(L, "", 0);
	p->maxstacksize = 2;
	p->nups = nups;

	return p;
}

/* The GC is not fond of finding upvalues in tables. We get around this
 * during persistence using a weakly keyed table, so that the GC doesn't
 * bother to mark them. This won't work in unpersisting, however, since
 * if we make the values weak they'll be collected (since nothing else
 * references them). Our solution, during unpersisting, is to represent
 * upvalues as dummy functions, each with one upvalue. */
static void boxupval(lua_State *L)
{
					/* ... upval */
	LClosure *lcl;
	UpVal *uv;

	uv = toupval(L, -1);
	lua_pop(L, 1);
					/* ... */
	lcl = (LClosure*)luaF_newLclosure(L, 1, &L->l_gt.value.gc->h);
	pushclosure(L, (Closure*)lcl);
					/* ... func */
	lcl->p = makefakeproto(L, 1);
	lcl->upvals[0] = uv;
}

static void unboxupval(lua_State *L)
{
					/* ... func */
	LClosure *lcl;
	UpVal *uv;

	lcl = (LClosure*)clvalue(getobject(L, -1));
	uv = lcl->upvals[0];
	lua_pop(L, 1);
					/* ... */
	pushupval(L, uv);
					/* ... upval */
}

static void unpersistfunction(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	LClosure *lcl;
	int i;
	lu_byte nupvalues;

	verify(luaZ_read(&upi->zio, &nupvalues, sizeof(lu_byte)) == 0);

	lcl = (LClosure*)luaF_newLclosure(upi->L, nupvalues, &upi->L->l_gt.value.gc->h);
	pushclosure(upi->L, (Closure*)lcl);

					/* perms reftbl ... func */
	/* Put *some* proto in the closure, before the GC can find it */
	lcl->p = makefakeproto(upi->L, nupvalues);

	/* Also, we need to temporarily fill the upvalues */
	lua_pushnil(upi->L);
					/* perms reftbl ... func nil */
	for(i=0; i<nupvalues; i++) {
		lcl->upvals[i] = makeupval(upi->L, -1);
	}
	lua_pop(upi->L, 1);
					/* perms reftbl ... func */

	/* I can't see offhand how a function would ever get to be self-
	 * referential, but just in case let's register it early */
	registerobject(ref, upi);

	/* Now that it's safe, we can get the real proto */
	unpersist(upi);
					/* perms reftbl ... func proto? */
	lua_assert(lua_type(upi->L, -1) == LUA_TPROTO);
					/* perms reftbl ... func proto */
	lcl->p = toproto(upi->L, -1);
	lua_pop(upi->L, 1);
					/* perms reftbl ... func */

	for(i=0; i<nupvalues; i++) {
					/* perms reftbl ... func */
		unpersist(upi);
					/* perms reftbl ... func func2 */
		unboxupval(upi->L);
					/* perms reftbl ... func upval */
		lcl->upvals[i] = toupval(upi->L, -1);
		lua_pop(upi->L, 1);
					/* perms reftbl ... func */
	}
					/* perms reftbl ... func */

	/* Finally, the fenv */
	unpersist(upi);
					/* perms reftbl ... func fenv/nil? */
	lua_assert(lua_type(upi->L, -1) == LUA_TNIL ||
		lua_type(upi->L, -1) == LUA_TTABLE);
					/* perms reftbl ... func fenv/nil */
	if(!lua_isnil(upi->L, -1)) {
					/* perms reftbl ... func fenv */
		lua_setfenv(upi->L, -2);
					/* perms reftbl ... func */
	} else {
					/* perms reftbl ... func nil */
		lua_pop(upi->L, 1);
					/* perms reftbl ... func */
	}
					/* perms reftbl ... func */
}

static void unpersistupval(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	UpVal *uv;
	(void)ref;

	unpersist(upi);
					/* perms reftbl ... obj */
	uv = makeupval(upi->L, -1);
	lua_pop(upi->L, 1);
					/* perms reftbl ... */
	pushupval(upi->L, uv);
					/* perms reftbl ... upval */
	boxupval(upi->L);
					/* perms reftbl ... func */
}
	
static void unpersistproto(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	Proto *p;
	int i;

	/* We have to be careful. The GC expects a lot out of protos. In
	 * particular, we need to give the function a valid string for its
	 * source, and valid code, even before we actually read in the real
	 * code. */
	TString *source = luaS_newlstr(upi->L, "", 0);
	p = luaF_newproto(upi->L);
	p->source = source;
	p->sizecode=1;
	p->code = luaM_newvector(upi->L, 1, Instruction);
	p->code[0] = CREATE_ABC(OP_RETURN, 0, 1, 0);
	p->maxstacksize = 2;
	
	(void)ref;

	pushproto(upi->L, p);
					/* perms reftbl ... proto */
	/* We don't need to register early, since protos can never ever be
	 * involved in cyclic references */

	/* Read in constant references */
	{
		verify(luaZ_read(&upi->zio, &p->sizek, sizeof(int)) == 0);
		luaM_reallocvector(upi->L, p->k, 0, p->sizek, TValue);
		for(i=0; i<p->sizek; i++) {
					/* perms reftbl ... proto */
			unpersist(upi);
					/* perms reftbl ... proto k */
			setobj2s(upi->L, &p->k[i], getobject(upi->L, -1));
			lua_pop(upi->L, 1);
					/* perms reftbl ... proto */
		}
					/* perms reftbl ... proto */
	}
	/* Read in sub-proto references */
	{
		verify(luaZ_read(&upi->zio, &p->sizep, sizeof(int)) == 0);
		luaM_reallocvector(upi->L, p->p, 0, p->sizep, Proto*);
		for(i=0; i<p->sizep; i++) {
					/* perms reftbl ... proto */
			unpersist(upi);
					/* perms reftbl ... proto subproto */
			p->p[i] = toproto(upi->L, -1);
			lua_pop(upi->L, 1);
					/* perms reftbl ... proto */
		}
					/* perms reftbl ... proto */
	}

	/* Read in code */
	{
		verify(luaZ_read(&upi->zio, &p->sizecode, sizeof(int)) == 0);
		luaM_reallocvector(upi->L, p->code, 1, p->sizecode, Instruction);
		verify(luaZ_read(&upi->zio, p->code, 
			sizeof(Instruction) * p->sizecode) == 0);
	}

	/* Read in misc values */
	{
		verify(luaZ_read(&upi->zio, &p->nups, sizeof(lu_byte)) == 0);
		verify(luaZ_read(&upi->zio, &p->numparams, sizeof(lu_byte)) == 0);
		verify(luaZ_read(&upi->zio, &p->is_vararg, sizeof(lu_byte)) == 0);
		verify(luaZ_read(&upi->zio, &p->maxstacksize, sizeof(lu_byte)) == 0);
	}
}

static void unpersistthread(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	lua_State *L2;
	L2 = lua_newthread(upi->L);
					/* L1: perms reftbl ... thr */
					/* L2: (empty) */
	registerobject(ref, upi);

	/* First, deserialize the object stack. */
	{
		int i, stacksize;
		verify(luaZ_read(&upi->zio, &stacksize, sizeof(int)) == 0);
		luaD_growstack(L2, stacksize);
		/* Make sure that the first stack element (a nil, representing
		 * the imaginary top-level C function) is written to the very,
		 * very bottom of the stack */
		L2->top--;
		for(i=0; i<stacksize; i++) {
			unpersist(upi);
					/* L1: perms reftbl ... thr obj* */
		}
		lua_xmove(upi->L, L2, stacksize);
					/* L1: perms reftbl ... thr */
					/* L2: obj* */
	}

	/* Now, deserialize the CallInfo stack. */
	{
		int i, numframes;
		verify(luaZ_read(&upi->zio, &numframes, sizeof(int)) == 0);
		luaD_reallocCI(L2,numframes*2);
		for(i=0; i<numframes; i++) {
			CallInfo *ci = L2->base_ci + i;
			int stackbase, stackfunc, stacktop, pc;
			verify(luaZ_read(&upi->zio, &stackbase, sizeof(int)) == 0);
			verify(luaZ_read(&upi->zio, &stackfunc, sizeof(int)) == 0);
			verify(luaZ_read(&upi->zio, &stacktop, sizeof(int)) == 0);
			verify(luaZ_read(&upi->zio, &pc, sizeof(int)) == 0);
			verify(luaZ_read(&upi->zio, &(ci->nresults), sizeof(int)) == 0);

			ci->base = L2->stack+stackbase;
			ci->func = L2->stack+stackfunc;
			ci->top = L2->stack+stacktop;
			/*if(!(ci->state & CI_C)) {*/
				ci->savedpc = ci_func(ci)->l.p->code + pc;
			/*}*/
			ci->tailcalls = 0;
			/* Update the pointer each time, to keep the GC 
			 * happy*/
			L2->ci = ci; 
		}
	}
	{
		int stackbase, stacktop;
		verify(luaZ_read(&upi->zio, &stackbase, sizeof(int)) == 0);
		verify(luaZ_read(&upi->zio, &stacktop, sizeof(int)) == 0);
		L2->base = L2->stack + stackbase;
		L2->top = L2->stack + stacktop;
	}
}

static void unpersistuserdata(int ref, UnpersistInfo *upi)
{
					/* perms reftbl ... */
	int isspecial;
	verify(luaZ_read(&upi->zio, &isspecial, sizeof(int)) == 0);
	if(isspecial) {
		unpersist(upi);
					/* perms reftbl ... spfunc? */
		lua_assert(lua_isfunction(upi->L, -1));
					/* perms reftbl ... spfunc */
#ifdef PLUTO_PASS_USERDATA_TO_PERSIST
		lua_pushlightuserdata(upi->L, &upi->zio);
		lua_call(upi->L, 1, 1);
#else
		lua_call(upi->L, 0, 1);
#endif
					/* perms reftbl ... udata? */
/* This assertion might not be necessary; it's conceivable, for
 * example, that the SP function might decide to return a table
 * with equivalent functionality. For the time being, we'll
 * ignore this possibility in favor of stricter and more testable
 * requirements. */
		lua_assert(lua_isuserdata(upi->L, -1));
					/* perms reftbl ... udata */
	} else {
		int length;
		verify(luaZ_read(&upi->zio, &length, sizeof(int)) == 0);

		lua_newuserdata(upi->L, length);
					/* perms reftbl ... udata */
		registerobject(ref, upi);
		verify(luaZ_read(&upi->zio, lua_touserdata(upi->L, -1), length) == 0);

		unpersist(upi);
					/* perms reftbl ... udata mt/nil? */
		lua_assert(lua_istable(upi->L, -1) || lua_isnil(upi->L, -1));
					/* perms reftbl ... udata mt/nil */
		lua_setmetatable(upi->L, -2);
					/* perms reftbl ... udata */
	}
					/* perms reftbl ... udata */
}

static void unpersistpermanent(int ref, UnpersistInfo *upi)
{
	(void)ref;
					/* perms reftbl ... */
	unpersist(upi);
					/* perms reftbl permkey */
	lua_gettable(upi->L, 1);
					/* perms reftbl perm? */
	/* We assume currently that the substituted permanent value
	 * shouldn't be nil. This may be a bad assumption. Real-life
	 * experience is needed to evaluate this. */
	lua_assert(!lua_isnil(upi->L, -1));
					/* perms reftbl perm */
}

/* For debugging only; not called when lua_assert is empty */
#ifdef DEBUG
static int inreftable(lua_State *L, intptr_t ref)
{
	int res;
					/* perms reftbl ... */
	lua_pushlightuserdata(L, (void*)ref);
					/* perms reftbl ... ref */
	lua_gettable(L, 2);
					/* perms reftbl ... obj? */
	res = !lua_isnil(L, -1);
	lua_pop(L, 1);
					/* perms reftbl ... */
	return res;
}
#endif

static void unpersist(UnpersistInfo *upi)
{
					/* perms reftbl ... */
	int firstTime;
	int stacksize = lua_gettop(upi->L); /* DEBUG */
	(void)stacksize;
	luaZ_read(&upi->zio, &firstTime, sizeof(int));
	if(firstTime) {
		int ref;
		int type;
		luaZ_read(&upi->zio, &ref, sizeof(int));
		lua_assert(!inreftable(upi->L, ref));
		luaZ_read(&upi->zio, &type, sizeof(int));
#ifdef PLUTO_DEBUG
		printindent(upi->level);
		printf("1 %d %d\n", ref, type);
		upi->level++;
#endif
		switch(type) {
		case LUA_TBOOLEAN:
			unpersistboolean(upi);
			break;
		case LUA_TLIGHTUSERDATA:
			unpersistlightuserdata(upi);
			break;
		case LUA_TNUMBER:
			unpersistnumber(upi);
			break;
		case LUA_TSTRING:
			unpersiststring(upi);
			break;
		case LUA_TTABLE:
			unpersisttable(ref, upi);
			break;
		case LUA_TFUNCTION:
			unpersistfunction(ref, upi);
			break;
		case LUA_TTHREAD:
			unpersistthread(ref, upi);
			break;
		case LUA_TPROTO:
			unpersistproto(ref, upi);
			break;
		case LUA_TUPVAL:
			unpersistupval(ref, upi);
			break;
		case LUA_TUSERDATA:
			unpersistuserdata(ref, upi);
			break;
		case PLUTO_TPERMANENT:
			unpersistpermanent(ref, upi);
			break;
		default:
			lua_assert(0);
		}
					/* perms reftbl ... obj */
		lua_assert(lua_type(upi->L, -1) == type || 
			type == PLUTO_TPERMANENT ||
			/* Remember, upvalues get a special dispensation, as
			 * described in boxupval */
			(lua_type(upi->L, -1) == LUA_TFUNCTION && 
				type == LUA_TUPVAL));
		registerobject(ref, upi);
					/* perms reftbl ... obj */
#ifdef PLUTO_DEBUG
		upi->level--;
#endif
	} else {
		intptr_t ref;
		luaZ_read(&upi->zio, &ref, sizeof(int));
#ifdef PLUTO_DEBUG
		printindent(upi->level);
		printf("0 %d\n", ref);
#endif
		if(ref == 0) {
			lua_pushnil(upi->L);
					/* perms reftbl ... nil */
		} else {
			lua_pushlightuserdata(upi->L, (void*)ref);
					/* perms reftbl ... ref */
			lua_gettable(upi->L, 2);
					/* perms reftbl ... obj? */
			lua_assert(!lua_isnil(upi->L, -1));
		}
					/* perms reftbl ... obj/nil */
	}
					/* perms reftbl ... obj/nil */
	lua_assert(lua_gettop(upi->L) == stacksize + 1);
}

void pluto_unpersist(lua_State *L, lua_Chunkreader reader, void *ud)
{
	/* We use the graciously provided ZIO (what the heck does the Z stand
	 * for?) library so that we don't have to deal with the reader directly.
	 * Letting the reader function decide how much data to return can be
	 * very unpleasant.
	 */
	UnpersistInfo upi;
	upi.L = L;
#ifdef PLUTO_DEBUG
	upi.level = 0;
#endif

	luaZ_init(L, &upi.zio, reader, ud/*, ""*/);

					/* perms */
	lua_newtable(L);
					/* perms reftbl */
	unpersist(&upi);
					/* perms reftbl rootobj */
	lua_replace(L, 2);
					/* perms rootobj  */
}

typedef struct LoadInfo_t {
  const char *buf;
  size_t size;
} LoadInfo;


static const char *bufreader(lua_State *L, void *ud, size_t *sz) {
	LoadInfo *li = (LoadInfo *)ud;
	(void)L;
	if(li->size == 0) {
		return NULL;
	}
	*sz = li->size;
	li->size = 0;
	return li->buf;
}

int unpersist_l(lua_State *L)
{
	LoadInfo li;
					/* perms? str? ...? */
	lua_settop(L, 2);
					/* perms? str? */
	li.buf = luaL_checklstring(L, 2, &li.size);
					/* perms? str */
	lua_pop(L, 1);
	/* It is conceivable that the buffer might now be collectable,
	 * which would cause problems in the reader. I can't think of
	 * any situation where there would be no other reference to the
	 * buffer, so for now I'll leave it alone, but this is a potential
	 * bug. */
					/* perms? */
	luaL_checktype(L, 1, LUA_TTABLE);
					/* perms */
	pluto_unpersist(L, bufreader, &li);
					/* perms rootobj */
	return 1;
}

static luaL_reg pluto_reg[] = {
	{ "persist", persist_l },
	{ "unpersist", unpersist_l },
	{ NULL, NULL }
};

int pluto_open(lua_State *L) {
	luaL_openlib(L, "pluto", pluto_reg, 0);
	return 1;
}
