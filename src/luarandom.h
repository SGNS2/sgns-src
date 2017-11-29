#ifndef LUARANDOM_H
#define LUARANDOM_H

#include <random>

struct lua_State;

#define LUARANDOM_META "Random"

class LuaRandom {
public:
	static int l_reseed(lua_State *L);
	static int l_rand01(lua_State *L);
	static int l_int(lua_State *L);
	static int l_real(lua_State *L);
	static int l_bernoulli(lua_State *L);
	static int l_binomial(lua_State *L);
	static int l_nbinomial(lua_State *L);
	static int l_geometric(lua_State *L);
	static int l_poisson(lua_State *L);
	static int l_exponential(lua_State *L);
	static int l_gamma(lua_State *L);
	static int l_weibull(lua_State *L);
	static int l_exval(lua_State *L);
	static int l_normal(lua_State *L);
	static int l_lognormal(lua_State *L);
	static int l_chisq(lua_State *L);
	static int l_cauchy(lua_State *L);
	static int l_f(lua_State *L);
	static int l_t(lua_State *L);

	static int l_create(lua_State *L);
	static int l_gc(lua_State *L);

	static void init_lua(lua_State *L);

	std::mt19937_64 *getRNG() { return &rng; }

private:
	LuaRandom();
	LuaRandom(unsigned long seed);

	std::mt19937_64 rng;
};

#endif // LUARANDOM_H
