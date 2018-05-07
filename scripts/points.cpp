#include <iostream>
#include <tuple>
#include <vector>
#include "shared.hpp"

using V = uint8_t_vec;

// ref https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c#L2160
//   iteratively verifies that (d + ...)G == (dG + ...G)
template <typename A, typename B, typename C, typename D>
void test_ec_combine (B& pa, C& pas, D& pfs) {
	bool ok = true;
	auto sum = ONE;
	auto sumQ = _pointFromScalar<A>(sum, ok);
	assert(ok);

	for (int i = 1; i <= 10; i++) {
		const auto d = randomPrivate();
		const auto Q = _pointFromScalar<A>(d, ok);
		assert(ok);

		// dG + ...G
		const auto V = _pointAdd<A>(sumQ, Q, ok);
		assert(ok);

		// (d + ...)G
		const auto U = _pointAddScalar<A>(sumQ, d, ok);
		assert(ok);
		assert(V == U);

		// (d + ...)G
		sum = _privAdd(sum, d, ok);
		assert(ok);

		const auto R = _pointFromScalar<A>(sum, ok);
		assert(ok);
		assert(V == R);

		pa.push_back({ sumQ, Q, V });
		pas.push_back({ sumQ, d, V });
		pfs.push_back({ sum, V });

		sumQ = V;
	}
}

struct IP { V a; bool e; std::string desc = ""; };
struct PA { V a; V b; V e; std::string except = ""; std::string desc = ""; };
struct PAS { V a; uint8_t_32 b; V e; std::string except = ""; std::string desc = ""; };
struct PC { V a; bool b; V e; std::string except = ""; std::string desc = ""; };
struct PFS { uint8_t_32 a; V e; std::string except = ""; std::string desc = ""; };

