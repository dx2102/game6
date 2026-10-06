#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"

#include <cmath>
#include <cstdio>

//visible area in table units:
static constexpr float ViewW = 880.0f;
static constexpr float ViewH = 480.0f;
static constexpr float TableW = float(Billiards::TableW / Billiards::Sub);
static constexpr float TableH = float(Billiards::TableH / Billiards::Sub);

static constexpr float FullPowerDistance = 200.0f;

static constexpr float RewindSpeed = 3.0f;

static char const *ReplayFile = "last-replay.txt";

PlayMode::PlayMode() {
	game.reset();
}

PlayMode::~PlayMode() {
}

glm::vec2 PlayMode::table_to_clip_scale(glm::uvec2 const &size) const {
	float px_per_unit = std::min(size.x / ViewW, size.y / ViewH);
	return glm::vec2(2.0f * px_per_unit / size.x, 2.0f * px_per_unit / size.y);
}

glm::vec2 PlayMode::aim_shot() const {
	Billiards::Ball const &cue = game.state.balls[0];
	glm::vec2 cue_pos = glm::vec2(cue.x, cue.y) / float(Billiards::Sub);
	glm::vec2 d = mouse - cue_pos;
	float len = glm::length(d);
	if (len < 1.0f) return glm::vec2(0.0f);
	float power = std::min(len / FullPowerDistance, 1.0f);
	return d / len * power * float(Billiards::MaxShot);
}

void PlayMode::take_shot(int64_t vx, int64_t vy) {
	history.emplace_back(game.state);
	game.shoot(vx, vy);
	shot_log.emplace_back(vx, vy);
	Billiards::save_shots(ReplayFile, shot_log);
	was_at_rest = false;
}

void PlayMode::print_status() const {
	std::printf("shots=%u left=%u steps=%u hash=%016llx\n",
		game.state.shots, game.targets_left(), game.state.steps, (unsigned long long)game.hash());
	std::fflush(stdout);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_Z) {
			rewinding = true;
			queued.clear();
			return true;
		} else if (evt.key.key == SDLK_R) {
			queued.clear();
			game.reset();
			history.clear();
			shot_log.clear();
			was_at_rest = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_Z) {
			rewinding = false;
			Billiards::save_shots(ReplayFile, shot_log);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		glm::vec2 clip = glm::vec2(
			2.0f * evt.motion.x / float(window_size.x) - 1.0f,
			1.0f - 2.0f * evt.motion.y / float(window_size.y)
		);
		mouse = clip / table_to_clip_scale(window_size) + glm::vec2(TableW, TableH) * 0.5f;
		return true;
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (evt.button.button == SDL_BUTTON_LEFT && !rewinding && queued.empty() && game.at_rest() && !game.state.balls[0].sunk) {
			glm::vec2 shot = aim_shot();
			int64_t vx = std::lround(shot.x);
			int64_t vy = std::lround(shot.y);
			if (vx != 0 || vy != 0) take_shot(vx, vy);
			return true;
		}
	}
	return false;
}

