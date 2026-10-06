#include "Billiards.hpp"

#include <fstream>
#include <sstream>

static int64_t isqrt(int64_t v) {
	if (v <= 0) return 0;
	uint64_t x = uint64_t(v);
	uint64_t res = 0;
	uint64_t bit = uint64_t(1) << 62;
	while (bit > x) bit >>= 2;
	while (bit != 0) {
		if (x >= res + bit) {
			x -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
	}
	return int64_t(res);
}

//unit vectors are scaled by this:
static constexpr int64_t NormalScale = 4096;

static constexpr int64_t Friction = 2;
static constexpr int64_t WallBounceNum = 9, WallBounceDen = 10;
static constexpr int64_t BallBounceNum = 39, BallBounceDen = 20; //1 + 0.95

static constexpr int64_t CueStartX = 200 * Billiards::Sub;
static constexpr int64_t CueStartY = 200 * Billiards::Sub;

void Billiards::reset() {
	state = State();
	auto add = [this](int64_t x, int64_t y, int64_t r, int64_t mass, Kind kind) {
		Ball b;
		b.x = x * Sub;
		b.y = y * Sub;
		b.r = r * Sub;
		b.mass = mass;
		b.kind = kind;
		state.balls.emplace_back(b);
	};
	add(200, 200, 12, 2, Cue);

	//bigger balls are heavier:
	add(560, 200, 12, 2, Target);
	add(585, 186, 12, 2, Target);
	add(585, 214, 12, 2, Target);
	add(612, 172, 16, 6, Target);
	add(612, 200,  9, 1, Target);
	add(612, 228, 16, 6, Target);
	add(700,  80,  9, 1, Target);
	add(700, 320,  9, 1, Target);

	//posts never move:
	add(400, 110, 14, 0, Post);
	add(400, 290, 14, 0, Post);
	add(700, 200, 14, 0, Post);
}

bool Billiards::at_rest() const {
	for (Ball const &b : state.balls) {
		if (b.sunk || b.kind == Post) continue;
		if (b.vx != 0 || b.vy != 0) return false;
	}
	return true;
}

void Billiards::shoot(int64_t vx, int64_t vy) {
	Ball &cue = state.balls[0];
	if (!at_rest() || cue.sunk) return;
	int64_t len2 = vx * vx + vy * vy;
	if (len2 > MaxShot * MaxShot) {
		int64_t len = isqrt(len2);
		vx = vx * MaxShot / len;
		vy = vy * MaxShot / len;
	}
	cue.vx = vx;
	cue.vy = vy;
	state.shots += 1;
}

void Billiards::step() {
	state.steps += 1;

	static const int64_t pockets[6][2] = {
		{0, 0}, {TableW / 2, 0}, {TableW, 0},
		{0, TableH}, {TableW / 2, TableH}, {TableW, TableH},
	};

	for (Ball &b : state.balls) {
		if (b.sunk || b.kind == Post) continue;
		if (b.vx == 0 && b.vy == 0) continue;

		int64_t speed = isqrt(b.vx * b.vx + b.vy * b.vy);
		if (speed <= Friction) {
			b.vx = 0;
			b.vy = 0;
			continue;
		}
		b.vx = b.vx * (speed - Friction) / speed;
		b.vy = b.vy * (speed - Friction) / speed;

		b.x += b.vx;
		b.y += b.vy;

		for (auto const &p : pockets) {
			int64_t dx = b.x - p[0];
			int64_t dy = b.y - p[1];
			if (dx * dx + dy * dy < PocketR * PocketR) {
				b.sunk = true;
				b.vx = 0;
				b.vy = 0;
				break;
			}
		}
		if (b.sunk) continue;

		if (b.x < b.r) { b.x = 2 * b.r - b.x; b.vx = -b.vx * WallBounceNum / WallBounceDen; }
		if (b.x > TableW - b.r) { b.x = 2 * (TableW - b.r) - b.x; b.vx = -b.vx * WallBounceNum / WallBounceDen; }
		if (b.y < b.r) { b.y = 2 * b.r - b.y; b.vy = -b.vy * WallBounceNum / WallBounceDen; }
		if (b.y > TableH - b.r) { b.y = 2 * (TableH - b.r) - b.y; b.vy = -b.vy * WallBounceNum / WallBounceDen; }
	}

	for (size_t i = 0; i < state.balls.size(); ++i) {
		Ball &a = state.balls[i];
		if (a.sunk) continue;
		for (size_t j = i + 1; j < state.balls.size(); ++j) {
			Ball &b = state.balls[j];
			if (b.sunk) continue;
			if (a.kind == Post && b.kind == Post) continue;

			int64_t dx = b.x - a.x;
			int64_t dy = b.y - a.y;
			int64_t rr = a.r + b.r;
			int64_t d2 = dx * dx + dy * dy;
			if (d2 >= rr * rr) continue;

			int64_t dist = isqrt(d2);
			int64_t nx = NormalScale, ny = 0;
			if (dist > 0) {
				nx = dx * NormalScale / dist;
				ny = dy * NormalScale / dist;
			}
			int64_t overlap = rr - dist + 1;

			if (a.kind == Post || b.kind == Post) {
				Ball &m = (a.kind == Post ? b : a);
				int64_t sx = (a.kind == Post ? nx : -nx);
				int64_t sy = (a.kind == Post ? ny : -ny);
				int64_t vn = (m.vx * sx + m.vy * sy) / NormalScale;
				if (vn < 0) {
					m.vx -= 2 * vn * sx / NormalScale;
					m.vy -= 2 * vn * sy / NormalScale;
				}
				m.x += overlap * sx / NormalScale;
				m.y += overlap * sy / NormalScale;
				continue;
			}

			int64_t msum = a.mass + b.mass;
			int64_t vn = ((b.vx - a.vx) * nx + (b.vy - a.vy) * ny) / NormalScale;
			if (vn < 0) {
				int64_t ja = vn * BallBounceNum * b.mass / (BallBounceDen * msum);
				int64_t jb = vn * BallBounceNum * a.mass / (BallBounceDen * msum);
				a.vx += ja * nx / NormalScale;
				a.vy += ja * ny / NormalScale;
				b.vx -= jb * nx / NormalScale;
				b.vy -= jb * ny / NormalScale;
			}
			int64_t oa = overlap * b.mass / msum;
			int64_t ob = overlap * a.mass / msum;
			a.x -= oa * nx / NormalScale;
			a.y -= oa * ny / NormalScale;
			b.x += ob * nx / NormalScale;
			b.y += ob * ny / NormalScale;
		}
	}

	//cue ball comes back after everything stops:
	Ball &cue = state.balls[0];
	if (cue.sunk && at_rest()) {
		cue.sunk = false;
		cue.x = CueStartX;
		cue.y = CueStartY;
	}
}

uint32_t Billiards::targets_left() const {
	uint32_t count = 0;
	for (Ball const &b : state.balls) {
		if (b.kind == Target && !b.sunk) count += 1;
	}
	return count;
}

uint64_t Billiards::hash() const {
	//FNV-1a
	uint64_t h = 14695981039346656037ull;
	auto mix = [&h](uint64_t v) {
		for (int i = 0; i < 8; ++i) {
			h ^= (v >> (8 * i)) & 0xff;
			h *= 1099511628211ull;
		}
	};
	for (Ball const &b : state.balls) {
		mix(uint64_t(b.x));
		mix(uint64_t(b.y));
		mix(uint64_t(b.vx));
		mix(uint64_t(b.vy));
		mix(b.sunk ? 1 : 0);
	}
	mix(state.shots);
	mix(state.steps);
	return h;
}

Billiards::Shots Billiards::load_shots(std::string const &filename) {
	Shots shots;
	std::ifstream in(filename);
	std::string line;
	while (std::getline(in, line)) {
		std::istringstream ls(line);
		std::string word;
		long long vx, vy;
		if ((ls >> word >> vx >> vy) && word == "shot") shots.emplace_back(vx, vy);
	}
	return shots;
}

void Billiards::save_shots(std::string const &filename, Shots const &shots) {
	std::ofstream out(filename);
	for (auto const &s : shots) {
		out << "shot " << s.first << " " << s.second << "\n";
	}
}
