#include <assert.h>
#include <string.h>
#include <wayland-server-protocol.h>
#include <wlr/util/box.h>
#include "util/matrix.h"



const float wlr_matrix_transforms[8][9] = {
	[WL_OUTPUT_TRANSFORM_NORMAL] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_90] = {
		0.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_180] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_270] = {
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_90] = {
		0.0f, 1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_180] = {
		1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_270] = {
		0.0f, -1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
};

void wlr_matrix_transform(float mat[static 9],
		enum wl_output_transform transform) {
	// NOTE: This optimized multiplication assumes that for all 8 standard
	// wl_output_transform matrices, the third column is [0, 0, 1]^T
	// (i.e. t[2]=t[5]=t[6]=t[7]=0 and t[8]=1). If non-standard transforms are
	// introduced where the third column is not [0, 0, 1]^T, this will produce
	// incorrect results.
	const float *t = wlr_matrix_transforms[transform];

	float m0 = mat[0], m1 = mat[1];
	mat[0] = m0 * t[0] + m1 * t[3];
	mat[1] = m0 * t[1] + m1 * t[4];

	float m3 = mat[3], m4 = mat[4];
	mat[3] = m3 * t[0] + m4 * t[3];
	mat[4] = m3 * t[1] + m4 * t[4];

	float m6 = mat[6], m7 = mat[7];
	mat[6] = m6 * t[0] + m7 * t[3];
	mat[7] = m6 * t[1] + m7 * t[4];
}

void matrix_projection(float mat[static 9], int width, int height,
		enum wl_output_transform transform) {
	memset(mat, 0, sizeof(*mat) * 9);

	const float *t = wlr_matrix_transforms[transform];
	float x = 2.0f / width;
	float y = 2.0f / height;

	// Rotation + reflection
	mat[0] = x * t[0];
	mat[1] = x * t[1];
	mat[3] = y * -t[3];
	mat[4] = y * -t[4];

	// Translation
	mat[2] = -copysign(1.0f, mat[0] + mat[1]);
	mat[5] = -copysign(1.0f, mat[3] + mat[4]);

	// Identity
	mat[8] = 1.0f;
}

void wlr_matrix_project_box(float mat[static 9], const struct wlr_box *box,
		enum wl_output_transform transform, const float projection[static 9]) {
	float x = box->x;
	float y = box->y;
	float width = box->width;
	float height = box->height;

	const float *t = wlr_matrix_transforms[transform];
	float m[9] = {
		width * t[0], width * t[1], width * 0.5f * (1.0f - t[0] - t[1]) + x,
		height * t[3], height * t[4], height * 0.5f * (1.0f - t[3] - t[4]) + y,
		0.0f, 0.0f, 1.0f,
	};

	wlr_matrix_multiply(mat, projection, m);
}

void matrix_invert(float out[static 9], float m[static 9]) {
	float a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];

	// See: https://en.wikipedia.org/wiki/Determinant
	float det = a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h;
	assert(det != 0);
	float inv_det = 1 / det;

	// See: https://en.wikipedia.org/wiki/Invertible_matrix#Inversion_of_3_%C3%97_3_matrices
	float result[] = {
		inv_det * (e*i - f*h),
		inv_det * -(b*i - c*h),
		inv_det * (b*f - c*e),
		inv_det * -(d*i - f*g),
		inv_det * (a*i - c*g),
		inv_det * -(a*f - c*d),
		inv_det * (d*h - e*g),
		inv_det * -(a*h - b*g),
		inv_det * (a*e - b*d),
	};
	memcpy(out, result, sizeof(result));
}
