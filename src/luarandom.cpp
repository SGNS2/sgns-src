
#include "stdafx.h"

#include <random>
#include <limits>

#include "luarandom.h"
#include <lua.hpp>

static unsigned long randomSeed() {
	static std::random_device rd;
	return rd();
}

LuaRandom::LuaRandom()
	: rng(randomSeed()) { }

LuaRandom::LuaRandom(unsigned long seed)
	: rng(seed) { }

int LuaRandom::l_reseed(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	unsigned long newSeed = randomSeed();
	lr->rng.seed(newSeed);
	lua_pushnumber(L, lua_Number(newSeed));
	return 1;
}

int LuaRandom::l_rand01(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::uniform_real_distribution<lua_Number> distr(0.0, 1.0);
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_int(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Integer lb = 1, ub = luaL_checkint(L, 2);
	if (lua_isnumber(L, 3)) {
		lb = ub;
		ub = lua_tointeger(L, 3);
	}
	std::uniform_int_distribution<lua_Integer> distr(lb, ub);
	lua_pushinteger(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_real(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Number lb = 0.0, ub = 1.0;
	if (lua_isnumber(L, 2)) {
		ub = lua_tonumber(L, 2);
		if (lua_isnumber(L, 3)) {
			lb = ub;
			ub = lua_tonumber(L, 3);
		}
	}
	std::uniform_real_distribution<lua_Number> distr(lb, ub);
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_bernoulli(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::bernoulli_distribution distr(luaL_optnumber(L, 2, 0.5));
	lua_pushboolean(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_binomial(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Integer N = luaL_checkinteger(L, 2);
	std::binomial_distribution<lua_Integer> distr(N, luaL_optnumber(L, 3, 0.5));
	lua_pushinteger(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_nbinomial(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Integer R = luaL_checkinteger(L, 2);
	std::negative_binomial_distribution<lua_Integer> distr(R, luaL_checknumber(L, 3));
	lua_pushinteger(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_geometric(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::geometric_distribution<lua_Integer> distr(luaL_checknumber(L, 2));
	lua_pushinteger(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_poisson(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::poisson_distribution<lua_Integer> distr(luaL_checknumber(L, 2));
	lua_pushinteger(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_exponential(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::exponential_distribution<lua_Number> distr(luaL_checknumber(L, 2));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_gamma(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::gamma_distribution<lua_Number> distr(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_weibull(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::weibull_distribution<lua_Number> distr(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_exval(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::extreme_value_distribution<lua_Number> distr(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_normal(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Number mu = 0.0, sigma = 1.0;
	if (lua_isnumber(L, 2)) {
		sigma = lua_tonumber(L, 2);
		if (lua_isnumber(L, 3)) {
			mu = sigma;
			sigma = lua_tonumber(L, 3);
		}
	}
	std::normal_distribution<lua_Number> distr(mu, sigma);
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_lognormal(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Number mu = 0.0, sigma = 1.0;
	if (lua_isnumber(L, 2)) {
		sigma = lua_tonumber(L, 2);
		if (lua_isnumber(L, 3)) {
			mu = sigma;
			sigma = lua_tonumber(L, 3);
		}
	}
	std::lognormal_distribution<lua_Number> distr(mu, sigma);
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_chisq(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::chi_squared_distribution<lua_Number> distr(luaL_checknumber(L, 2));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_cauchy(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lua_Number location = 0.0, scale = 1.0;
	if (lua_isnumber(L, 2)) {
		scale = lua_tonumber(L, 2);
		if (lua_isnumber(L, 3)) {
			location = scale;
			scale = lua_tonumber(L, 3);
		}
	}
	std::cauchy_distribution<lua_Number> distr(location, scale);
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_f(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::fisher_f_distribution<lua_Number> distr(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_t(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	std::student_t_distribution<lua_Number> distr(luaL_checknumber(L, 2));
	lua_pushnumber(L, distr(lr->rng));
	return 1;
}

int LuaRandom::l_create(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(lua_newuserdata(L, sizeof(LuaRandom)));
	if (lua_isnumber(L, 1)) {
		new(lr) LuaRandom(unsigned long(luaL_checkinteger(L, 1)));
	} else {
		new(lr) LuaRandom();
	}

	luaL_newmetatable(L, LUARANDOM_META);
	lua_setmetatable(L, -2);
	return 1;
}

int LuaRandom::l_gc(lua_State *L) {
	LuaRandom *lr = static_cast<LuaRandom*>(luaL_checkudata(L, 1, LUARANDOM_META));
	lr->LuaRandom::~LuaRandom();
	return 0;
}


static luaL_Reg LuaRandom_Functions[] = {
	{ "reseed", &LuaRandom::l_reseed },
	{ "int", &LuaRandom::l_int },
	{ "uniform", &LuaRandom::l_real },
	{ "bernoulli", &LuaRandom::l_bernoulli },
	{ "binomial", &LuaRandom::l_binomial },
	{ "nbinomial", &LuaRandom::l_nbinomial },
	{ "geometric", &LuaRandom::l_geometric },
	{ "poisson", &LuaRandom::l_poisson },
	{ "exponential", &LuaRandom::l_exponential },
	{ "gamma", &LuaRandom::l_gamma },
	{ "weibull", &LuaRandom::l_weibull },
	{ "extremevalue", &LuaRandom::l_exval },
	{ "normal", &LuaRandom::l_normal },
	{ "lognormal", &LuaRandom::l_lognormal },
	{ "chisquared", &LuaRandom::l_chisq },
	{ "cauchy", &LuaRandom::l_cauchy },
	{ "f", &LuaRandom::l_f },
	{ "t", &LuaRandom::l_t },
	{ "__call", &LuaRandom::l_rand01 },
	{ "__gc", &LuaRandom::l_gc },
	{ 0, 0 }
};

void LuaRandom::init_lua(lua_State *L) {
	luaL_newmetatable(L, LUARANDOM_META);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, LuaRandom_Functions);
	lua_pop(L, 1);

	lua_pushcfunction(L, &LuaRandom::l_create);
	lua_setglobal(L, "RNG");
}