auto generate () {
	using A = uint8_t_33;

	bool ok = true;
	const auto G_LESS_1 = _pointFromScalar<A>(GROUP_ORDER_LESS_1, ok);
	const auto G_LESS_2 = _pointFromScalar<A>(GROUP_ORDER_LESS_2, ok);
	const auto G_LESS_3 = _pointFromScalar<A>(GROUP_ORDER_LESS_3, ok);
	const auto G_ONE = _pointFromUInt32<A>(1, ok);
	const auto G_TWO = _pointFromUInt32<A>(2, ok);
	const auto G_THREE = _pointFromUInt32<A>(3, ok);
	assert(ok);
	const auto NULLQ = vectorify(Null<A>());
	const auto BAD_POINTS_C = generateBadPoints<uint8_t_33>();
	const auto BAD_POINTS = generateBadPoints<uint8_t_65>();

	///////////////////////////////// isPoint
	std::vector<IP> ip = {
		{ G, true },
		{ G_ONE, true },
		{ G_TWO, true },
		{ G_THREE, true },
		{ _pointFromX(P_LESS_1), true }
	};
	const auto _ip = ip; // prevent trashing ip while adding
	for (auto& x : _ip) ip.push_back({ _pointFlip(x.a), x.e, x.desc });
	for (const auto x : BAD_POINTS) ip.push_back({ x.a, false, x.desc });
	for (const auto x : BAD_POINTS_C) ip.push_back({ x.a, false, x.desc });

	// fuzz
	for (size_t i = 0; i < 1000; ++i) {
		ip.push_back({ _pointFromScalar<uint8_t_33>(randomPrivate(), ok), true }); assert(ok);
	}

	for (size_t i = 0; i < 1000; ++i) {
		ip.push_back({ _pointFromScalar<uint8_t_65>(randomPrivate(), ok), true });
	}
	assert(ok);

	///////////////////////////////// pointAdd
	// XXX: only compressed point fixtures, flip for each combination when testing

	std::vector<PA> pa = {
		{ G_LESS_1, G_LESS_1, G_LESS_2 },
		{ G_LESS_1, G_LESS_2, G_LESS_3 },
		{ G_LESS_1, G_LESS_2, G_LESS_3 },

		// https://github.com/bitcoin-core/secp256k1/blob/452d8e4d2a2f9f1b5be6b02e18f1ba102e5ca0b4/src/tests.c#L3857
		{ G_ONE, G_LESS_1, NULLQ }, // == 0/infinity
		{ G_ONE, G_LESS_2, G_LESS_1 }, // == -1
		{ G_TWO, G_LESS_1, G_ONE }, // == 1
		{ G_ONE, G, NULLQ },
		{ G_ONE, G_ONE, G_TWO },
		{ G_ONE, G_TWO, G_THREE }
	};

	// fuzz
	for (size_t i = 0; i < 100; ++i) {
		const auto a = _pointFromScalar<A>(randomPrivate(), ok); assert(ok);
		const auto b = _pointFromScalar<A>(randomPrivate(), ok); assert(ok);
		const auto e = _pointAdd<A>(a, b, ok);

		pa.push_back({ a, b, e });
	}

	///////////////////////////////// pointAddScalar
	// XXX: only compressed point fixtures, flip for each combination when testing

	std::vector<PAS> pas = {
		{ G_LESS_1, ZERO, G_LESS_1 }, // #L3719, -1 + 0 == -1
		{ G_LESS_1, ONE, NULLQ, "", "Adds to infinity" },
		{ G_LESS_1, TWO, G_ONE },
		{ G_LESS_1, THREE, G_TWO },
		{ G_LESS_1, GROUP_ORDER_LESS_1, G_LESS_2 },
		{ G_LESS_1, GROUP_ORDER_LESS_2, G_LESS_3 },
		{ G_LESS_1, GROUP_ORDER_LESS_2, G_LESS_3 },
		{ G_LESS_2, ONE, G_LESS_1 },
		{ G_LESS_2, TWO, NULLQ, "", "Adds to infinity" },
		{ G_LESS_2, THREE, G_ONE },
		{ G_ONE, GROUP_ORDER_LESS_1, NULLQ, "", "Adds to infinity" },
		{ G_ONE, GROUP_ORDER_LESS_2, G_LESS_1, "", "== G - 1" }, // == -1
		{ G_TWO, GROUP_ORDER_LESS_1, G_ONE, "", "== 1" } // == 1
	};

	for (uint32_t i = 1; i < 5; ++i) {
		bool ok = true;
		const auto G_i = _pointFromUInt32<A>(i, ok); assert(ok);
		const auto G_i_p1 = _pointFromUInt32<A>(i + 1, ok); assert(ok);

		pas.push_back({ G_i, ONE, G_i_p1 });
	}

	///////////////////////////////// pointCompress

	std::vector<PC> pc = {
		{ G, false, G },
		{ G, true, GC },
		{ GC, false, G },
		{ GC, true, GC }
	};

	for (auto i = 1; i < 10; ++i) {
		const auto iic = vectorify(_pointFromUInt32<uint8_t_33>(i, ok)); assert(ok);
		const auto ii = vectorify(_pointFromUInt32<uint8_t_65>(i, ok)); assert(ok);

		pc.push_back({ iic, true, iic });
		pc.push_back({ iic, false, ii });
		pc.push_back({ ii, true, iic });
		pc.push_back({ ii, false, ii });
	}

	// fuzz
	for (size_t i = 0; i < 50; ++i) {
		const auto ii = _pointFromScalar<uint8_t_65>(randomPrivate(), ok);
		assert(ok);

		uint8_t_32 iix;
		std::copy(ii.begin() + 1, ii.begin() + 33, iix.begin());
		const auto iic = _pointFromX(iix);

		pc.push_back({ iic, true, iic });
		pc.push_back({ iic, false, ii });
		pc.push_back({ ii, true, iic });
		pc.push_back({ ii, false, ii });
	}

	///////////////////////////////// pointFromScalar
	// XXX: only compressed point fixtures, flip for each combination when testing

	std::vector<PFS> pfs = {
		{ ONE, G_ONE }, // #L3153, #L3692, OK, > 0
		{ TWO, G_TWO },
		{ THREE, G_THREE },
		{ GROUP_ORDER_LESS_1, G_LESS_1 }, // #L3171, #L3710, OK == G - 1
		{ GROUP_ORDER_LESS_2, G_LESS_2 },
		{ GROUP_ORDER_LESS_3, G_LESS_3 }
	};

	// ref https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c#L2160
	test_ec_combine<A>(pa, pas, pfs);

	return std::make_tuple(ip, pa, pas, pc, pfs);
}

