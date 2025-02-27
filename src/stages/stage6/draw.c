/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2024, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2024, Andrei Alexeyev <akari@taisei-project.org>.
 */

#include "draw.h"

#include "global.h"
#include "resource/model.h"
#include "stagedraw.h"
#include "util/glm.h"
#include "util/graphics.h"

static Stage6DrawData *stage6_draw_data;

Stage6DrawData *stage6_get_draw_data(void) {
	return NOT_NULL(stage6_draw_data);
}

void stage6_drawsys_init(void) {
	stage6_draw_data = ALLOC(typeof(*stage6_draw_data));
	// TODO: make this background slightly less horribly inefficient
	stage3d_init(&stage_3d_context, 128);

	for(int i = 0; i < NUM_STARS; i++) {
		vec3 p = { 0 };

		for(int c = 0; c < 10; c++) {
			p[0] += rng_f32s();
			p[1] += rng_f32s();
			p[2] += rng_f32s();
		} // now x,y,z are approximately gaussian

		// normalize them and it’s evenly distributed on a sphere
		glm_vec3_normalize(p);
		glm_vec3_copy(p, stage6_draw_data->stars.position[i]);
	}

	FBAttachmentConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.attachment = FRAMEBUFFER_ATTACH_COLOR0;
	cfg.tex_params.type = TEX_TYPE_RGBA;
	cfg.tex_params.filter.min = TEX_FILTER_LINEAR;
	cfg.tex_params.filter.mag = TEX_FILTER_LINEAR;
	cfg.tex_params.wrap.s = TEX_WRAP_MIRROR;
	cfg.tex_params.wrap.t = TEX_WRAP_MIRROR;
	stage6_draw_data->baryon.fbpair.front = stage_add_background_framebuffer("Baryon FB 1", 0.25, 0.5, 1, &cfg);
	stage6_draw_data->baryon.fbpair.back = stage_add_background_framebuffer("Baryon FB 2", 0.25, 0.5, 1, &cfg);
	stage6_draw_data->baryon.aux_fb = stage_add_background_framebuffer("Baryon FB AUX", 0.25, 0.5, 1, &cfg);

	stage6_draw_data->envmap = res_texture("stage6/sky");
	stage6_draw_data->models.calabi_yau_quintic = res_model("stage6/calabi-yau-quintic");

	pbr_load_model(&stage6_draw_data->models.rim,          "stage6/rim",          "stage6/rim");
	pbr_load_model(&stage6_draw_data->models.spires,       "stage6/spires",       "stage6/spires");
	pbr_load_model(&stage6_draw_data->models.stairs,       "stage6/stairs",       "stage6/stairs");
	pbr_load_model(&stage6_draw_data->models.tower,        "stage6/tower",        "stage6/tower");
	pbr_load_model(&stage6_draw_data->models.tower_bottom, "stage6/tower_bottom", "stage6/tower_bottom");
	pbr_load_model(&stage6_draw_data->models.floor,        "stage6/floor",        "stage6/floor");
}

void stage6_drawsys_shutdown(void) {
	stage3d_shutdown(&stage_3d_context);
	mem_free(stage6_draw_data);
	stage6_draw_data = NULL;
}

static void stage6_bg_setup_pbr_lighting(Camera3D *cam) {
	PointLight3D lights[] = {
		{ { 0, 10, 100 }, { 10000, 10000, 10000 } },
	};

	camera3d_set_point_light_uniforms(cam, ARRAY_SIZE(lights), lights);
	r_uniform_vec3("ambient_color", 1, 1, 1);
}

static void stage6_bg_setup_pbr_env(Camera3D *cam, PBREnvironment *env) {
	stage6_bg_setup_pbr_lighting(cam);
	glm_vec3_broadcast(1.0f, env->ambient_color);
	glm_vec3_broadcast(1.0f, env->environment_color);
	env->environment_map = stage6_draw_data->envmap;
	camera3d_apply_inverse_transforms(cam, env->cam_inverse_transform);
}

static uint stage6_towertop_pos(Stage3D *s3d, vec3 cam, float maxrange) {
	vec3 origin = {0, 0, 0};
	return stage3d_pos_single(s3d, cam, origin, maxrange);
}