void PlayMode::update(float elapsed) {
	if (rewinding) {
		step_timer += elapsed * RewindSpeed;
		while (step_timer >= Billiards::StepTime && !history.empty()) {
			step_timer -= Billiards::StepTime;
			game.state = history.back();
			history.pop_back();
		}
		if (history.empty()) step_timer = 0.0f;
		if (shot_log.size() > game.state.shots) shot_log.resize(game.state.shots);
		was_at_rest = game.at_rest();
		return;
	}

	//no steps while at rest, so the result depends only on the shots:
	if (game.at_rest()) {
		step_timer = 0.0f;
		if (!queued.empty()) {
			replay_wait += elapsed;
			if (replay_wait > 1.0f) {
				replay_wait = 0.0f;
				take_shot(queued.front().first, queued.front().second);
				queued.pop_front();
			}
		}
		return;
	}

	step_timer += elapsed;
	while (step_timer >= Billiards::StepTime && !game.at_rest()) {
		step_timer -= Billiards::StepTime;
		history.emplace_back(game.state);
		game.step();
	}

	if (game.at_rest() && !was_at_rest) print_status();
	was_at_rest = game.at_rest();
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(0.12f, 0.06f, 0.03f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glm::vec2 s = table_to_clip_scale(drawable_size);
	glm::mat4 clip_from_table = glm::mat4(
		s.x, 0.0f, 0.0f, 0.0f,
		0.0f, s.y, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-TableW * 0.5f * s.x, -TableH * 0.5f * s.y, 0.0f, 1.0f
	);

	{
		DrawLines lines(clip_from_table);

		auto disk = [&](glm::vec2 c, float r, glm::u8vec4 color) {
			constexpr int N = 32;
			for (int i = 0; i < N; ++i) {
				float a0 = float(i) / N * 2.0f * float(M_PI);
				float a1 = float(i + 1) / N * 2.0f * float(M_PI);
				lines.draw_triangle(glm::vec3(c, 0.0f),
					glm::vec3(c + r * glm::vec2(std::cos(a0), std::sin(a0)), 0.0f),
					glm::vec3(c + r * glm::vec2(std::cos(a1), std::sin(a1)), 0.0f), color);
			}
		};

		glm::u8vec4 felt(0x10, 0x60, 0x30, 0xff);
		lines.draw_triangle(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(TableW, 0.0f, 0.0f), glm::vec3(TableW, TableH, 0.0f), felt);
		lines.draw_triangle(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(TableW, TableH, 0.0f), glm::vec3(0.0f, TableH, 0.0f), felt);

		float pr = float(Billiards::PocketR) / Billiards::Sub;
		for (float x : {0.0f, TableW * 0.5f, TableW}) {
			for (float y : {0.0f, TableH}) {
				disk(glm::vec2(x, y), pr, glm::u8vec4(0x00, 0x00, 0x00, 0xff));
			}
		}

		for (Billiards::Ball const &b : game.state.balls) {
			if (b.sunk) continue;
			glm::vec2 c = glm::vec2(b.x, b.y) / float(Billiards::Sub);
			float r = float(b.r) / Billiards::Sub;
			glm::u8vec4 color;
			if (b.kind == Billiards::Cue) color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
			else if (b.kind == Billiards::Post) color = glm::u8vec4(0x60, 0x60, 0x60, 0xff);
			else if (b.mass <= 1) color = glm::u8vec4(0xff, 0x50, 0x40, 0xff);
			else if (b.mass <= 2) color = glm::u8vec4(0xc0, 0x18, 0x10, 0xff);
			else color = glm::u8vec4(0x70, 0x08, 0x05, 0xff); //heavier = darker
			disk(c, r, color);
		}

		Billiards::Ball const &cue = game.state.balls[0];
		if (game.at_rest() && !rewinding && !cue.sunk && game.targets_left() > 0) {
			glm::vec2 c = glm::vec2(cue.x, cue.y) / float(Billiards::Sub);
			glm::vec2 shot = aim_shot();
			if (!queued.empty()) shot = glm::vec2(queued.front().first, queued.front().second);
			shot /= float(Billiards::MaxShot);
			lines.draw(glm::vec3(c, 0.0f), glm::vec3(c + shot * FullPowerDistance, 0.0f), glm::u8vec4(0xff, 0xff, 0x80, 0xff));
		}
	}

	{
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		constexpr float H = 0.07f;
		auto text = [&](std::string const &str, float x, float y) {
			lines.draw_text(str, glm::vec3(x, y, 0.0f), glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		std::string status = "Shots: " + std::to_string(game.state.shots)
			+ "   Red balls left: " + std::to_string(game.targets_left());
		if (game.targets_left() == 0) status = "Cleared in " + std::to_string(game.state.shots) + " shots! R to play again.";
		if (rewinding) status += "   << REWIND";
		if (!queued.empty()) status += "   REPLAY (Z or R to stop)";
		text(status, -aspect + 0.1f * H, 1.0f - 1.2f * H);
		text("Mouse aims (farther = harder), click shoots, hold Z to rewind, R restarts",
			-aspect + 0.1f * H, -1.0f + 0.3f * H);
	}
	GL_ERRORS();
}
