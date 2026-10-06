#pragma once

//Physics uses only integers, so it gives the same result on every platform.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct Billiards {
	//256 sub-units per table unit:
	static constexpr int64_t Sub = 256;
	static constexpr int64_t TableW = 800 * Sub;
	static constexpr int64_t TableH = 400 * Sub;
	static constexpr int64_t PocketR = 32 * Sub;

	static constexpr int StepsPerSecond = 240;
	static constexpr float StepTime = 1.0f / float(StepsPerSecond);

	enum Kind : uint8_t { Cue = 0, Target = 1, Post = 2 };

	struct Ball {
		int64_t x = 0, y = 0;
		int64_t vx = 0, vy = 0; //sub-units per step
		int64_t r = 0;
		int64_t mass = 1;
		Kind kind = Target;
		bool sunk = false;
	};

	struct State {
		std::vector< Ball > balls;
		uint32_t shots = 0;
		uint32_t steps = 0;
	};

	State state;

	void reset();
	bool at_rest() const;

	static constexpr int64_t MaxShot = 6 * Sub;
	void shoot(int64_t vx, int64_t vy);

	void step();

	uint32_t targets_left() const;
	uint64_t hash() const;

	//shot files have one "shot vx vy" per line:
	typedef std::vector< std::pair< int64_t, int64_t > > Shots;
	static Shots load_shots(std::string const &filename);
	static void save_shots(std::string const &filename, Shots const &shots);
};