static void stage6_towertop_draw(vec3 pos) {
	r_state_push();
	r_mat_mv_push();
	r_mat_mv_translate_v(pos);

	r_shader("pbr");

	PBREnvironment env = { 0 };
	stage6_bg_setup_pbr_env(&stage_3d_context.cam, &env);

	pbr_draw_model(&stage6_draw_data->models.stairs, &env);
	pbr_draw_model(&stage6_draw_data->models.tower, &env);
	// TODO optimize: draw tower bottom only after falling off the tower
	pbr_draw_model(&stage6_draw_data->models.tower_bottom, &env);
	pbr_draw_model(&stage6_draw_data->models.rim, &env);
	pbr_draw_model(&stage6_draw_data->models.spires, &env);
	pbr_draw_model(&stage6_draw_data->models.floor, &env);

	r_disable(RCAP_CULL_FACE);
	r_disable(RCAP_DEPTH_WRITE);
	r_mat_mv_translate(0, 0, 7);
	r_mat_mv_scale(0.7, 0.7, 0.7);
	//r_mat_mv_translate(stage_3d_context.cam.pos[0], stage_3d_context.cam.pos[1], stage_3d_context.cam.pos[2]);
	r_shader("calabi-yau-quintic");
	//r_mat_mv_rotate(-global.frames*0.03, 0, 0, 1);
	r_mat_mv_rotate(global.frames*0.01, 0, 1, 0);
	r_uniform_float("alpha", global.frames*0.03);
	r_draw_model_ptr(stage6_draw_data->models.calabi_yau_quintic, 0, 0);
	r_mat_mv_pop();
	r_state_pop();
}

static uint stage6_skysphere_pos(Stage3D *s3d, vec3 cam, float maxrange) {
	return stage3d_pos_single(s3d, cam, cam, maxrange);
}

static void stage6_skysphere_draw(vec3 pos) {
	r_state_push();

	r_disable(RCAP_DEPTH_TEST);
	r_cull(CULL_FRONT);
	r_shader("stage6_sky");
	r_uniform_sampler("skybox", "stage6/sky");

	r_mat_mv_push();
	r_mat_mv_translate_v(stage_3d_context.cam.pos);
	r_mat_mv_scale(50, 50, 50);
	r_draw_model("cube");

	r_shader("sprite_default");

	for(int i = 0; i < NUM_STARS; i++) {
		vec3 p;
		glm_vec3_copy(stage6_draw_data->stars.position[i], p);
		vec3 axis = { 1, 0, 0.2 };
		glm_vec3_rotate(p, global.frames*0.001, axis);
		if(p[2] < 0) {
			continue;
		}

		r_mat_mv_push();
		r_mat_mv_translate_v(p);
		r_mat_mv_rotate(acos(p[2]), -p[1], p[0], 0);
		r_mat_mv_scale(3e-4, 3e-4, 3e-4);
		r_draw_sprite(&(SpriteParams) {
			.sprite = "part/smoothdot",
			.color = RGBA_MUL_ALPHA(1, 1, 1, sqrt(p[2])),
		});
		r_mat_mv_pop();

	}

	r_mat_mv_pop();
	r_state_pop();
}

void stage6_draw(void) {
	Stage3DSegment segs[] = {
		/*
		 * TODO: Render the skybox last, so that most of it can be
		 * rejected by the depth test. This doesn't work right now
		 * for some reason.
		 */
		{ stage6_skysphere_draw, stage6_skysphere_pos },
		{ stage6_towertop_draw, stage6_towertop_pos },
	};

	stage3d_draw(&stage_3d_context, 100, ARRAY_SIZE(segs), segs);
}

void elly_spellbg_classic(Boss *b, int t) {
	fill_viewport(0,0,0.7,"stage6/spellbg_classic");
	r_blend(BLEND_MOD);
	r_color4(1, 1, 1, 0);
	fill_viewport(0,-t*0.005,0.7,"stage6/spellbg_chalk");
	r_blend(BLEND_PREMUL_ALPHA);
	r_color4(1, 1, 1, 1);
}

void elly_spellbg_modern(Boss *b, int t) {
	fill_viewport(0,0,0.6,"stage6/spellbg_modern");
	r_blend(BLEND_MOD);
	r_color4(1, 1, 1, 0);
	fill_viewport(0,-t*0.005,0.7,"stage6/spellbg_chalk");
	r_blend(BLEND_PREMUL_ALPHA);
	r_color4(1, 1, 1, 1);
}

void elly_spellbg_modern_dark(Boss *b, int t) {
	elly_spellbg_modern(b, t);
	fade_out(0.75f * min(1.0f, t / 300.0f));
}

static bool stage6_fog(Framebuffer *fb) {
	r_shader("zbuf_fog");
	r_uniform_sampler("depth", r_framebuffer_get_attachment(fb, FRAMEBUFFER_ATTACH_DEPTH));
	r_uniform_vec4_rgba("fog_color", RGB(0.1,0.3,0.8));
	r_uniform_float("start", 0.2);
	r_uniform_float("end", 5);
	r_uniform_float("exponent", 3.0);
	r_uniform_float("curvature", 0);
	draw_framebuffer_tex(fb, VIEWPORT_W, VIEWPORT_H);
	r_shader_standard();
	return true;
}

ShaderRule stage6_bg_effects[] = {
	stage6_fog,
	NULL
};