auto generateBad () {
	using A = uint8_t_33;

	bool ok = true;
	const auto G_ONE = _pointFromUInt32<A>(1, ok);
	const auto BAD_POINTS_C = generateBadPoints<uint8_t_33>();
	const auto BAD_POINTS = generateBadPoints<uint8_t_65>();
	assert(ok);

	std::vector<PA> pa;
	for (const auto x : BAD_POINTS) {
		pa.push_back({ x.a, G_ONE, {}, THROW_BAD_POINT, x.desc });
		pa.push_back({ G_ONE, x.a, {}, THROW_BAD_POINT, x.desc });
	}

	for (const auto x : BAD_POINTS_C) {
		pa.push_back({ x.a, G_ONE, {}, THROW_BAD_POINT, x.desc });
		pa.push_back({ G_ONE, x.a, {}, THROW_BAD_POINT, x.desc });
	}

	std::vector<PAS> pas;
	for (const auto x : BAD_POINTS) pas.push_back({ x.a, ONE, {}, THROW_BAD_POINT, x.desc });
	for (const auto x : BAD_POINTS_C) pas.push_back({ x.a, ONE, {}, THROW_BAD_POINT, x.desc });
	for (const auto x : BAD_TWEAKS) pas.push_back({ G_ONE, x.a, {}, THROW_BAD_TWEAK, x.desc });

	std::vector<PC> pc;
	for (const auto x : BAD_POINTS) pc.push_back({ x.a, true, {}, THROW_BAD_POINT, x.desc });
	for (const auto x : BAD_POINTS_C) pc.push_back({ x.a, true, {}, THROW_BAD_POINT, x.desc });

	std::vector<PFS> pfs;
	for (const auto x : BAD_PRIVATES) pfs.push_back({ x.a, {}, THROW_BAD_PRIVATE, x.desc });

	return std::make_tuple(pa, pas, pc, pfs);
}

template <typename A, typename B>
void dumpJSON (
	std::ostream& o,
	const A& good,
	const B& bad
) {
	const auto jIP = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("expected", jsonify(x.e))
		});
	};
	const auto jPA = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("Q", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except)),
		});
	};
	const auto jPAS = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("d", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except))
		});
	};
	const auto jPC = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("compress", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except)),
		});
	};
	const auto jPFS = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("d", jsonify(x.a)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except)),
		});
	};

	o << jsonifyO({
		jsonp("valid", jsonifyO({
			jsonp("isPoint", jsonifyA(std::get<0>(good), jIP)),
			jsonp("pointAdd", jsonifyA(std::get<1>(good), jPA)),
			jsonp("pointAddScalar", jsonifyA(std::get<2>(good), jPAS)),
			jsonp("pointCompress", jsonifyA(std::get<3>(good), jPC)),
			jsonp("pointFromScalar", jsonifyA(std::get<4>(good), jPFS)),
		})),
		jsonp("invalid", jsonifyO({
			jsonp("pointAdd", jsonifyA(std::get<0>(bad), jPA)),
			jsonp("pointAddScalar", jsonifyA(std::get<1>(bad), jPAS)),
			jsonp("pointCompress", jsonifyA(std::get<2>(bad), jPC)),
			jsonp("pointFromScalar", jsonifyA(std::get<3>(bad), jPFS)),
		}))
	});
}

int main () {
	_ec_init();
	const auto a = generate();
	const auto b = generateBad();
	dumpJSON(std::cout, a, b);

	return 0;
}
