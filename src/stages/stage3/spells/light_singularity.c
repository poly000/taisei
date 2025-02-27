/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2024, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2024, Andrei Alexeyev <akari@taisei-project.org>.
 */

#include "spells.h"

TASK(singularity_laser, { cmplx pos; cmplx vel; real amp; real freq; }) {
	Laser *l = TASK_BIND(create_laser(
		ARGS.pos, 200, 10000, RGBA(0.0, 0.2, 1.0, 0.0),
		laser_rule_sine_expanding(ARGS.vel, ARGS.amp, ARGS.freq, 0)
	));

	laser_make_static(l);
	l->unclearable = true;

	real spin_factor = difficulty_value(1.05, 1.4, 1.75, 2.1);
	cmplx r = cdir(M_PI/500.0 * spin_factor);

	auto rd = NOT_NULL(laser_get_ruledata_sine_expanding(l));

	for(int t = 0;; ++t, YIELD) {
		if(t == 140) {
			play_sfx("laser1");
		}

		laser_charge(l, t, 150, 10 + 10 * psin(re(rd->velocity) + t / 60.0));
		rd->phase = t / 10.0;
		rd->velocity *= r;

		l->color = *HSLA((carg(rd->velocity) + M_PI) / (M_PI * 2), 1.0, 0.5, 0.0);
	}
}

DEFINE_EXTERN_TASK(stage3_spell_light_singularity) {
	Boss *boss = INIT_BOSS_ATTACK(&ARGS);
	boss->move = move_from_towards(boss->pos, VIEWPORT_W/2 + VIEWPORT_H*I/2, 0.05);
	BEGIN_BOSS_ATTACK(&ARGS);

	WAIT(100);
	aniplayer_queue(&boss->ani, "specialshot_charge", 1);
	aniplayer_queue(&boss->ani, "specialshot_release", 1);
	aniplayer_queue(&boss->ani, "main", 0);
	WAIT(20);

	int nlasers = difficulty_value(3, 4, 5, 6);
	int nbullets = difficulty_value(8, 10, 12, 14);
	real laser_angle_ofs = difficulty_value(0.7, 0, 0.7, 0);
	real bullet_speed = difficulty_value(1, 0.8, 0.6, 0.4);

	ProjPrototype *ptypes[] = {
		pp_thickrice,
		pp_rice,
		pp_bullet,
		pp_wave,
		pp_ball,
		pp_plainball,
		pp_bigball,
		pp_soul,
	};

	int bullets_cycle = 0;

	for(int i = 0; i < nlasers; ++i) {
		INVOKE_TASK(singularity_laser,
			.pos = boss->pos,
			.vel = 2 * cdir(laser_angle_ofs + M_PI / 4 + M_PI * 2 * i / (real)nlasers),
			.amp = (4.0 / nlasers) * (M_PI / 5),
			.freq = 0.05
		);
	}

	for(;;YIELD) {
		WAIT(270);
		aniplayer_queue(&boss->ani, "specialshot_charge", 1);
		aniplayer_queue(&boss->ani, "specialshot_release", 1);
		aniplayer_queue(&boss->ani, "main", 0);
		WAIT(30);

		ProjPrototype *ptype = ptypes[bullets_cycle];

		if(bullets_cycle < ARRAY_SIZE(ptypes) - 1) {
			++bullets_cycle;
		}

		real colorofs = rng_real();

		for(int i = 0; i < nbullets; ++i) {
			real f = i / (real)nbullets;
			cmplx dir = cdir(M_TAU * f);

			ProjFlags flags = 0;
			real angle = 0;
			real angle_delta = 0;

			if(ptype == pp_soul) {
				flags |= PFLAG_MANUALANGLE,
				angle = rng_angle();
				angle_delta = -M_PI/93;
			}

			PROJECTILE(
				.proto = ptype,
				.pos = boss->pos,
				.color = HSLA(f + colorofs, 1.0, 0.5, 0),
				.move = move_asymptotic_simple(dir * bullet_speed, 20),
				.flags = flags,
				.angle = angle,
				.angle_delta = angle_delta,
			);
		}

		play_sfx("shot_special1");
	}
}
