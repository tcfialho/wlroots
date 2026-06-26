#ifndef UTIL_MATRIX_H
#define UTIL_MATRIX_H

#include <wayland-server-protocol.h>

struct wlr_box;

/** Writes the identity matrix into mat */
static inline void wlr_matrix_identity(float mat[static 9]) {
	mat[0] = 1.0f; mat[1] = 0.0f; mat[2] = 0.0f;
	mat[3] = 0.0f; mat[4] = 1.0f; mat[5] = 0.0f;
	mat[6] = 0.0f; mat[7] = 0.0f; mat[8] = 1.0f;
}

static inline void wlr_matrix_multiply(float mat[static 9], const float a[static 9],
		const float b[static 9]) {
	float product[9];

	product[0] = a[0]*b[0] + a[1]*b[3] + a[2]*b[6];
	product[1] = a[0]*b[1] + a[1]*b[4] + a[2]*b[7];
	product[2] = a[0]*b[2] + a[1]*b[5] + a[2]*b[8];

	product[3] = a[3]*b[0] + a[4]*b[3] + a[5]*b[6];
	product[4] = a[3]*b[1] + a[4]*b[4] + a[5]*b[7];
	product[5] = a[3]*b[2] + a[4]*b[5] + a[5]*b[8];

	product[6] = a[6]*b[0] + a[7]*b[3] + a[8]*b[6];
	product[7] = a[6]*b[1] + a[7]*b[4] + a[8]*b[7];
	product[8] = a[6]*b[2] + a[7]*b[5] + a[8]*b[8];

	mat[0] = product[0];
	mat[1] = product[1];
	mat[2] = product[2];
	mat[3] = product[3];
	mat[4] = product[4];
	mat[5] = product[5];
	mat[6] = product[6];
	mat[7] = product[7];
	mat[8] = product[8];
}

static inline void wlr_matrix_translate(float mat[static 9], float x, float y) {
	mat[2] = mat[0] * x + mat[1] * y + mat[2];
	mat[5] = mat[3] * x + mat[4] * y + mat[5];
	mat[8] = mat[6] * x + mat[7] * y + mat[8];
}

static inline void wlr_matrix_scale(float mat[static 9], float x, float y) {
	mat[0] *= x;
	mat[3] *= x;
	mat[6] *= x;
	mat[1] *= y;
	mat[4] *= y;
	mat[7] *= y;
}

void wlr_matrix_transform(float mat[static 9],
	enum wl_output_transform transform);

/** Shortcut for the various matrix operations involved in projecting the
 *  specified wlr_box onto a given orthographic projection with a given
 *  rotation. The result is written to mat, which can be applied to each
 *  coordinate of the box to get a new coordinate from [-1,1]. */
void wlr_matrix_project_box(float mat[static 9], const struct wlr_box *box,
	enum wl_output_transform transform, const float projection[static 9]);

/**
 * Writes a 2D orthographic projection matrix to mat of (width, height) with a
 * specified wl_output_transform.
 *
 * Equivalent to glOrtho(0, width, 0, height, 1, -1) with the transform applied.
 */
void matrix_projection(float mat[static 9], int width, int height,
	enum wl_output_transform transform);

/**
 * Compute the inverse of a matrix.
 *
 * The matrix needs to be inversible.
 */
void matrix_invert(float out[static 9], float m[static 9]);

extern const float wlr_matrix_transforms[8][9];

#endif
