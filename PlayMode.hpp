#include "Mode.hpp"

#include "Billiards.hpp"

#include <glm/glm.hpp>

#include <deque>
#include <vector>
#include <utility>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	Billiards game;

	//states for rewinding:
	std::vector< Billiards::State > history;
	Billiards::Shots shot_log;

	//shots waiting to be played by --watch:
	std::deque< std::pair< int64_t, int64_t > > queued;
	float replay_wait = 0.0f;

	bool rewinding = false;
	float step_timer = 0.0f;
	bool was_at_rest = true;

	glm::vec2 mouse = glm::vec2(0.0f);

	glm::vec2 table_to_clip_scale(glm::uvec2 const &size) const;
	glm::vec2 aim_shot() const;

	void take_shot(int64_t vx, int64_t vy);
	void print_status() const;
};
